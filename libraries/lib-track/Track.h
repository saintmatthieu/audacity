/*!********************************************************************

  Audacity: A Digital Audio Editor

  @file Track.h
  @brief declares abstract base class Track, TrackList, and iterators over TrackList

  Dominic Mazzoni

**********************************************************************/
#ifndef __AUDACITY_TRACK__
#define __AUDACITY_TRACK__

#include <atomic>
#include <cassert>
#include <utility>
#include <vector>
#include <list>
#include <optional>
#include <functional>
#include <wx/longlong.h>

#include "ClientData.h"
#include "Observer.h"
// TrackAttachment needs to be a complete type for the Windows build, though
// not the others, so there is a nested include here:
#include "TrackAttachment.h"
#include "TypeEnumerator.h"
#include "TypeSwitch.h"
#include "XMLTagHandler.h"

#ifdef __WXMSW__
#pragma warning(disable:4284)
#endif

class wxTextFile;
class Track;
class ProjectSettings;
class AudacityProject;

using TrackArray = std::vector< Track* >;

class TrackList;
struct UndoStackElem;

using ListOfTracks = std::list< std::shared_ptr< Track > >;

//! Pairs a std::list iterator and a pointer to a list, for comparison purposes
/*! Compare owning lists first, and only if same, then the iterators;
 else MSVC debug runtime complains. */
using TrackNodePointer =
std::pair< ListOfTracks::iterator, ListOfTracks* >;

inline bool operator == (const TrackNodePointer &a, const TrackNodePointer &b)
{ return a.second == b.second && a.first == b.first; }

inline bool operator != (const TrackNodePointer &a, const TrackNodePointer &b)
{ return !(a == b); }

//! Empty class which will have subclasses
BEGIN_TYPE_ENUMERATION(TrackTypeTag)

//! This macro should be called immediately after the definition of each Track
//! subclass
/*! It must occur at file scope, not within any other namespace */
#define ENUMERATE_TRACK_TYPE(T) ENUMERATE_TYPE(TrackTypeTag, T)

// forward declarations, so we can make them friends
template<typename T>
   std::enable_if_t< std::is_pointer_v<T>, T >
      track_cast(Track *track);

template<typename T>
   std::enable_if_t<
      std::is_pointer_v<T> &&
         std::is_const_v< std::remove_pointer_t< T > >,
      T
   >
      track_cast(const Track *track);

//! An in-session identifier of track objects across undo states.  It does not persist between sessions
/*!
    Default constructed value is not equal to the id of any track that has ever
    been added to a TrackList, or (directly or transitively) copied from such.
    (A track added by TrackList::RegisterPendingNewTrack() that is not yet applied is not
    considered added.)

    TrackIds are assigned uniquely across projects. */
class TrackId
{
public:
   TrackId() : mValue(-1) {}
   explicit TrackId (long value) : mValue(value) {}

   bool operator == (const TrackId &other) const
   { return mValue == other.mValue; }

   bool operator != (const TrackId &other) const
   { return mValue != other.mValue; }

   // Define this in case you want to key a std::map on TrackId
   // The operator does not mean anything else
   bool operator <  (const TrackId &other) const
   { return mValue <  other.mValue; }

private:
   long mValue;
};

//! Optional extra information about an interval, appropriate to a subtype of Track
struct TRACK_API TrackIntervalData {
   virtual ~TrackIntervalData();
};

//! A start and an end time, and non-mutative access to optional extra information
/*! @invariant `Start() <= End()` */
class ConstTrackInterval {
public:

   /*! @pre `start <= end` */
   ConstTrackInterval( double start, double end,
      std::unique_ptr<TrackIntervalData> pExtra = {} )
   : start{ start }, end{ end }, pExtra{ std::move( pExtra ) }
   {
      wxASSERT( start <= end );
   }

   ConstTrackInterval( ConstTrackInterval&& ) = default;
   ConstTrackInterval &operator=( ConstTrackInterval&& ) = default;

   double Start() const { return start; }
   double End() const { return end; }
   const TrackIntervalData *Extra() const { return pExtra.get(); }

private:
   double start, end;
protected:
   // TODO C++17: use std::any instead
   std::unique_ptr< TrackIntervalData > pExtra;
};

//! A start and an end time, and mutative access to optional extra information
/*! @invariant `Start() <= End()` */
class TrackInterval : public ConstTrackInterval {
public:
   using ConstTrackInterval::ConstTrackInterval;

   TrackInterval(TrackInterval&&) = default;
   TrackInterval &operator= (TrackInterval&&) = default;

   TrackIntervalData *Extra() const { return pExtra.get(); }
};

//! Template generated base class for Track lets it host opaque UI related objects
using AttachedTrackObjects = ClientData::Site<
   Track, TrackAttachment, ClientData::ShallowCopying, std::shared_ptr
>;

