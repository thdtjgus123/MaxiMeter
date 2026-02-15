#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <random>

namespace Export
{

//==============================================================================
/// Post-processing settings attached to an export job.
struct PostProcessSettings
{
    // ── Chromatic Aberration ──
    bool   chromaticAberration = false;
    float  caIntensity         = 3.0f;   ///< pixel offset for R/B channels (0–20)

    // ── Vignette ──
    bool   vignette            = false;
    float  vignetteIntensity   = 0.5f;   ///< darkness at corners (0–1)
    float  vignetteRadius      = 0.8f;   ///< inner clear radius (0–1, 1 = edge of frame)

    // ── Screen Shake ──
    bool   screenShake         = false;
    float  shakeIntensity      = 5.0f;   ///< max offset in pixels (0–50)
    bool   shakeBeatSync       = true;   ///< tie shake amplitude to audio level

    // ── Beat-Sync Zoom ──
    bool   beatZoom            = false;
    float  beatZoomAmount      = 0.05f;  ///< max zoom increase (0–0.3, e.g. 0.05 = 5%)
    float  beatZoomDecay       = 0.85f;  ///< zoom decay factor per frame (0–1)

    bool hasAnyEffect() const
    {
        return chromaticAberration || vignette || screenShake || beatZoom;
    }
};

//==============================================================================
/// Applies post-processing effects to rendered frames.
/// Created once per export job; call `processFrame()` per frame.
class PostProcessor
{
public:
    PostProcessor(const PostProcessSettings& settings, int width, int height, int fps)
        : settings_(settings), width_(width), height_(height), fps_(fps),
          rng_(42)  // fixed seed for reproducible exports
    {
    }

    /// Supply current audio RMS level (0–1 linear) for beat-reactive effects.
    void setCurrentRMS(float rms) { currentRMS_ = juce::jlimit(0.0f, 1.0f, rms); }

    /// Apply all enabled effects to the image (modifies in-place).
    void processFrame(juce::Image& image)
    {
        if (!settings_.hasAnyEffect()) return;

        if (settings_.beatZoom)
            applyBeatZoom(image);

        if (settings_.screenShake)
            applyScreenShake(image);

        if (settings_.chromaticAberration)
            applyChromaticAberration(image);

        if (settings_.vignette)
            applyVignette(image);
    }

private:
    PostProcessSettings settings_;
    int width_, height_, fps_;
    float currentRMS_ = 0.0f;
    float zoomLevel_  = 0.0f;   ///< current beat-zoom level (decays per frame)
    std::mt19937 rng_;

    //==========================================================================
    void applyChromaticAberration(juce::Image& image)
    {
        const float intensity = settings_.caIntensity;
        if (intensity < 0.5f) return;

        // Scale offset by RMS if beat-sync shake is on
        float offset = intensity;
        if (settings_.shakeBeatSync)
            offset *= (0.3f + currentRMS_ * 0.7f);

        auto src = image.createCopy();  // snapshot
        juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(image, juce::Image::BitmapData::readWrite);

        const int w = image.getWidth();
        const int h = image.getHeight();
        const int off = static_cast<int>(std::round(offset));

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                // Red channel shifted left
                int rxR = juce::jlimit(0, w - 1, x - off);
                auto srcPixelR = srcData.getPixelColour(rxR, y);

                // Blue channel shifted right
                int rxB = juce::jlimit(0, w - 1, x + off);
                auto srcPixelB = srcData.getPixelColour(rxB, y);

                // Green channel stays centered
                auto srcPixelG = srcData.getPixelColour(x, y);

                dstData.setPixelColour(x, y,
                    juce::Colour(srcPixelR.getRed(),
                                 srcPixelG.getGreen(),
                                 srcPixelB.getBlue(),
                                 srcPixelG.getAlpha()));
            }
        }
    }

    //==========================================================================
    void applyVignette(juce::Image& image)
    {
        const int w = image.getWidth();
        const int h = image.getHeight();
        const float cx = w * 0.5f;
        const float cy = h * 0.5f;
        const float maxDist = std::sqrt(cx * cx + cy * cy);
        const float innerR = settings_.vignetteRadius * maxDist;
        const float outerR = maxDist;
        const float intensity = settings_.vignetteIntensity;

        juce::Image::BitmapData data(image, juce::Image::BitmapData::readWrite);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                float dx = x - cx;
                float dy = y - cy;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist > innerR)
                {
                    float t = juce::jlimit(0.0f, 1.0f, (dist - innerR) / (outerR - innerR));
                    float darken = 1.0f - (t * t * intensity);  // quadratic falloff
                    auto col = data.getPixelColour(x, y);
                    data.setPixelColour(x, y,
                        juce::Colour(
                            static_cast<uint8_t>(col.getRed()   * darken),
                            static_cast<uint8_t>(col.getGreen() * darken),
                            static_cast<uint8_t>(col.getBlue()  * darken),
                            col.getAlpha()));
                }
            }
        }
    }

    //==========================================================================
    void applyScreenShake(juce::Image& image)
    {
        float amp = settings_.shakeIntensity;
        if (settings_.shakeBeatSync)
            amp *= currentRMS_;

        if (amp < 0.5f) return;

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        int dx = static_cast<int>(dist(rng_) * amp);
        int dy = static_cast<int>(dist(rng_) * amp);

        if (dx == 0 && dy == 0) return;

        auto src = image.createCopy();
        juce::Graphics g(image);
        g.fillAll(juce::Colours::black);
        g.drawImageAt(src, dx, dy);
    }

    //==========================================================================
    void applyBeatZoom(juce::Image& image)
    {
        // Drive zoom from RMS — peak-detect with decay
        float target = currentRMS_ * settings_.beatZoomAmount;
        zoomLevel_ = std::max(target, zoomLevel_ * settings_.beatZoomDecay);

        if (zoomLevel_ < 0.001f) return;

        float scale = 1.0f + zoomLevel_;
        int w = image.getWidth();
        int h = image.getHeight();
        int newW = static_cast<int>(w * scale);
        int newH = static_cast<int>(h * scale);

        // Upscale into a temp image
        auto zoomed = image.rescaled(newW, newH, juce::Graphics::highResamplingQuality);

        // Crop center region back to original size
        int cropX = (newW - w) / 2;
        int cropY = (newH - h) / 2;

        auto cropped = zoomed.getClippedImage(juce::Rectangle<int>(cropX, cropY, w, h));

        // Copy back
        juce::Graphics g(image);
        g.drawImageAt(cropped, 0, 0);
    }
};

} // namespace Export
