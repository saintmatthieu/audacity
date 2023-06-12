#include "WaveClipStretchHandle.h"
#include "ProjectAudioIO.h"
#include "RefreshCode.h"

#include <wx/event.h>
#include <wx/utils.h>

#include "../../../../../images/Cursors.h"
#include "../../../../Snap.h"
#include "../../../../TrackArtist.h"
#include "../../../../TrackPanelDrawingContext.h"
#include "HitTestResult.h"
#include "ProjectHistory.h"
#include "TrackPanelMouseEvent.h"
#include "UndoManager.h"
#include "ViewInfo.h"
#include "WaveClip.h"
#include "WaveTrack.h"
#include "WaveTrackView.h"

namespace
{

std::vector<std::shared_ptr<WaveClip>>
FindClipsInChannels(double start, double end, WaveTrack* track)
{
   std::vector<std::shared_ptr<WaveClip>> result;
   for (auto channel : TrackList::Channels(track))
   {
      for (auto& clip : channel->GetClips())
      {
         if (clip->GetPlayStartTime() == start && clip->GetPlayEndTime() == end)
            result.push_back(clip);
      }
   }
   return result;
}
} // namespace

WaveClipStretchHandle::ClipStretchPolicy::~ClipStretchPolicy() = default;

class WaveClipStretchHandle::AdjustBorder final :
    public WaveClipStretchHandle::ClipStretchPolicy
{
   std::shared_ptr<WaveTrack> mTrack;
   std::vector<std::shared_ptr<WaveClip>> mClips;
   double mInitialBorderPosition {};
   int mDragStartX {};
   std::pair<double, double> mRange;
   bool mAdjustingLeftBorder;

   std::unique_ptr<SnapManager> mSnapManager;
   SnapResults mSnap;

   void StretchTo(double t)
   {
      if (mAdjustingLeftBorder)
      {
         for (auto& clip : mClips)
            clip->StretchLeftTo(t);
      }
      else
      {
         for (auto& clip : mClips)
            clip->StretchRightTo(t);
      }
   }

   // Search for a good snap points among all tracks, including
   // the one to which adjusted clip belongs to, but not counting
   // the borders of adjusted clip
   static SnapPointArray FindSnapPoints(
      const WaveTrack* currentTrack, WaveClip* adjustedClip,
      const std::pair<double, double> range)
   {
      SnapPointArray result;

      auto addSnapPoint = [&](double t, const Track* track) {
         if (t > range.second || t < range.first)
            return;

         for (const auto& snapPoint : result)
            if (snapPoint.t == t)
               return;
         result.emplace_back(t, track);
      };

      if (const auto trackList = currentTrack->GetOwner())
      {
         for (const auto track : trackList->Any())
         {
            const auto isSameTrack =
               (track == currentTrack) ||
               (track->GetLinkType() == Track::LinkType::Aligned &&
                *trackList->FindLeader(currentTrack) == track) ||
               (currentTrack->GetLinkType() == Track::LinkType::Aligned &&
                *trackList->FindLeader(track) == currentTrack);
            for (const auto& interval : track->GetIntervals())
            {
               if (isSameTrack)
               {
                  auto waveTrackIntervalData =
                     dynamic_cast<WaveTrack::IntervalData*>(interval.Extra());
                  if (waveTrackIntervalData->GetClip().get() == adjustedClip)
                     // exclude boundaries of the adjusted clip
                     continue;
               }
               addSnapPoint(interval.Start(), track);
               if (interval.Start() != interval.End())
                  addSnapPoint(interval.End(), track);
            }
         }
      }
      return result;
   }

public:
   AdjustBorder(
      const std::shared_ptr<WaveTrack>& track,
      const std::shared_ptr<WaveClip>& clip, bool leftBorder,
      const ZoomInfo& zoomInfo)
       : mTrack(track)
       , mAdjustingLeftBorder(leftBorder)
   {
      auto clips = track->GetClips();

      wxASSERT(std::find(clips.begin(), clips.end(), clip) != clips.end());

      if (
         track->IsAlignedWithLeader() ||
         track->GetLinkType() == Track::LinkType::Aligned)
         // find clips in other channels which are also should be trimmed
         mClips = FindClipsInChannels(
            clip->GetPlayStartTime(), clip->GetPlayEndTime(), track.get());
      else
         mClips.push_back(clip);

      if (mAdjustingLeftBorder)
      {
         auto left = clip->GetSequenceStartTime();
         for (const auto& other : clips)
            if (
               other->GetPlayStartTime() < clip->GetPlayStartTime() &&
               other->GetPlayEndTime() > left)
               left = other->GetPlayEndTime();
         // not less than 1 sample length
         mRange = std::make_pair(
            left, clip->GetPlayEndTime() - 1.0 / clip->GetRate());

         mInitialBorderPosition = mClips[0]->GetPlayStartTime();
      }
      else
      {
         auto right = clip->GetSequenceEndTime();
         for (const auto& other : clips)
            if (
               other->GetPlayStartTime() > clip->GetPlayStartTime() &&
               other->GetPlayStartTime() < right)
               right = other->GetPlayStartTime();
         // not less than 1 sample length
         mRange = std::make_pair(
            clip->GetPlayStartTime() + 1.0 / clip->GetRate(), right);

         mInitialBorderPosition = mClips[0]->GetPlayEndTime();
      }

      if (const auto trackList = track->GetOwner())
      {
         mSnapManager = std::make_unique<SnapManager>(
            *trackList->GetOwner(),
            FindSnapPoints(track.get(), clip.get(), mRange), zoomInfo);
      }
   }

   bool Init(const TrackPanelMouseEvent& event) override
   {
      if (event.event.LeftDown())
      {
         mDragStartX = event.event.GetX();
         return true;
      }
      return false;
   }

   UIHandle::Result
   Stretch(const TrackPanelMouseEvent& event, AudacityProject& project) override
   {
      const auto eventX = event.event.GetX();
      const auto dx = eventX - mDragStartX;

      const auto& viewInfo = ViewInfo::Get(project);

      const auto eventT = viewInfo.PositionToTime(
         viewInfo.TimeToPosition(mInitialBorderPosition, event.rect.x) + dx,
         event.rect.x);

      const auto offset =
         sampleCount(
            floor((eventT - mInitialBorderPosition) * mClips[0]->GetRate()))
            .as_double() /
         mClips[0]->GetRate();
      const auto t = mInitialBorderPosition + offset;
      const auto wasSnapped = mSnap.Snapped();
      if (mSnapManager)
         mSnap = mSnapManager->Snap(mTrack.get(), t, !mAdjustingLeftBorder);
      if (mSnap.Snapped())
      {
         StretchTo(mSnap.outTime);
         return RefreshCode::RefreshAll;
      }
      else
         StretchTo(t);
      // If there was a snap line, make sure it is removed
      // from the screen by redrawing whole TrackPanel
      return wasSnapped ? RefreshCode::RefreshAll : RefreshCode::RefreshCell;
   }

   void Finish(AudacityProject& project) override
   {
      if (mClips[0]->GetPlayStartTime() != mInitialBorderPosition)
      {
         if (mAdjustingLeftBorder)
         {
            auto dt =
               std::abs(mClips[0]->GetPlayStartTime() - mInitialBorderPosition);
            ProjectHistory::Get(project).PushState(
               XO("Clip-Trim-Left"), XO("Moved by %.02f").Format(dt),
               UndoPush::CONSOLIDATE);
         }
         else
         {
            auto dt =
               std::abs(mInitialBorderPosition - mClips[0]->GetPlayEndTime());
            ProjectHistory::Get(project).PushState(
               XO("Clip-Trim-Right"), XO("Moved by %.02f").Format(dt),
               UndoPush::CONSOLIDATE);
         }
      }
   }

   void Cancel() override
   {
      StretchTo(mInitialBorderPosition);
   }

   void Draw(
      TrackPanelDrawingContext& context, const wxRect& rect,
      unsigned iPass) override
   {
      if (iPass == TrackArtist::PassSnapping && mSnap.Snapped())
      {
         auto& dc = context.dc;
         SnapManager::Draw(&dc, rect.x + mSnap.outCoord, -1);
      }
   }

   wxRect DrawingArea(
      TrackPanelDrawingContext&, const wxRect& rect, const wxRect& panelRect,
      unsigned iPass) override
   {
      if (iPass == TrackArtist::PassSnapping)
         return MaximizeHeight(rect, panelRect);
      return rect;
   }
};