//! Abstract base class for an object holding data associated with points on a time axis
class TRACK_API Track /* not final */
   : public XMLTagHandler
   , public AttachedTrackObjects
   , public std::enable_shared_from_this<Track> // see SharedPointer()
{
protected:
   //! Empty argument passed to some public constructors
   /*!
    Passed to function templates like make_shared, which don't need to be
    friends; but construction of the argument is controlled by the class
    */
   struct ProtectedCreationArg{};
public:

   //! For two tracks describes the type of the linkage
   enum class LinkType : int {
       None = 0, //< No linkage
       Group = 2, //< Tracks are grouped together
       Aligned, //< Tracks are grouped and changes should be synchronized
   };

   struct ChannelGroupData;

   //! Hosting of objects attached by higher level code
   using ChannelGroupAttachments = ClientData::Site<
      ChannelGroupData, ClientData::Cloneable<>, ClientData::DeepCopying
   >;

   // Structure describing data common to channels of a group of tracks
   // Should be deep-copyable (think twice before adding shared pointers!)
   struct TRACK_API ChannelGroupData : ChannelGroupAttachments {
      wxString mName;
      LinkType mLinkType{ LinkType::None };
      bool mSelected{ false };
   };

private:

   friend class TrackList;

 private:
   TrackId mId; //!< Identifies the track only in-session, not persistently

   std::unique_ptr<ChannelGroupData> mpGroupData;

 protected:
   std::weak_ptr<TrackList> mList; //!< Back pointer to owning TrackList
   //! Holds iterator to self, so that TrackList::Find can be constant-time
   /*! mNode's pointer to std::list might not be this TrackList, if it's a pending update track */
   TrackNodePointer mNode{};
   int            mIndex; //!< 0-based position of this track in its TrackList

 public:

   //! Alias for my base type
   using AttachedObjects = ::AttachedTrackObjects;

   TrackId GetId() const { return mId; }
 private:
   void SetId( TrackId id ) { mId = id; }
 public:

   // Given a bare pointer, find a shared_ptr.  Undefined results if the track
   // is not yet managed by a shared_ptr.  Undefined results if the track is
   // not really of the subclass.  (That is, trusts the caller and uses static
   // not dynamic casting.)
   template<typename Subclass = Track>
   inline std::shared_ptr<Subclass> SharedPointer()
   {
      // shared_from_this is injected into class scope by base class
      // std::enable_shared_from_this<Track>
      return std::static_pointer_cast<Subclass>( shared_from_this() );
   }

   template<typename Subclass = const Track>
   inline auto SharedPointer() const ->
      std::enable_if_t<
         std::is_const_v<Subclass>, std::shared_ptr<Subclass>
      >
   {
      // shared_from_this is injected into class scope by base class
      // std::enable_shared_from_this<Track>
      return std::static_pointer_cast<Subclass>( shared_from_this() );
   }

   // Static overloads of SharedPointer for when the pointer may be null
   template<typename Subclass = Track>
   static inline std::shared_ptr<Subclass> SharedPointer( Track *pTrack )
   { return pTrack ? pTrack->SharedPointer<Subclass>() : nullptr; }

   template<typename Subclass = const Track>
   static inline std::shared_ptr<Subclass> SharedPointer( const Track *pTrack )
   { return pTrack ? pTrack->SharedPointer<Subclass>() : nullptr; }

   // Find anything registered with TrackList::RegisterPendingChangedTrack and
   // not yet cleared or applied; if no such exists, return this track
   std::shared_ptr<Track> SubstitutePendingChangedTrack();
   std::shared_ptr<const Track> SubstitutePendingChangedTrack() const;

   // If this track is a pending changed track, return the corresponding
   // original; else return this track
   std::shared_ptr<const Track> SubstituteOriginalTrack() const;

   using IntervalData = TrackIntervalData;
   using Interval = TrackInterval;
   using Intervals = std::vector< Interval >;
   using ConstInterval = ConstTrackInterval;
   using ConstIntervals = std::vector< ConstInterval >;

   //! Names of a track type for various purposes.
   /*! Some of the distinctions exist only for historical reasons. */
   struct TypeNames {
      wxString info; //!< short, like "wave", in macro output, not internationalized
      wxString property; //!< short, like "wave", as a Lisp symbol property, not internationalized
      TranslatableString name; //!< long, like "Wave Track"
   };
   struct TypeInfo {
      TypeNames names;
      bool concrete = false;
      const TypeInfo *pBaseInfo = nullptr;

      bool IsBaseOf(const TypeInfo &other) const
      {
         for (auto pInfo = &other;
              pInfo; pInfo = pInfo->pBaseInfo)
            if (this == pInfo)
               return true;
         return false;
      }
   };
   virtual const TypeInfo &GetTypeInfo() const = 0;
   static const TypeInfo &ClassTypeInfo();
   virtual const TypeNames &GetTypeNames() const
   { return GetTypeInfo().names; }

   //! Whether this track type implements cut-copy-paste; by default, true
   virtual bool SupportsBasicEditing() const;

   using Holder = std::shared_ptr<Track>;

   //! Find or create the destination track for a paste, maybe in a different project
   /*! @return A smart pointer to the track; its `use_count()` can tell whether it is new */
   virtual Holder PasteInto( AudacityProject & ) const = 0;

   //! Report times on the track where important intervals begin and end, for UI to snap to
   /*!
   Some intervals may be empty, and no ordering of the intervals is assumed.
   */
   virtual ConstIntervals GetIntervals() const;

   /*! @copydoc GetIntervals()
   This overload exposes the extra data of the intervals as non-const
    */
   virtual Intervals GetIntervals();

 public:
   mutable std::pair<int, int> vrulerSize;

public:
   static void FinishCopy (const Track *n, Track *dest);

   //! Check consistency of channel groups, and maybe fix it
   /*!
    @param doFix whether to make any changes to correct inconsistencies
    @param completeList whether to assume that the TrackList containing this
    is completely loaded; if false, skip some of the checks
    @return true if no inconsistencies were found
    */
   virtual bool LinkConsistencyFix(bool doFix = true, bool completeList = true);

   //! Do the non-mutating part of consistency fix only and return status
   bool LinkConsistencyCheck(bool completeList)
   { return const_cast<Track*>(this)->LinkConsistencyFix(false, completeList); }

   bool HasOwner() const { return static_cast<bool>(GetOwner());}

   std::shared_ptr<TrackList> GetOwner() const { return mList.lock(); }

   LinkType GetLinkType() const noexcept;
   //! Returns true if the leader track has link type LinkType::Aligned
   bool IsAlignedWithLeader() const;

   ChannelGroupData &GetGroupData();
   const ChannelGroupData &GetGroupData() const;

protected:

   /*!
    @param completeList only influences debug build consistency checking
    */
   void SetLinkType(LinkType linkType, bool completeList = true);

   // Use this only to fix temporary inconsistency during deserialization!
   void DestroyGroupData();

private:
   int GetIndex() const;
   void SetIndex(int index);

   ChannelGroupData &MakeGroupData();
   /*!
    @param completeList only influences debug build consistency checking
    */
   void DoSetLinkType(LinkType linkType, bool completeList = true);

   Track* GetLinkedTrack() const;
   //! Returns true for leaders of multichannel groups
   bool HasLinkedTrack() const noexcept;

   //! Retrieve mNode with debug checks
   TrackNodePointer GetNode() const;
   //! Update mNode when Track is added to TrackList, or removed from it
   void SetOwner
      (const std::weak_ptr<TrackList> &list, TrackNodePointer node);

 // Keep in Track

 protected:
   double              mOffset;

 public:

   Track();
   Track(const Track &orig, ProtectedCreationArg&&);
   Track& operator =(const Track &orig) = delete;

   virtual ~ Track();

   void Init(const Track &orig);

   // public nonvirtual duplication function that invokes Clone():
   virtual Holder Duplicate() const;

   //! Name is always the same for all channels of a group
   const wxString &GetName() const;
   void SetName( const wxString &n );

   //! Selectedness is always the same for all channels of a group
   bool GetSelected() const;
   virtual void SetSelected(bool s);

   // The argument tells whether the last undo history state should be
   // updated for the appearance change
   void EnsureVisible( bool modifyState = false );

public:

   virtual double GetOffset() const = 0;

   void Offset(double t) { SetOffset(GetOffset() + t); }
   virtual void SetOffset (double o) { mOffset = o; }

   // method to set project tempo on track
   void OnProjectTempoChange(double newTempo);

   // Create a NEW track and modify this track
   // Return non-NULL or else throw
   // May assume precondition: t0 <= t1
   virtual Holder Cut(double WXUNUSED(t0), double WXUNUSED(t1)) = 0;

   // Create a NEW track and don't modify this track
   // Return non-NULL or else throw
   // Note that subclasses may want to distinguish tracks stored in a clipboard
   // from those stored in a project
   // May assume precondition: t0 <= t1
   // Should invoke Track::Init
   virtual Holder Copy
      (double WXUNUSED(t0), double WXUNUSED(t1), bool forClipboard = true) const = 0;

   // May assume precondition: t0 <= t1
   virtual void Clear(double WXUNUSED(t0), double WXUNUSED(t1)) = 0;

   virtual void Paste(double WXUNUSED(t), const Track * WXUNUSED(src)) = 0;

   // This can be used to adjust a sync-lock selected track when the selection
   // is replaced by one of a different length.
   virtual void SyncLockAdjust(double oldT1, double newT1);

   // May assume precondition: t0 <= t1
   virtual void Silence(double WXUNUSED(t0), double WXUNUSED(t1)) = 0;

   // May assume precondition: t0 <= t1
   virtual void InsertSilence(double WXUNUSED(t), double WXUNUSED(len)) = 0;

private:
   // Subclass responsibility implements only a part of Duplicate(), copying
   // the track data proper (not associated data such as for groups and views):
   virtual Holder Clone() const = 0;

   template<typename T>
      friend std::enable_if_t< std::is_pointer_v<T>, T >
         track_cast(Track *track);
   template<typename T>
      friend std::enable_if_t<
         std::is_pointer_v<T> &&
            std::is_const_v< std::remove_pointer_t< T > >,
         T
      >
         track_cast(const Track *track);

public:
   bool SameKindAs(const Track &track) const
      { return &GetTypeInfo() == &track.GetTypeInfo(); }

   /*!
    Do a TypeSwitch on this track, among all subtypes enumerated up to the point
    of the call
    */
   template<typename R = void, typename ...Functions>
   R TypeSwitch(const Functions &...functions)
   {
      struct Here : TrackTypeTag {};
      // List more derived classes later
      using TrackTypes =
         typename TypeEnumerator::CollectTypes<TrackTypeTag, Here>::type;
      return TypeSwitch::VDispatch<R, TrackTypes>(*this, functions...);
   }

   /*! @copydoc Track::TypeSwitch */
   template<typename R = void, typename ...Functions>
   R TypeSwitch(const Functions &...functions) const
   {
      struct Here : TrackTypeTag {};
      // List more derived classes later
      using namespace TypeList;
      using TrackTypes = Map_t<Fn<std::add_const_t>,
         typename TypeEnumerator::CollectTypes<TrackTypeTag, Here>::type>;
      return TypeSwitch::VDispatch<R, TrackTypes>(*this, functions...);
   }

   // XMLTagHandler callback methods -- NEW virtual for writing
   virtual void WriteXML(XMLWriter &xmlFile) const = 0;

   //! Returns nonempty if an error was encountered while trying to
   //! open the track from XML
   /*!
    May assume consistency of stereo channel grouping and examine other channels
    */
   virtual std::optional<TranslatableString> GetErrorOpening() const;

   virtual double GetStartTime() const = 0;
   virtual double GetEndTime() const = 0;

   // Send a notification to subscribers when state of the track changes
   // To do: define values for the argument to distinguish different parts
   // of the state
   void Notify(bool allChannels, int code = -1);

   // An always-true predicate useful for defining iterators
   bool Any() const;

   // Frequently useful operands for + and -
   bool IsSelected() const;
   bool IsLeader() const;
   bool IsSelectedLeader() const;

   // Cause this track and following ones in its TrackList to adjust
   void AdjustPositions();

   // Serialize, not with tags of its own, but as attributes within a tag.
   void WriteCommonXMLAttributes(
      XMLWriter &xmlFile, bool includeNameAndSelected = true) const;

   // Return true iff the attribute is recognized.
   bool HandleCommonXMLAttribute(const std::string_view& attr, const XMLAttributeValueView& valueView);

private:
   virtual void DoOnProjectTempoChange(
      const std::optional<double>& oldTempo, double newTempo) = 0;

   std::optional<double> mProjectTempo;
};

