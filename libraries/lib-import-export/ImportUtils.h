/**********************************************************************

  Audacity: A Digital Audio Editor

  ImportUtils.h

  Dominic Mazzoni

  Vitaly Sverchinsky split from ImportPlugin.h

**********************************************************************/

#pragma once

#include <memory>
#include <vector>

#include "ClipAnalysis.h"
#include "Import.h"
#include "SampleFormat.h"
#include "Internat.h"
#include "Track.h"

class wxString;

class TrackList;
class WaveTrackFactory;
class WaveTrack;
class WaveChannel;

class IMPORT_EXPORT_API ImportUtils final
{
public:

   //! Choose appropriate format, which will not be narrower than the specified one
   static sampleFormat ChooseFormat(sampleFormat effectiveFormat);

   //! Builds a wave track and places it into a track list.
   //! The format will not be narrower than the specified one.
   static TrackListHolder NewWaveTrack(WaveTrackFactory &trackFactory, unsigned nChannels,
      sampleFormat effectiveFormat, double rate);

   static void ShowMessageBox(const TranslatableString& message, const TranslatableString& caption = XO("Import Project"));

   //! Iterates over channels in each wave track from the list
   static
   void ForEachChannel(TrackList& trackList, const std::function<void(WaveChannel&)>& op);

   //! Flushes the given channels and moves them to \p outTracks
   static std::optional<ClipAnalysis::MeterInfo> FinalizeImport(
      TrackHolders& outTracks,
      const std::vector<TrackListHolder>& importedStreams,
      const std::optional<double>& tempoHint = std::nullopt);

   //! Flushes the given channels and moves them to \p outTracks
   static std::optional<ClipAnalysis::MeterInfo> FinalizeImport(
      TrackHolders& outTracks, TrackListHolder trackList,
      const std::optional<double>& tempoHint = std::nullopt);
};
