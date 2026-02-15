#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"

//==============================================================================
/// SplashOverlay — black screen with ico.png that fades in then fades out.
///
/// Timeline (total ~2.5s):
///   0..800ms    — fade in  (alpha 0→1)
///   800..1700ms — hold     (alpha 1)
///   1700..2500ms — fade out (alpha 1→0)
///
/// After fade-out completes the component is removed by its parent
/// (checked via isFinished()).
class SplashOverlay : public juce::Component,
                      public juce::Timer
{
public:
    SplashOverlay()
    {
        setOpaque(true);
        setAlwaysOnTop(true);

        // Load icon from binary data
        iconImage_ = juce::ImageFileFormat::loadFrom(
            BinaryData::ico_png, BinaryData::ico_pngSize);

        startTime_ = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60);
    }

    ~SplashOverlay() override { stopTimer(); }

    bool isFinished() const { return finished_; }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        if (!iconImage_.isValid())
            return;

        // Compute alpha based on elapsed time
        double elapsed = juce::Time::getMillisecondCounterHiRes() - startTime_;
        float alpha = 0.0f;

        if (elapsed < fadeInMs_)
        {
            alpha = static_cast<float>(elapsed / fadeInMs_);
        }
        else if (elapsed < fadeInMs_ + holdMs_)
        {
            alpha = 1.0f;
        }
        else if (elapsed < totalMs_)
        {
            double fadeElapsed = elapsed - fadeInMs_ - holdMs_;
            alpha = 1.0f - static_cast<float>(fadeElapsed / fadeOutMs_);
        }
        else
        {
            alpha = 0.0f;
            finished_ = true;
        }

        alpha = juce::jlimit(0.0f, 1.0f, alpha);

        // Draw icon centred, scaled to fit nicely (max 256px or half the component size)
        float maxDim = juce::jmin(256.0f,
                                  getWidth() * 0.3f,
                                  getHeight() * 0.5f);
        float imgW = static_cast<float>(iconImage_.getWidth());
        float imgH = static_cast<float>(iconImage_.getHeight());
        float scale = juce::jmin(maxDim / imgW, maxDim / imgH);

        float drawW = imgW * scale;
        float drawH = imgH * scale;
        float drawX = (getWidth() - drawW) * 0.5f;
        float drawY = (getHeight() - drawH) * 0.5f;

        g.setOpacity(alpha);
        g.drawImage(iconImage_,
                    juce::Rectangle<float>(drawX, drawY, drawW, drawH));
    }

    void timerCallback() override
    {
        repaint();
        if (finished_)
            stopTimer();
    }

private:
    juce::Image iconImage_;
    double startTime_ = 0.0;
    bool finished_ = false;

    static constexpr double fadeInMs_  = 800.0;
    static constexpr double holdMs_    = 900.0;
    static constexpr double fadeOutMs_ = 800.0;
    static constexpr double totalMs_   = fadeInMs_ + holdMs_ + fadeOutMs_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplashOverlay)
};
