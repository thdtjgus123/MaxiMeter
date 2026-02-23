#pragma once

#include <JuceHeader.h>
#include "../UI/MeterBase.h"

//==============================================================================
/// A built-in canvas component that reflects the canvas content positioned
/// directly above it, with animated water ripple distortion.
///
/// It periodically captures a snapshot of the parent's area above its top edge,
/// flips it vertically, applies sinusoidal pixel displacement, and draws the
/// result with a blue/cyan tint and a fade-out gradient.
class WaterReflectionComponent : public juce::Component,
                                  public MeterBase,
                                  private juce::Timer
{
public:
    WaterReflectionComponent();
    ~WaterReflectionComponent() override;

    //==========================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;

    //==========================================================================
    // All setters invalidate the next frame automatically.

    /// Ripple animation speed multiplier (0.1 .. 3.0).
    void setSpeed(float v)             { speed_           = v; }
    float getSpeed() const             { return speed_; }

    /// Wave distortion amount (0 = still mirror, 1 = maximum ripple).
    void setIntensity(float v)         { intensity_       = juce::jlimit(0.0f, 1.0f, v); }
    float getIntensity() const         { return intensity_; }

    /// Blur radius applied to the reflection (0.5 .. 6.0).
    void setBlurRadius(float v)        { blurRadius_      = juce::jmax(0.5f, v); }
    float getBlurRadius() const        { return blurRadius_; }

    /// Wave frequency scale – higher = tighter ripples (0.2 .. 3.0).
    void setWaveScale(float v)         { waveScale_       = juce::jmax(0.1f, v); }
    float getWaveScale() const         { return waveScale_; }

    /// Colour desaturation of the reflected image (0 = full colour, 1 = greyscale).
    void setDesaturation(float v)      { desaturation_    = juce::jlimit(0.0f, 1.0f, v); }
    float getDesaturation() const      { return desaturation_; }

    /// Opacity of the white-blue surface mist at the top edge (0 .. 1).
    void setMistOpacity(float v)       { mistOpacity_     = juce::jlimit(0.0f, 1.0f, v); }
    float getMistOpacity() const       { return mistOpacity_; }

    /// Number of specular shimmer lines drawn across the surface (0 .. 12).
    void setShimmerCount(int v)        { shimmerCount_    = juce::jlimit(0, 12, v); }
    int  getShimmerCount() const       { return shimmerCount_; }

    /// Overall opacity of the distorted reflection layer (0 .. 1).
    void setReflectOpacity(float v)    { reflectOpacity_  = juce::jlimit(0.0f, 1.0f, v); }
    float getReflectOpacity() const    { return reflectOpacity_; }

    /// Depth fade strength — how aggressively the bottom of the reflection darkens (0 .. 1).
    void setDepthFade(float v)         { depthFade_       = juce::jlimit(0.0f, 1.0f, v); }
    float getDepthFade() const         { return depthFade_; }

    /// Perspective strength: 0 = flat rectangle, higher = bottom edge zoomed in
    /// (top always fills bounding box width; bottom converges inward as value increases).
    void setPerspective(float v)        { perspective_     = juce::jmax(0.0f, v); }
    float getPerspective() const        { return perspective_; }

    /// Water surface tint colour (alpha controls tint strength).
    void setTintColour(juce::Colour c) { tintColour_      = c; }
    juce::Colour getTintColour() const { return tintColour_; }

private:
    //==========================================================================
    void timerCallback() override;

    /// Capture a snapshot from the parent component for the area above us.
    void captureAbove();

    /// Apply sinusoidal water-distortion to @p src — uses member parameters.
    void applyWaveDistortion(const juce::Image& src, juce::Image& dst, float timeSeconds);

    /// Apply perspective warp: top always fills full width, bottom zooms by perspectiveStrength_.
    void applyPerspective(const juce::Image& src, juce::Image& dst);

    /// Box blur approximating Gaussian.
    static void applyBlur(const juce::Image& src, juce::Image& dst, float radius);

    /// Desaturate + cool-shift + depth-fade a pixel.
    static juce::PixelARGB waterColour(juce::PixelARGB in, float depthFade,
                                        float desat, float depthFadeStr);

    //==========================================================================
    float speed_           = 1.0f;
    float intensity_       = 0.6f;
    float blurRadius_      = 1.5f;
    float waveScale_       = 1.0f;
    float desaturation_    = 0.55f;
    float mistOpacity_     = 0.8f;
    int   shimmerCount_    = 6;
    float reflectOpacity_  = 0.9f;
    float depthFade_       = 0.85f;
    float perspective_     = 2.0f;   ///< 0=flat, higher=more perspective at bottom

    juce::Colour tintColour_ { 0x55304050 };

    juce::Image capturedAbove_;   ///< raw (flipped) snapshot of what's above
    juce::Image blurred_;         ///< horizontally blurred reflection
    juce::Image distorted_;       ///< wave-distorted version ready to paint

    double startTime_ = 0.0;
    bool   captureScheduled_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaterReflectionComponent)
};