class WaveClipStretchHandle::AdjustBetweenBorders final :
    public WaveClipStretchHandle::ClipStretchPolicy
{
   std::pair<double, double> mRange;
   std::vector<std::shared_ptr<WaveClip>> mLeftClips;
   std::vector<std::shared_ptr<WaveClip>> mRightClips;
   double mInitialBorderPosition {};
   int mDragStartX {};

   void StretchTo(double t)
   {
      t = std::clamp(t, mRange.first, mRange.second);

      for (auto& clip : mLeftClips)
         clip->StretchRightTo(t);
      for (auto& clip : mRightClips)
         clip->StretchLeftTo(t);
   }

public:
   AdjustBetweenBorders(
      WaveTrack* track, std::shared_ptr<WaveClip>& leftClip,
      std::shared_ptr<WaveClip>& rightClip)
   {
      auto clips = track->GetClips();

      wxASSERT(std::find(clips.begin(), clips.end(), leftClip) != clips.end());
      wxASSERT(std::find(clips.begin(), clips.end(), rightClip) != clips.end());

      if (
         track->IsAlignedWithLeader() ||
         track->GetLinkType() == Track::LinkType::Aligned)
      {
         // find clips in other channels which are also should be trimmed
         mLeftClips = FindClipsInChannels(
            leftClip->GetPlayStartTime(), leftClip->GetPlayEndTime(), track);
         mRightClips = FindClipsInChannels(
            rightClip->GetPlayStartTime(), rightClip->GetPlayEndTime(), track);
      }
      else
      {
         mLeftClips.push_back(leftClip);
         mRightClips.push_back(rightClip);
      }

      mRange = std::make_pair(
         // not less than 1 sample length
         mLeftClips[0]->GetPlayStartTime() + 1.0 / mLeftClips[0]->GetRate(),
         mRightClips[0]->GetPlayEndTime() - 1.0 / mRightClips[0]->GetRate());
      mInitialBorderPosition = mRightClips[0]->GetPlayStartTime();
   }

   bool Init(const TrackPanelMouseEvent& event) override
   {
      if (event.event.LeftDown())
      {
         mDragStartX = event.event.GetX();
         return true;
      }
      return false;
   }

   UIHandle::Result
   Stretch(const TrackPanelMouseEvent& event, AudacityProject& project) override
   {
      const auto newX = event.event.GetX();
      const auto dx = newX - mDragStartX;

      auto& viewInfo = ViewInfo::Get(project);

      auto eventT = viewInfo.PositionToTime(
         viewInfo.TimeToPosition(mInitialBorderPosition, event.rect.x) + dx,
         event.rect.x);
      auto offset =
         sampleCount(
            floor((eventT - mInitialBorderPosition) * mLeftClips[0]->GetRate()))
            .as_double() /
         mLeftClips[0]->GetRate();

      StretchTo(mInitialBorderPosition + offset);

      return RefreshCode::RefreshCell;
   }

   void Finish(AudacityProject& project) override
   {
      if (mRightClips[0]->GetPlayStartTime() != mInitialBorderPosition)
      {
         auto dt = std::abs(
            mRightClips[0]->GetPlayStartTime() - mInitialBorderPosition);
         ProjectHistory::Get(project).PushState(
            XO("Clip-Trim-Between"), XO("Moved by %.02f").Format(dt),
            UndoPush::CONSOLIDATE);
      }
   }

   void Cancel() override
   {
      StretchTo(mInitialBorderPosition);
   }
};

