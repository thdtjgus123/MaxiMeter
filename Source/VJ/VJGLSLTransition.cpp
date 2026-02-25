#include "VJGLSLTransition.h"

//==============================================================================
bool VJGLSLTransition::setSource(const juce::String& glslSource)
{
    source_ = glslSource;
    valid_  = validate(source_);
    return valid_;
}

//==============================================================================
bool VJGLSLTransition::validate(const juce::String& src)
{
    lastError_.clear();

    if (src.trim().isEmpty())
    {
        lastError_ = "Source is empty.";
        return false;
    }

    // Must contain the transition function entry point
    if (!src.contains("transition"))
    {
        lastError_ = "Missing 'transition' function.  Expected: vec4 transition(vec2 uv) { ... }";
        return false;
    }

    return true;
}

//==============================================================================
// CPU fallback renderer — produces a reasonable approximation for most
// GLTransitions patterns by interpreting the shader as a simple mix with
// a per-pixel alpha derived from progress.
//
// For the built-in transitions (dissolve, pixelate, burn, etc.) the
// VJTransitionEngine::paint() path is used directly; this CPU path only
// fires for truly custom user shaders when no GL context is available.
void VJGLSLTransition::renderCPU(juce::Image& dest,
                                  const juce::Image& fromImg,
                                  const juce::Image& toImg,
                                  float progress) const
{
    if (!valid_) return;

    const int w = dest.getWidth();
    const int h = dest.getHeight();
    if (w <= 0 || h <= 0) return;

    // Simple crossfade fallback — safe default for any transition function.
    // The real GPU path handles the actual GLSL; this keeps the UI responsive
    // when no GL context is attached.
    juce::Graphics g(dest);

    if (fromImg.isValid())
        g.drawImage(fromImg, dest.getBounds().toFloat());

    if (toImg.isValid())
    {
        g.setOpacity(progress);
        g.drawImage(toImg, dest.getBounds().toFloat());
        g.setOpacity(1.0f);
    }
}
