#pragma once

#include <JuceHeader.h>
#include "VJGLSLTransition.h"

//==============================================================================
/// Blends between two canvas snapshot images with a configurable transition effect.
class VJTransitionEngine
{
public:
    enum class Type { Cut, Crossfade, WipeLeft, WipeRight, WipeUp, WipeDown, ZoomBlend, CustomGLSL };

    VJTransitionEngine() = default;

    //==========================================================================
    /// Begin a transition from the current 'from' image to a new 'toImage'.
    /// Pass durationMs <= 0 for an instant Cut.
    void startTransition(Type type, juce::Image fromImage, juce::Image toImage, int durationMs = 500);

    /// Update the transition (call from a timer, e.g. 60Hz). Returns true while active.
    bool tick();

    /// Draw the current transition frame into 'g' at 'bounds'.
    void paint(juce::Graphics& g, juce::Rectangle<int> bounds) const;

    /// True while a transition is in progress.
    bool isActive() const noexcept { return active_; }

    /// 0.0 (just started) → 1.0 (complete)
    float getProgress() const noexcept { return progress_; }

    //==========================================================================
    void setType(Type t)        { nextType_ = t; }
    void setDuration(int ms)    { durationMs_ = juce::jmax(0, ms); }
    Type getType()    const     { return nextType_; }
    int  getDuration() const    { return durationMs_; }

    /// Called by VJEditor to supply the current rendered canvas frame
    /// (used as the 'to' image when a transition completes or as the live feed).
    void setLiveFrame(const juce::Image& frame) { liveFrame_ = frame; }
    const juce::Image& getLiveFrame() const     { return liveFrame_; }

    /// Set / get the custom GLSL source for the transition.
    void setCustomGLSL(const juce::String& source);
    VJGLSLTransition* getGLSLTransition() { return glslTransition_.get(); }
    const juce::String& getGLSLSource() const;

private:
    Type       currentType_  = Type::Crossfade;
    Type       nextType_     = Type::Crossfade;
    int        durationMs_   = 500;
    bool       active_       = false;
    float      progress_     = 1.0f;
    juce::int64 startMs_     = 0;

    juce::Image fromImage_;
    juce::Image toImage_;
    juce::Image liveFrame_;

    std::unique_ptr<VJGLSLTransition> glslTransition_;
};