ENUMERATE_TRACK_TYPE(Track);

//! Encapsulate the checked down-casting of track pointers
/*! Eliminates possibility of error -- and not quietly casting away const

Typical usage:
```
if (auto wt = track_cast<const WaveTrack*>(track)) { ... }
```
 */
template<typename T>
   inline std::enable_if_t< std::is_pointer_v<T>, T >
      track_cast(Track *track)
{
   using BareType = std::remove_pointer_t< T >;
   if (track &&
       BareType::ClassTypeInfo().IsBaseOf(track->GetTypeInfo() ))
      return reinterpret_cast<T>(track);
   else
      return nullptr;
}

/*! @copydoc track_cast(Track*)
This overload for const pointers can cast only to other const pointer types. */
template<typename T>
   inline std::enable_if_t<
      std::is_pointer_v<T> && std::is_const_v< std::remove_pointer_t< T > >,
      T
   >
      track_cast(const Track *track)
{
   using BareType = std::remove_pointer_t< T >;
   if (track &&
       BareType::ClassTypeInfo().IsBaseOf(track->GetTypeInfo() ))
      return reinterpret_cast<T>(track);
   else
      return nullptr;
}

template < typename TrackType > struct TrackIterRange;

//! Iterator over only members of a TrackList of the specified subtype, optionally filtered by a predicate; past-end value dereferenceable, to nullptr
/*! Does not suffer invalidation when an underlying std::list iterator is deleted, provided that is not
    equal to its current position or to the beginning or end iterator.

    The filtering predicate is tested only when the iterator is constructed or advanced.

    @tparam TrackType Track or a subclass, maybe const-qualified
 */