HitTestPreview
WaveClipStretchHandle::HitPreview(const AudacityProject*, bool unsafe)
{
   static auto disabledCursor =
      ::MakeCursor(wxCURSOR_NO_ENTRY, DisabledCursorXpm, 16, 16);
   static auto slideCursor =
      MakeCursor(wxCURSOR_SIZEWE, StretchCursorXpm, 16, 16);
   auto message = XO("Click and drag to stretch audio");

   return { message, (unsafe ? &*disabledCursor : &*slideCursor) };
}

WaveClipStretchHandle::WaveClipStretchHandle(
   std::unique_ptr<ClipStretchPolicy>& clipStretchPolicy)
    : mClipStretchPolicy { std::move(clipStretchPolicy) }
{
}

void WaveClipStretchHandle::ClipStretchPolicy::Draw(
   TrackPanelDrawingContext&, const wxRect&, unsigned)
{
}

wxRect WaveClipStretchHandle::ClipStretchPolicy::DrawingArea(
   TrackPanelDrawingContext&, const wxRect& rect, const wxRect&, unsigned)
{
   return rect;
}

UIHandlePtr WaveClipStretchHandle::HitAnywhere(
   std::weak_ptr<WaveClipStretchHandle>& holder,
   const std::shared_ptr<WaveTrack>& waveTrack, const AudacityProject* pProject,
   const TrackPanelMouseState& state)
{
   if (!wxGetKeyState(WXK_ALT))
      return {};
   const auto rect = state.rect;

   auto px = state.state.m_x;

   auto& zoomInfo = ViewInfo::Get(*pProject);

   std::shared_ptr<WaveClip> leftClip;
   std::shared_ptr<WaveClip> rightClip;

   // Test left and right boundaries of each clip
   // to determine which type of trimming should be applied
   // and input for the policy
   for (auto& clip : waveTrack->GetClips())
   {
      if (!WaveTrackView::ClipDetailsVisible(*clip, zoomInfo, rect))
         continue;

      auto clipRect = ClipParameters::GetClipRect(*clip.get(), zoomInfo, rect);

      // double the hit testing area in case if clip are close to each other
      if (std::abs(px - clipRect.GetLeft()) <= BoundaryThreshold * 2)
         rightClip = clip;
      else if (std::abs(px - clipRect.GetRight()) <= BoundaryThreshold * 2)
         leftClip = clip;
   }

   std::unique_ptr<ClipStretchPolicy> clipStretchPolicy;
   if (leftClip && rightClip)
   {
      // between adjacent clips
      if (
         ClipParameters::GetClipRect(*leftClip, zoomInfo, rect).GetRight() > px)
         clipStretchPolicy = std::make_unique<AdjustBorder>(
            waveTrack, leftClip, false, zoomInfo); // right border
      else
         clipStretchPolicy = std::make_unique<AdjustBorder>(
            waveTrack, rightClip, true, zoomInfo); // left border
   }
   else
   {
      auto clip = leftClip ? leftClip : rightClip;
      if (clip)
      {
         // single clip case, determine the border,
         // hit testing area differs from one
         // used for general case
         auto clipRect =
            ClipParameters::GetClipRect(*clip.get(), zoomInfo, rect);
         if (std::abs(px - clipRect.GetLeft()) <= BoundaryThreshold)
            clipStretchPolicy =
               std::make_unique<AdjustBorder>(waveTrack, clip, true, zoomInfo);
         else if (std::abs(px - clipRect.GetRight()) <= BoundaryThreshold)
            clipStretchPolicy =
               std::make_unique<AdjustBorder>(waveTrack, clip, false, zoomInfo);
      }
   }

   if (clipStretchPolicy)
      return AssignUIHandlePtr(
         holder, std::make_shared<WaveClipStretchHandle>(clipStretchPolicy));
   return {};
}

