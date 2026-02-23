#pragma once

#include <JuceHeader.h>
#include "ThreeDWebView.h"
#include "ThreeDExportProgressWindow.h"
#include "../Export/ExportSettings.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/FFTProcessor.h"
#include "../UI/ThemeManager.h"

//==============================================================================
/// 3D workflow editor: embeds Three.js + Monaco shader editor as a WebView.
/// Audio data is forwarded to the JS scene at ~30 Hz via the JUCE event bridge.
class ThreeDEditor : public juce::Component,
                     public juce::Timer,
                     public ThemeManager::Listener
{
public:
    explicit ThreeDEditor (AudioEngine& audioEngine, FFTProcessor& fftProcessor);
    ~ThreeDEditor() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    void timerCallback() override;

    /// Forward audio data from MainComponent's timer.
    void timerTick();

    /// Capture the Three.js canvas, show a preview window with PNG/JPEG export.
    void showRenderPreview3D();

    /// Launch 3D video export using the given compiled settings.
    void startVideoExport (const Export::Settings& settings);

private:
    AudioEngine&  audioEngine_;
    FFTProcessor& fftProcessor_;

    ThreeDWebView webView_;

    void themeChanged (AppTheme theme) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeDEditor)
};