template <
   typename TrackType
> class TrackIter
   : public ValueIterator< TrackType *, std::bidirectional_iterator_tag >
{
public:
   //! Type of predicate taking pointer to const TrackType
   using FunctionType = std::function< bool(
      std::add_pointer_t< std::add_const_t< std::remove_pointer_t<TrackType> > >
   ) >;

   //! Constructor, usually not called directly except by methods of TrackList
   TrackIter(
         TrackNodePointer begin, //!< Remember lower bound
         TrackNodePointer iter, //!< The actual pointer
         TrackNodePointer end, //!< Remember upper bound
         FunctionType pred = {} //!< %Optional filter
   )
      : mBegin( begin ), mIter( iter ), mEnd( end )
      , mPred( std::move(pred) )
   {
      // Establish the class invariant
      if (this->mIter != this->mEnd && !this->valid())
         this->operator ++ ();
   }

   //! Return an iterator that replaces the predicate
   /*! Advance to the first position at or after the old position that satisfies the new predicate,
   or to the end */
   template < typename Predicate2 >
   TrackIter Filter( const Predicate2 &pred2 ) const
   {
      return { this->mBegin, this->mIter, this->mEnd, pred2 };
   }

   //! Return an iterator for a subclass of TrackType (and not removing const) with same predicate
   /*! Advance to the first position at or after the old position that
   satisfies the type constraint, or to the end */
   template < typename TrackType2 >
      auto Filter() const
         -> std::enable_if_t<
            std::is_base_of_v< TrackType, TrackType2 > &&
               (!std::is_const_v<TrackType> ||
                 std::is_const_v<TrackType2>),
            TrackIter< TrackType2 >
         >
   {
      return { this->mBegin, this->mIter, this->mEnd, this->mPred };
   }

   const FunctionType &GetPredicate() const
   { return this->mPred; }

   //! Safe to call even when at the end
   /*! In that case *this remains unchanged. */
   TrackIter &operator ++ ()
   {
      // Maintain the class invariant
      if (this->mIter != this->mEnd) do
         ++this->mIter.first;
      while (this->mIter != this->mEnd && !this->valid() );
      return *this;
   }

   //! @copydoc operator++
   TrackIter operator ++ (int)
   {
      TrackIter result { *this };
      this-> operator ++ ();
      return result;
   }

   //! Safe to call even when at the beginning
   /*! In that case *this wraps to the end. */
   TrackIter &operator -- ()
   {
      // Maintain the class invariant
      do {
         if (this->mIter == this->mBegin)
            // Go circularly
            this->mIter = this->mEnd;
         else
            --this->mIter.first;
      } while (this->mIter != this->mEnd && !this->valid() );
      return *this;
   }

   //! @copydoc operator--
   TrackIter operator -- (int)
   {
      TrackIter result { *this };
      this->operator -- ();
      return result;
   }

   //! Safe to call even when at the end
   /*! In that case you get nullptr. */
   TrackType *operator * () const
   {
      if (this->mIter == this->mEnd)
         return nullptr;
      else
         // Other methods guarantee that the cast is correct
         // (provided no operations on the TrackList invalidated
         // underlying iterators or replaced the tracks there)
         return static_cast< TrackType * >( &**this->mIter.first );
   }

   //! This might be called operator + , but it's not constant-time as with a random access iterator
   TrackIter advance(
      long amount //!< may be negative
   ) const
   {
      auto copy = *this;
      std::advance( copy, amount );
      return copy;
   }

   //! Compares only current positions, assuming same beginnings and ends
   friend inline bool operator == (TrackIter a, TrackIter b)
   {
      // Assume the predicate is not stateful.  Just compare the iterators.
      return
         a.mIter == b.mIter
         // Assume this too:
         // && a.mBegin == b.mBegin && a.mEnd == b.mEnd
      ;
   }

   //! @copydoc operator==
   friend inline bool operator != (TrackIter a, TrackIter b)
   {
      return !(a == b);
   }

private:
   /*! @pre underlying iterators are still valid, and mPred, if not null, is well behaved */
   /*! @invariant mIter == mEnd, or else, mIter != mEnd,
   and **mIter is of the appropriate subclass, and mPred is null or mPred(&**mIter) is true. */

   //! Test satisfaction of the invariant, while initializing, incrementing, or decrementing
   bool valid() const
   {
      // assume mIter != mEnd
      const auto pTrack = track_cast< TrackType * >( &**this->mIter.first );
      if (!pTrack)
         return false;
      return !this->mPred || this->mPred( pTrack );
   }

   //! This friendship is needed in TrackIterRange::StartingWith and TrackIterRange::EndingAfter()
   friend TrackIterRange< TrackType >;

   TrackNodePointer
      mBegin, //!< Allows end of reverse iteration to be detected without comparison to other TrackIter
      mIter, //!< Current position
      mEnd; //!< Allows end of iteration to be detected without comparison to other TrackIter
   FunctionType mPred; //!< %Optional filter
};

