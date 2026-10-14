#include "clipfades.h"

#include "au3-xml/XMLAttributeValueView.h"
#include "au3-xml/XMLWriter.h"

namespace au::au3 {
static constexpr auto FadeIn_attr = "fadeIn";
static constexpr auto FadeOut_attr = "fadeOut";

static const WaveClip::Attachments::RegisteredFactory keyClipFades{
    [](WaveClip& clip) { return std::make_unique<ClipFades>(clip); }
};

ClipFades::ClipFades(WaveClip& clip)
    : m_clip{clip}
{
}

ClipFades& ClipFades::Get(WaveClip& clip)
{
    return clip.Attachments::Get<ClipFades>(keyClipFades);
}

const ClipFades& ClipFades::Get(const WaveClip& clip)
{
    return Get(const_cast<WaveClip&>(clip));
}

void ClipFades::MarkChanged() noexcept
{
}

void ClipFades::Invalidate()
{
}

std::unique_ptr<WaveClipListener> ClipFades::Clone() const
{
    return std::make_unique<ClipFades>(*this);
}

void ClipFades::WriteXMLAttributes(XMLWriter& writer) const
{
    writer.WriteAttr(FadeIn_attr, m_fadeIn, 8);
    writer.WriteAttr(FadeOut_attr, m_fadeOut, 8);
}

bool ClipFades::HandleXMLAttribute(const std::string_view& attr, const XMLAttributeValueView& valueView)
{
    if (attr == FadeIn_attr) {
        return valueView.TryGet(m_fadeIn);
    }
    if (attr == FadeOut_attr) {
        return valueView.TryGet(m_fadeOut);
    }
    return false;
}

namespace {
/**
 * @brief When the clip is trimmed down to the point that it's not long enough to accommodate both fades,
 * some choice must be made. There are several strategies, here are a the current candidates.
 */
enum class ClampStrategy {
    Fair, // Relative fade durations are preserved, e.g., if fade-in is 1s, fade-out is 2 and the clip is trimmed down to 1s, fade-in becomes 1/3 and fade-out, 2/3.
    Reaper, // Preserve fade-in to the detriment of fade out
    // A third one could be `Vegas`, which preserves the dragged fade to the detriment of the other.
    // It'd require more code, though, because the last dragged fade would have to be remembered.
};

// Not sure yet which one will make it, or whether it'll be something configurable. For now the one from the design.
constexpr ClampStrategy strategy = ClampStrategy::Fair;
}

double ClipFades::fadeIn() const
{
    const auto duration = m_clip.GetPlayDuration();
    switch (strategy) {
    case ClampStrategy::Fair:
        return std::clamp(m_fadeIn, 0.0, m_fadeIn / (m_fadeIn + m_fadeOut) * duration);
    case ClampStrategy::Reaper:
        return std::clamp(m_fadeIn, 0.0, duration);
    default:
        assert(false);
        return 0.0;
    }
}

void ClipFades::setFadeIn(double value)
{
    m_fadeIn = value;
}

double ClipFades::fadeOut() const
{
    const double duration = m_clip.GetPlayDuration();
    switch (strategy) {
    case ClampStrategy::Fair:
        return std::clamp(m_fadeOut, 0.0, m_fadeOut / (m_fadeIn + m_fadeOut) * duration);
    case ClampStrategy::Reaper:
        return std::clamp(m_fadeOut, 0.0, duration - m_fadeIn);
    default:
        assert(false);
        return 0.0;
    }
}

void ClipFades::setFadeOut(double value)
{
    m_fadeOut = value;
}
} // namespace au::au3
