/**********************************************************************

  Audacity: A Digital Audio Editor

  LabelTrack.h

  Dominic Mazzoni
  James Crook
  Jun Wan

**********************************************************************/

#ifndef _LABELTRACK_
#define _LABELTRACK_

#include "SelectedRegion.h"
#include "Track.h"

class wxTextFile;

class AudacityProject;
class TimeWarper;

class LabelTrack;
struct LabelTrackHit;
struct TrackPanelDrawingContext;

class AUDACITY_DLL_API LabelStruct
{
public:
   LabelStruct() = default;
   // Copies region
   LabelStruct(const SelectedRegion& region, const wxString &aTitle);
   // Copies region but then overwrites other times
   LabelStruct(
      const SelectedRegion& region, double t0, double t1, BPS labTempo,
      const wxString& aTitle);
   const SelectedRegion &getSelectedRegion() const { return selectedRegion; }
   double getDuration(BPS labTempo) const { return selectedRegion.duration(labTempo); }
   double getT0(BPS labTempo) const { return selectedRegion.t0(labTempo); }
   double getT1(BPS labTempo) const { return selectedRegion.t1(labTempo); }
   // Returns true iff the label got inverted:
   bool AdjustEdge( int iEdge, double fNewTime, BPS labTempo);
   void MoveLabel( int iEdge, double fNewTime, BPS labTempo);

   struct BadFormatException {};
   static LabelStruct Import(wxTextFile &file, int &index);

   void Export(wxTextFile &file) const;

   /// Relationships between selection region and labels
   enum TimeRelations
   {
       BEFORE_LABEL,
       AFTER_LABEL,
       SURROUNDS_LABEL,
       WITHIN_LABEL,
       BEGINS_IN_LABEL,
       ENDS_IN_LABEL
   };

   /// Returns relationship between a region described and this label; if
   /// parent is set, it will consider point labels at the very beginning
   /// and end of parent to be within a region that borders them (this makes
   /// it possible to DELETE capture all labels with a Select All).
   TimeRelations RegionRelation(double reg_t0, double reg_t1,
                                const LabelTrack *parent = NULL) const;

public:
   SelectedRegion selectedRegion;
   wxString title; /// Text of the label.
   mutable int width{}; /// width of the text in pixels.

// Working storage for on-screen layout.
   mutable int x{};     /// Pixel position of left hand glyph
   mutable int x1{};    /// Pixel position of right hand glyph
   mutable int xText{}; /// Pixel position of left hand side of text box
   mutable int y{};     /// Pixel position of label.

   bool updated{};                  /// flag to tell if the label times were updated
};

using LabelArray = std::vector<LabelStruct>;

class AUDACITY_DLL_API LabelTrack final
   : public Track
   , public Observer::Publisher<struct LabelTrackEvent>
{
 public:
   static wxString GetDefaultName();

   // Construct and also build all attachments
   static LabelTrack *New(AudacityProject &project);

   /**
    * \brief Create a new LabelTrack with specified @p name and append it to the @p trackList
    * \return New LabelTrack with custom name
    */
   static LabelTrack* Create(TrackList& trackList, const wxString& name);

   /**
    * \brief Create a new LabelTrack with unique default name and append it to the @p trackList
    * \return New LabelTrack with unique default name
    */
   static LabelTrack* Create(TrackList& trackList);

   LabelTrack();
   LabelTrack(const LabelTrack &orig, ProtectedCreationArg&&);

   virtual ~ LabelTrack();

   void SetLabel( size_t iLabel, const LabelStruct &newLabel );

   void SetOffset(double dOffset, BPS labTempo) override;

   void SetSelected(bool s) override;

   double GetOffset(BPS labTempo) const override;
   double GetStartTime(BPS labTempo) const override;
   double GetEndTime(BPS labTempo) const override;

   using Holder = std::shared_ptr<LabelTrack>;

private:
   Track::Holder Clone() const override;

public:
   bool HandleXMLTag(const std::string_view& tag, const AttributesList& attrs) override;
   XMLTagHandler *HandleXMLChild(const std::string_view& tag) override;
   void WriteXML(XMLWriter &xmlFile) const override;

   Track::Holder Cut(double t0, double t1, BPS labTempo) override;
   Track::Holder Copy(
      double t0, double t1, BPS labTempo,
      bool forClipboard = true) const override;
   void Clear(double t0, double t1, BPS labTempo) override;
   void Paste(double t, BPS labTempo, const Track* src) override;
   bool Repeat(double t0, double t1, int n);
   void SyncLockAdjust(double oldT1, double newT1, BPS labTempo) override;

   void Silence(double t0, double t1, BPS labTempo) override;
   void InsertSilence(double t, double len, BPS labTempo) override;

   void Import(wxTextFile & f);
   void Export(wxTextFile & f) const;

   int GetNumLabels() const;
   const LabelStruct *GetLabel(int index) const;
   const LabelArray &GetLabels() const { return mLabels; }

   void OnLabelAdded( const wxString &title, int pos );
   //This returns the index of the label we just added.
   int AddLabel(const SelectedRegion &region, const wxString &title);

   //This deletes the label at given index.
   void DeleteLabel(int index);

   // This pastes labels without shifting existing ones
   bool PasteOver(double t, const Track *src);

   // PRL:  These functions were not used because they were not overrides!  Was that right?
   //Track::Holder SplitCut(double b, double e) /* not override */;
   //bool SplitDelete(double b, double e) /* not override */;

   void ShiftLabelsOnInsert(double length, double pt, BPS labTempo);
   void ChangeLabelsOnReverse(double b, double e, BPS labTempo);
   void ScaleLabels(double b, double e, double change, BPS labTempo);
   double AdjustTimeStampOnScale(double t, double b, double e, double change, BPS labTempo);
   void WarpLabels(const TimeWarper &warper, BPS labTempo);

   // Returns tab-separated text of all labels completely within given region
   wxString GetTextOfLabels(double t0, double t1) const;

   int FindNextLabel(const SelectedRegion& currentSelection);
   int FindPrevLabel(const SelectedRegion& currentSelection);

   const TypeInfo &GetTypeInfo() const override;
   static const TypeInfo &ClassTypeInfo();

   Track::Holder PasteInto(AudacityProject&, BPS labTempo) const override;

   struct IntervalData final : Track::IntervalData {
      size_t index;
      explicit IntervalData(size_t index) : index{index} {};
   };
   ConstInterval MakeInterval(size_t index, BPS labTempo) const;
   Interval MakeInterval(size_t index, BPS labTempo);
   ConstIntervals GetIntervals(BPS labTempo) const override;
   Intervals GetIntervals(BPS labTempo) override;

public:
   void SortLabels();

 private:
   LabelArray mLabels;

   // Set in copied label tracks
   double mClipLen;

   int miLastLabel;                 // used by FindNextLabel and FindPrevLabel
};

ENUMERATE_TRACK_TYPE(LabelTrack);

struct LabelTrackEvent
{
   enum Type {
      Addition,
      Deletion,
      Permutation,
      Selection,
   } type;

   const std::weak_ptr<Track> mpTrack;

   // invalid for selection events
   wxString mTitle;

   // invalid for addition and selection events
   int mFormerPosition;

   // invalid for deletion and selection events
   int mPresentPosition;

   LabelTrackEvent( Type type, const std::shared_ptr<LabelTrack> &pTrack,
      const wxString &title,
      int formerPosition,
      int presentPosition)
   : type{ type }
   , mpTrack{ pTrack }
   , mTitle{ title }
   , mFormerPosition{ formerPosition }
   , mPresentPosition{ presentPosition }
   {}
};

#endif