//! Range between two @ref TrackIter "TrackIters", usable in range-for statements, and with Visit member functions
template <
   typename TrackType // Track or a subclass, maybe const-qualified
> struct TrackIterRange
   : public IteratorRange< TrackIter< TrackType > >
{
   TrackIterRange
      ( const TrackIter< TrackType > &begin,
        const TrackIter< TrackType > &end )
         : IteratorRange< TrackIter< TrackType > >
            ( begin, end )
   {}

   // Conjoin the filter predicate with another predicate
   // Read + as "and"
   template< typename Predicate2 >
      TrackIterRange operator + ( const Predicate2 &pred2 ) const
   {
      const auto &pred1 = this->first.GetPredicate();
      using Function = typename TrackIter<TrackType>::FunctionType;
      const auto &newPred = pred1
         ? Function{ [=] (typename Function::argument_type track) {
            return pred1(track) && pred2(track);
         } }
         : Function{ pred2 };
      return {
         this->first.Filter( newPred ),
         this->second.Filter( newPred )
      };
   }

   // Specify the added conjunct as a pointer to member function
   // Read + as "and"
   template< typename R, typename C >
      TrackIterRange operator + ( R ( C ::* pmf ) () const ) const
   {
      return this->operator + ( std::mem_fn( pmf ) );
   }

   // Conjoin the filter predicate with the negation of another predicate
   // Read - as "and not"
   template< typename Predicate2 >
      TrackIterRange operator - ( const Predicate2 &pred2 ) const
   {
      using ArgumentType =
         typename TrackIterRange::iterator::FunctionType::argument_type;
      auto neg = [=] (ArgumentType track) { return !pred2( track ); };
      return this->operator + ( neg );
   }

   // Specify the negated conjunct as a pointer to member function
   // Read - as "and not"
   template< typename R, typename C >
      TrackIterRange operator - ( R ( C ::* pmf ) () const ) const
   {
      return this->operator + ( std::not1( std::mem_fn( pmf ) ) );
   }

   template< typename TrackType2 >
      TrackIterRange< TrackType2 > Filter() const
   {
      return {
         this-> first.template Filter< TrackType2 >(),
         this->second.template Filter< TrackType2 >()
      };
   }

   TrackIterRange StartingWith( const Track *pTrack ) const
   {
      auto newBegin = this->find( pTrack );
      // More careful construction is needed so that the independent
      // increment and decrement of each iterator in the NEW pair
      // has the expected behavior at boundaries of the range
      return {
         { newBegin.mIter, newBegin.mIter,    this->second.mEnd,
           this->first.GetPredicate() },
         { newBegin.mIter, this->second.mIter, this->second.mEnd,
           this->second.GetPredicate() }
      };
   }

   TrackIterRange EndingAfter( const Track *pTrack ) const
   {
      const auto newEnd = this->reversal().find( pTrack ).base();
      // More careful construction is needed so that the independent
      // increment and decrement of each iterator in the NEW pair
      // has the expected behavior at boundaries of the range
      return {
         { this->first.mBegin, this->first.mIter, newEnd.mIter,
           this->first.GetPredicate() },
         { this->first.mBegin, newEnd.mIter,      newEnd.mIter,
           this->second.GetPredicate() }
      };
   }

   // Exclude one given track
   TrackIterRange Excluding ( const TrackType *pExcluded ) const
   {
      return this->operator - (
         [=](const Track *pTrack){ return pExcluded == pTrack; } );
   }

   //! See Track::TypeSwitch
   template< typename ...Functions >
   void Visit(const Functions &...functions)
   {
      for (auto track : *this)
         track->TypeSwitch(functions...);
   }

   //! See Track::TypeSwitch
   /*! Visit until flag is false, or no more tracks */
   template< typename Flag, typename ...Functions >
   void VisitWhile(Flag &flag, const Functions &...functions)
   {
      if ( flag ) for (auto track : *this) {
         track->TypeSwitch(functions...);
         if (!flag)
            break;
      }
   }
};


