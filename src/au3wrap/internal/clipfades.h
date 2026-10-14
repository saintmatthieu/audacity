#pragma once

#include "au3-wave-track/WaveClip.h"

class WaveClip;

namespace au::au3 {
//! Fade lengths of a clip, in seconds of play time
//! As a clip attachment, it is copied along with the clip (split, paste, undo history)
//! and saved as attributes of the clip in the project file.
class ClipFades final : public WaveClipListener
{
public:
    explicit ClipFades(WaveClip& clip);

    static ClipFades& Get(WaveClip& clip);
    static const ClipFades& Get(const WaveClip& clip);

    void MarkChanged() noexcept override;
    void Invalidate() override;
    std::unique_ptr<WaveClipListener> Clone() const override;

    void WriteXMLAttributes(XMLWriter& writer) const override;
    bool HandleXMLAttribute(const std::string_view& attr, const XMLAttributeValueView& valueView) override;

    double fadeIn() const;
    void setFadeIn(double);

    double fadeOut() const;
    void setFadeOut(double);

private:
    WaveClip& m_clip;

    double m_fadeIn = 0.0;
    double m_fadeOut = 0.0;
};
}
