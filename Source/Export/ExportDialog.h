#pragma once

#include <JuceHeader.h>
#include "ExportSettings.h"

//==============================================================================
/// Modal dialog for configuring video export settings.
/// Once the user clicks "Export", onExport is invoked with the settings.
class ExportDialog : public juce::Component
{
public:
    ExportDialog(const Export::Settings& initialSettings,
                 const juce::File& currentAudioFile);
    ~ExportDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /// Callback fired when the user clicks Export.
    std::function<void(const Export::Settings&)> onExport;

    /// Collect current UI state into an Export::Settings struct.
    Export::Settings getSettings() const;

    /// Recommended dialog size.
    static constexpr int kWidth  = 500;
    static constexpr int kHeight = 680;

private:
    // Video
    juce::ComboBox  resolutionCombo_;
    juce::Label     resolutionLabel_;
    juce::TextEditor customWEdit_, customHEdit_;
    juce::Label      customSizeLabel_;

    juce::ComboBox  fpsCombo_;
    juce::Label     fpsLabel_;

    // Codec
    juce::ComboBox  codecCombo_;
    juce::Label     codecLabel_;

    // Quality
    juce::ComboBox  qualityCombo_;
    juce::Label     qualityLabel_;
    juce::Slider    bitrateSlider_;
    juce::Label     bitrateLabel_;

    // Audio
    juce::ComboBox  audioCodecCombo_;
    juce::Label     audioCodecLabel_;
    juce::Slider    audioBitrateSlider_;
    juce::Label     audioBitrateLabel_;

    // Output
    juce::TextEditor outputPathEdit_;
    juce::TextButton browseButton_  { "Browse..." };
    juce::Label      outputLabel_;

    // ── Post-processing ──
    juce::ToggleButton caToggle_        { "Chromatic Aberration" };
    juce::Slider       caSlider_;
    juce::Label        caLabel_;

    juce::ToggleButton vignetteToggle_  { "Vignette" };
    juce::Slider       vigIntSlider_;    ///< intensity
    juce::Label        vigIntLabel_;
    juce::Slider       vigRadSlider_;    ///< radius
    juce::Label        vigRadLabel_;

    juce::ToggleButton shakeToggle_     { "Screen Shake" };
    juce::Slider       shakeSlider_;
    juce::Label        shakeLabel_;
    juce::ToggleButton shakeBeatToggle_ { "Beat Sync" };

    juce::ToggleButton beatZoomToggle_  { "Beat-Sync Zoom" };
    juce::Slider       beatZoomSlider_;
    juce::Label        beatZoomLabel_;

    // Buttons
    juce::TextButton exportButton_  { "Export" };
    juce::TextButton cancelButton_  { "Cancel" };

    // FFmpeg status
    juce::Label      ffmpegStatusLabel_;

    // Scrollable content
    juce::Viewport   viewport_;
    juce::Component  content_;
    juce::Label      videoHeader_, audioHeader_, outputHeader_, effectsHeader_;

    // State
    Export::Settings settings_;
    juce::File       audioFile_;

    void layoutContent();
    void updateCustomSizeVisibility();
    void updateFileExtension();
    void checkFFmpeg();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportDialog)
};