//! Notification of changes in individual tracks of TrackList, or of TrackList's composition
struct TrackListEvent
{
   enum Type {
      //! Posted when the set of selected tracks changes.
      SELECTION_CHANGE,

      //! Posted when certain fields of a track change.
      TRACK_DATA_CHANGE,

      //! Posted when a track needs to be scrolled into view.
      TRACK_REQUEST_VISIBLE,

      //! Posted when tracks are reordered but otherwise unchanged.
      /*! mpTrack points to the moved track that is earliest in the New ordering. */
      PERMUTED,

      //! Posted when some track changed its height.
      RESIZING,

      //! Posted when a track has been added to a tracklist.  Also posted when one track replaces another
      ADDITION,

      //! Posted when a track has been deleted from a tracklist. Also posted when one track replaces another
      /*! mpTrack points to the removed track. It is expected, that track is valid during the event.
       *! mExtra is 1 if the track is being replaced by another track, 0 otherwise.
       */
      DELETION,
   };

   TrackListEvent( Type type,
      const std::weak_ptr<Track> &pTrack = {}, int extra = -1)
      : mType{ type }
      , mpTrack{ pTrack }
      , mExtra{ extra }
   {}

   TrackListEvent( const TrackListEvent& ) = default;

   const Type mType;
   const std::weak_ptr<Track> mpTrack;
   const int mExtra;
};

/*! @brief A flat linked list of tracks supporting Add,  Remove,
 * Clear, and Contains, serialization of the list of tracks, event notifications
 */