UIHandlePtr WaveClipStretchHandle::HitTest(
   std::weak_ptr<WaveClipStretchHandle>& holder, WaveTrackView& view,
   const AudacityProject* pProject, const TrackPanelMouseState& state)
{
   if (!wxGetKeyState(WXK_ALT))
      return {};
   auto waveTrack = std::dynamic_pointer_cast<WaveTrack>(view.FindTrack());
   // For multichannel tracks, show trim handle only for the leader track
   if (!waveTrack->IsLeader() && waveTrack->IsAlignedWithLeader())
      return {};

   std::vector<UIHandlePtr> results;

   const auto rect = state.rect;

   auto px = state.state.m_x;
   auto py = state.state.m_y;

   if (
      py >= rect.GetTop() &&
      py <= (rect.GetTop() + static_cast<int>(rect.GetHeight() * 0.3)))
   {
      return HitAnywhere(holder, waveTrack, pProject, state);
   }
   return {};
}

HitTestPreview WaveClipStretchHandle::Preview(
   const TrackPanelMouseState& mouseState, AudacityProject* pProject)
{
   const bool unsafe = ProjectAudioIO::Get(*pProject).IsAudioActive();
   return HitPreview(pProject, unsafe);
}

UIHandle::Result WaveClipStretchHandle::Click(
   const TrackPanelMouseEvent& event, AudacityProject* pProject)
{
   if (!ProjectAudioIO::Get(*pProject).IsAudioActive())
   {
      if (mClipStretchPolicy->Init(event))
         return RefreshCode::RefreshNone;
   }
   return RefreshCode::Cancelled;
}

UIHandle::Result WaveClipStretchHandle::Drag(
   const TrackPanelMouseEvent& event, AudacityProject* project)
{
   return mClipStretchPolicy->Stretch(event, *project);
}

UIHandle::Result WaveClipStretchHandle::Release(
   const TrackPanelMouseEvent& event, AudacityProject* project,
   wxWindow* pParent)
{
   mClipStretchPolicy->Finish(*project);
   return RefreshCode::RefreshAll;
}

UIHandle::Result WaveClipStretchHandle::Cancel(AudacityProject* pProject)
{
   mClipStretchPolicy->Cancel();
   return RefreshCode::RefreshAll;
}

void WaveClipStretchHandle::Draw(
   TrackPanelDrawingContext& context, const wxRect& rect, unsigned iPass)
{
   mClipStretchPolicy->Draw(context, rect, iPass);
}

wxRect WaveClipStretchHandle::DrawingArea(
   TrackPanelDrawingContext& context, const wxRect& rect,
   const wxRect& panelRect, unsigned iPass)
{
   return mClipStretchPolicy->DrawingArea(context, rect, panelRect, iPass);
}