class TRACK_API TrackList final
   : public Observer::Publisher<TrackListEvent>
   , public ListOfTracks
   , public std::enable_shared_from_this<TrackList>
   , public ClientData::Base
{
   // privatize this, make you use Add instead:
   using ListOfTracks::push_back;

   // privatize this, make you use Swap instead:
   using ListOfTracks::swap;

   // Disallow copy
   TrackList(const TrackList &that) = delete;
   TrackList &operator= (const TrackList&) = delete;

   // No need for move, disallow it
   TrackList(TrackList &&that) = delete;
   TrackList& operator= (TrackList&&) = delete;

   void clear() = delete;

 public:
   static TrackList &Get( AudacityProject &project );
   static const TrackList &Get( const AudacityProject &project );

   static TrackList *FindUndoTracks(const UndoStackElem &state);

   // Create an empty TrackList
   // Don't call directly -- use Create() instead
   explicit TrackList( AudacityProject *pOwner );

   // Create an empty TrackList
   static std::shared_ptr<TrackList> Create( AudacityProject *pOwner );

   // Move is defined in terms of Swap
   void Swap(TrackList &that);

   // Destructor
   virtual ~TrackList();

   // Find the owning project, which may be null
   AudacityProject *GetOwner() { return mOwner; }
   const AudacityProject *GetOwner() const { return mOwner; }

   /**
    * \brief Returns string that contains baseTrackName,
    * but is guaranteed to be unique among other tracks in that list.
    * \param baseTrackName String to be put into the template
    * \return Formatted string: "[baseTrackName] [N]"
    */
   wxString MakeUniqueTrackName(const wxString& baseTrackName) const;

   // Iteration

   // Hide the inherited begin() and end()
   using iterator = TrackIter<Track>;
   using const_iterator = TrackIter<const Track>;
   using value_type = Track *;
   iterator begin() { return Any().begin(); }
   iterator end() { return Any().end(); }
   const_iterator begin() const { return Any().begin(); }
   const_iterator end() const { return Any().end(); }
   const_iterator cbegin() const { return begin(); }
   const_iterator cend() const { return end(); }

   //! Turn a pointer into a TrackIter (constant time); get end iterator if this does not own the track
   template < typename TrackType = Track >
      auto Find(Track *pTrack)
         -> TrackIter< TrackType >
   {
      if (!pTrack || pTrack->GetOwner().get() != this)
         return EndIterator<TrackType>();
      else
         return MakeTrackIterator<TrackType>( pTrack->GetNode() );
   }

   //! @copydoc Find
   /*! const overload will only produce iterators over const TrackType */
   template < typename TrackType = const Track >
      auto Find(const Track *pTrack) const
         -> std::enable_if_t< std::is_const_v<TrackType>,
            TrackIter< TrackType >
         >
   {
      if (!pTrack || pTrack->GetOwner().get() != this)
         return EndIterator<TrackType>();
      else
         return MakeTrackIterator<TrackType>( pTrack->GetNode() );
   }

   // If the track is not an audio track, or not one of a group of channels,
   // return the track itself; else return the first channel of its group --
   // in either case as an iterator that will only visit other leader tracks.
   // (Generalizing away from the assumption of at most stereo)
   TrackIter< Track > FindLeader( Track *pTrack );

   TrackIter< const Track >
      FindLeader( const Track *pTrack ) const
   {
      return const_cast<TrackList*>(this)->
         FindLeader( const_cast<Track*>(pTrack) ).Filter< const Track >();
   }


   template < typename TrackType = Track >
      auto Any()
         -> TrackIterRange< TrackType >
   {
      return Tracks< TrackType >();
   }

   template < typename TrackType = const Track >
      auto Any() const
         -> std::enable_if_t< std::is_const_v<TrackType>,
            TrackIterRange< TrackType >
         >
   {
      return Tracks< TrackType >();
   }

   // Abbreviating some frequently used cases
   template < typename TrackType = Track >
      auto Selected()
         -> TrackIterRange< TrackType >
   {
      return Tracks< TrackType >( &Track::IsSelected );
   }

   template < typename TrackType = const Track >
      auto Selected() const
         -> std::enable_if_t< std::is_const_v<TrackType>,
            TrackIterRange< TrackType >
         >
   {
      return Tracks< TrackType >( &Track::IsSelected );
   }


   template < typename TrackType = Track >
      auto Leaders()
         -> TrackIterRange< TrackType >
   {
      return Tracks< TrackType >( &Track::IsLeader );
   }

   template < typename TrackType = const Track >
      auto Leaders() const
         -> std::enable_if_t< std::is_const_v<TrackType>,
            TrackIterRange< TrackType >
         >
   {
      return Tracks< TrackType >( &Track::IsLeader );
   }


   template < typename TrackType = Track >
      auto SelectedLeaders()
         -> TrackIterRange< TrackType >
   {
      return Tracks< TrackType >( &Track::IsSelectedLeader );
   }

   template < typename TrackType = const Track >
      auto SelectedLeaders() const
         -> std::enable_if_t< std::is_const_v<TrackType>,
            TrackIterRange< TrackType >
         >
   {
      return Tracks< TrackType >( &Track::IsSelectedLeader );
   }


   template<typename TrackType>
      static auto SingletonRange( TrackType *pTrack )
         -> TrackIterRange< TrackType >
   {
      return pTrack->GetOwner()->template Any<TrackType>()
         .StartingWith( pTrack ).EndingAfter( pTrack );
   }


private:
   Track *DoAddToHead(const std::shared_ptr<Track> &t);
   Track *DoAdd(const std::shared_ptr<Track> &t);

   template< typename TrackType, typename InTrackType >
      static TrackIterRange< TrackType >
         Channels_( TrackIter< InTrackType > iter1 )
   {
      // Assume iterator filters leader tracks
      if (*iter1) {
         return {
            iter1.Filter( &Track::Any )
               .template Filter<TrackType>(),
            (++iter1).Filter( &Track::Any )
               .template Filter<TrackType>()
         };
      }
      else
         // empty range
         return {
            iter1.template Filter<TrackType>(),
            iter1.template Filter<TrackType>()
         };
   }

public:
   // Find an iterator range of channels including the given track.
   template< typename TrackType >
      static auto Channels( TrackType *pTrack )
         -> TrackIterRange< TrackType >
   {
      return Channels_<TrackType>( pTrack->GetOwner()->FindLeader(pTrack) );
   }

   //! Count channels of a track
   static size_t NChannels(const Track &track)
   {
      return Channels(&track).size();
   }

   //! If the given track is one of a pair of channels, swap them
   /*! @return success */
   static bool SwapChannels(Track &track);

   friend class Track;

   //! For use in sorting:  assume each iterator points into this list, no duplications
   void Permute(const std::vector<TrackNodePointer> &permutation);

   Track *FindById( TrackId id );

   /// Add a Track, giving it a fresh id
   template<typename TrackKind>
      TrackKind *AddToHead( const std::shared_ptr< TrackKind > &t )
         { return static_cast< TrackKind* >( DoAddToHead( t ) ); }

   template<typename TrackKind>
      TrackKind *Add( const std::shared_ptr< TrackKind > &t )
         { return static_cast< TrackKind* >( DoAdd( t ) ); }

   //! Removes linkage if track belongs to a group
   void UnlinkChannels(Track& track);
   /** \brief Converts channels to a multichannel track.
   * @param first and the following must be in this list. Tracks should
   * not be a part of another group (not linked)
   * @param nChannels number of channels, for now only 2 channels supported
   * @param aligned if true, the link type will be set to Track::LinkType::Aligned,
   * or Track::LinkType::Group otherwise
   * @returns true on success, false if some prerequisites do not met
   */
   bool MakeMultiChannelTrack(Track& first, int nChannels, bool aligned);

   /// Replace first track with second track, give back a holder
   /// Give the replacement the same id as the replaced
   ListOfTracks::value_type Replace(
      Track * t, const ListOfTracks::value_type &with);

   //! Remove the Track and return an iterator to what followed it.
   TrackNodePointer Remove(Track *t);

   /// Make the list empty
   void Clear(bool sendEvent = true);

   bool CanMoveUp(Track * t) const;
   bool CanMoveDown(Track * t) const;

   bool MoveUp(Track * t);
   bool MoveDown(Track * t);
   bool Move(Track * t, bool up) { return up ? MoveUp(t) : MoveDown(t); }

   //! Mainly a test function. Uses a linear search, so could be slow.
   bool Contains(const Track * t) const;

   // Return non-null only if the weak pointer is not, and the track is
   // owned by this list; constant time.
   template <typename Subclass>
   std::shared_ptr<Subclass> Lock(const std::weak_ptr<Subclass> &wTrack)
   {
      auto pTrack = wTrack.lock();
      if (pTrack) {
         auto pList = pTrack->mList.lock();
         if (pTrack && this == pList.get())
            return pTrack;
      }
      return {};
   }

   bool empty() const;
   size_t NChannels() const;
   size_t Size() const { return Leaders().size(); }

   double GetStartTime() const;
   double GetEndTime() const;

   double GetMinOffset() const;

private:
   using ListOfTracks::size;

   // Visit all tracks satisfying a predicate, mutative access
   template <
      typename TrackType = Track,
      typename Pred =
         typename TrackIterRange< TrackType >::iterator::FunctionType
   >
      auto Tracks( const Pred &pred = {} )
         -> TrackIterRange< TrackType >
   {
      auto b = getBegin(), e = getEnd();
      return { { b, b, e, pred }, { b, e, e, pred } };
   }

   // Visit all tracks satisfying a predicate, const access
   template <
      typename TrackType = const Track,
      typename Pred =
         typename TrackIterRange< TrackType >::iterator::FunctionType
   >
      auto Tracks( const Pred &pred = {} ) const
         -> std::enable_if_t< std::is_const_v<TrackType>,
            TrackIterRange< TrackType >
         >
   {
      auto b = const_cast<TrackList*>(this)->getBegin();
      auto e = const_cast<TrackList*>(this)->getEnd();
      return { { b, b, e, pred }, { b, e, e, pred } };
   }

   Track *GetPrev(Track * t, bool linked = false) const;
   Track *GetNext(Track * t, bool linked = false) const;

   template < typename TrackType >
      TrackIter< TrackType >
         MakeTrackIterator( TrackNodePointer iter ) const
   {
      auto b = const_cast<TrackList*>(this)->getBegin();
      auto e = const_cast<TrackList*>(this)->getEnd();
      return { b, iter, e };
   }

   template < typename TrackType >
      TrackIter< TrackType >
         EndIterator() const
   {
      auto e = const_cast<TrackList*>(this)->getEnd();
      return { e, e, e };
   }

   TrackIterRange< Track > EmptyRange() const;

   bool isNull(TrackNodePointer p) const
   { return (p.second == this && p.first == ListOfTracks::end())
      || (p.second == &mPendingUpdates && p.first == mPendingUpdates.end()); }
   TrackNodePointer getEnd() const
   { return { const_cast<TrackList*>(this)->ListOfTracks::end(),
              const_cast<TrackList*>(this)}; }
   TrackNodePointer getBegin() const
   { return { const_cast<TrackList*>(this)->ListOfTracks::begin(),
              const_cast<TrackList*>(this)}; }

   //! Move an iterator to the next node, if any; else stay at end
   TrackNodePointer getNext(TrackNodePointer p) const
   {
      if ( isNull(p) )
         return p;
      auto q = p;
      ++q.first;
      return q;
   }

   //! Move an iterator to the previous node, if any; else wrap to end
   TrackNodePointer getPrev(TrackNodePointer p) const
   {
      if (p == getBegin())
         return getEnd();
      else {
         auto q = p;
         --q.first;
         return q;
      }
   }

   void RecalcPositions(TrackNodePointer node);
   void QueueEvent(TrackListEvent event);
   void SelectionEvent(Track &track);
   void PermutationEvent(TrackNodePointer node);
   void DataEvent(
      const std::shared_ptr<Track> &pTrack, bool allChannels, int code );
   void EnsureVisibleEvent(
      const std::shared_ptr<Track> &pTrack, bool modifyState );
   void DeletionEvent(std::weak_ptr<Track> node, bool duringReplace);
   void AdditionEvent(TrackNodePointer node);
   void ResizingEvent(TrackNodePointer node);

   void SwapNodes(TrackNodePointer s1, TrackNodePointer s2);

   // Nondecreasing during the session.
   // Nonpersistent.
   // Used to assign ids to added tracks.
   static long sCounter;

public:
   using Updater = std::function< void(Track &dest, const Track &src) >;
   // Start a deferred update of the project.
   // The return value is a duplicate of the given track.
   // While ApplyPendingTracks or ClearPendingTracks is not yet called,
   // there may be other direct changes to the project that push undo history.
   // Meanwhile the returned object can accumulate other changes for a deferred
   // push, and temporarily shadow the actual project track for display purposes.
   // The Updater function, if not null, merges state (from the actual project
   // into the pending track) which is not meant to be overridden by the
   // accumulated pending changes.
   // To keep the display consistent, the Y and Height values, minimized state,
   // and Linked state must be copied, and this will be done even if the
   // Updater does not do it.
   // Pending track will have the same TrackId as the actual.
   // Pending changed tracks will not occur in iterations.
   std::shared_ptr<Track> RegisterPendingChangedTrack(
      Updater updater,
      Track *src
   );

   // Like the previous, but for a NEW track, not a replacement track.  Caller
   // supplies the track, and there are no updates.
   // Pending track will have an unassigned TrackId.
   // Pending changed tracks WILL occur in iterations, always after actual
   // tracks, and in the sequence that they were added.  They can be
   // distinguished from actual tracks by TrackId.
   void RegisterPendingNewTrack( const std::shared_ptr<Track> &pTrack );

   // Invoke the updaters of pending tracks.  Pass any exceptions from the
   // updater functions.
   void UpdatePendingTracks();

   // Forget pending track additions and changes;
   // if requested, give back the pending added tracks.
   void ClearPendingTracks( ListOfTracks *pAdded = nullptr );

   // Change the state of the project.
   // Strong guarantee for project state in case of exceptions.
   // Will always clear the pending updates.
   // Return true if the state of the track list really did change.
   bool ApplyPendingTracks();

   bool HasPendingTracks() const;

private:
   AudacityProject *mOwner;

   //! Shadow tracks holding append-recording in progress; need to put them into a list so that GetLink() works
   /*! Beware, they are in a disjoint iteration sequence from ordinary tracks */
   ListOfTracks mPendingUpdates;
   //! This is in correspondence with mPendingUpdates
   std::vector< Updater > mUpdaters;
};

#endif
