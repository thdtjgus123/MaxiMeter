#include "ExportDialog.h"
#include "FFmpegProcess.h"
#include "../UI/ThemeManager.h"

//==============================================================================
ExportDialog::ExportDialog(const Export::Settings& initial,
                           const juce::File& currentAudioFile)
    : settings_(initial), audioFile_(currentAudioFile)
{
    setSize(kWidth, kHeight);

    // --- Resolution ---
    addAndMakeVisible(resolutionLabel_);
    resolutionLabel_.setText("Resolution:", juce::dontSendNotification);
    resolutionLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(resolutionCombo_);
    resolutionCombo_.addItem(Export::resolutionName(Export::Resolution::HD_1080p),      1);
    resolutionCombo_.addItem(Export::resolutionName(Export::Resolution::QHD_1440p),     2);
    resolutionCombo_.addItem(Export::resolutionName(Export::Resolution::UHD_4K),        3);
    resolutionCombo_.addItem(Export::resolutionName(Export::Resolution::Vertical_1080), 4);
    resolutionCombo_.addItem("Custom", 5);
    // Select based on initial settings
    int resId = (int)settings_.resolution + 1;
    if (resId < 1 || resId > 5) resId = 1;
    resolutionCombo_.setSelectedId(resId, juce::dontSendNotification);
    resolutionCombo_.onChange = [this]() { updateCustomSizeVisibility(); };

    addAndMakeVisible(customSizeLabel_);
    customSizeLabel_.setText("W x H:", juce::dontSendNotification);
    customSizeLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(customWEdit_);
    customWEdit_.setText("1920");
    customWEdit_.setInputRestrictions(5, "0123456789");

    addAndMakeVisible(customHEdit_);
    customHEdit_.setText("1080");
    customHEdit_.setInputRestrictions(5, "0123456789");

    customSizeLabel_.setVisible(false);
    customWEdit_.setVisible(false);
    customHEdit_.setVisible(false);

    // --- Frame rate ---
    addAndMakeVisible(fpsLabel_);
    fpsLabel_.setText("Frame Rate:", juce::dontSendNotification);
    fpsLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(fpsCombo_);
    fpsCombo_.addItem("30 fps", 1);
    fpsCombo_.addItem("60 fps", 2);
    // Select based on initial settings
    int fpsId = (settings_.frameRate == Export::FrameRate::FPS_60) ? 2 : 1;
    fpsCombo_.setSelectedId(fpsId, juce::dontSendNotification);

    // --- Video Codec ---
    addAndMakeVisible(codecLabel_);
    codecLabel_.setText("Video Codec:", juce::dontSendNotification);
    codecLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(codecCombo_);
    codecCombo_.addItem(Export::videoCodecName(Export::VideoCodec::H264_MP4),     1);
    codecCombo_.addItem(Export::videoCodecName(Export::VideoCodec::H265_MP4),     2);
    codecCombo_.addItem(Export::videoCodecName(Export::VideoCodec::VP9_WebM),     3);
    codecCombo_.addItem(Export::videoCodecName(Export::VideoCodec::ProRes_MOV),   4);
    codecCombo_.addItem(Export::videoCodecName(Export::VideoCodec::PNG_Sequence), 5);
    codecCombo_.setSelectedId(1, juce::dontSendNotification);
    codecCombo_.onChange = [this]() { updateFileExtension(); };

    // --- Quality ---
    addAndMakeVisible(qualityLabel_);
    qualityLabel_.setText("Quality:", juce::dontSendNotification);
    qualityLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(qualityCombo_);
    qualityCombo_.addItem("Draft (fast)", 1);
    qualityCombo_.addItem("Good",         2);
    qualityCombo_.addItem("Best (slow)",  3);
    qualityCombo_.setSelectedId(2, juce::dontSendNotification);

    addAndMakeVisible(bitrateLabel_);
    bitrateLabel_.setText("Bitrate (Mbps):", juce::dontSendNotification);
    bitrateLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(bitrateSlider_);
    bitrateSlider_.setRange(1.0, 100.0, 1.0);
    bitrateSlider_.setValue(20.0);
    bitrateSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 24);

    // --- Audio ---
    addAndMakeVisible(audioCodecLabel_);
    audioCodecLabel_.setText("Audio Codec:", juce::dontSendNotification);
    audioCodecLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(audioCodecCombo_);
    audioCodecCombo_.addItem(Export::audioCodecName(Export::AudioCodec::Passthrough), 1);
    audioCodecCombo_.addItem(Export::audioCodecName(Export::AudioCodec::AAC),         2);
    audioCodecCombo_.addItem(Export::audioCodecName(Export::AudioCodec::MP3),         3);
    audioCodecCombo_.addItem(Export::audioCodecName(Export::AudioCodec::Opus),        4);
    audioCodecCombo_.addItem(Export::audioCodecName(Export::AudioCodec::FLAC),        5);
    audioCodecCombo_.setSelectedId(2, juce::dontSendNotification);

    addAndMakeVisible(audioBitrateLabel_);
    audioBitrateLabel_.setText("Audio Bitrate (kbps):", juce::dontSendNotification);
    audioBitrateLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(audioBitrateSlider_);
    audioBitrateSlider_.setRange(64, 320, 32);
    audioBitrateSlider_.setValue(192);
    audioBitrateSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 24);

    // --- Output ---
    addAndMakeVisible(outputLabel_);
    outputLabel_.setText("Output:", juce::dontSendNotification);
    outputLabel_.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(outputPathEdit_);
    // Default output path: same directory as audio, with .mp4 extension
    if (audioFile_.existsAsFile())
    {
        auto defaultOut = audioFile_.getParentDirectory()
            .getChildFile(audioFile_.getFileNameWithoutExtension() + ".mp4");
        outputPathEdit_.setText(defaultOut.getFullPathName());
    }

    addAndMakeVisible(browseButton_);
    browseButton_.onClick = [this]()
    {
        auto codec = static_cast<Export::VideoCodec>(codecCombo_.getSelectedId() - 1);
        juce::String ext = Export::videoCodecExtension(codec);

        if (codec == Export::VideoCodec::PNG_Sequence)
        {
            auto chooser = std::make_shared<juce::FileChooser>("Choose output folder");
            chooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectDirectories,
                [this, chooser](const juce::FileChooser& fc)
                {
                    if (fc.getResults().size() > 0)
                        outputPathEdit_.setText(fc.getResult().getFullPathName());
                });
        }
        else
        {
            juce::String wildcard = "*" + ext;
            auto chooser = std::make_shared<juce::FileChooser>(
                "Save video as...", juce::File(outputPathEdit_.getText()), wildcard);
            chooser->launchAsync(juce::FileBrowserComponent::saveMode
                                 | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser](const juce::FileChooser& fc)
                {
                    if (fc.getResults().size() > 0)
                        outputPathEdit_.setText(fc.getResult().getFullPathName());
                });
        }
    };

    // --- Buttons ---
    addAndMakeVisible(exportButton_);
    exportButton_.onClick = [this]()
    {
        // Validate output path
        auto outPath = outputPathEdit_.getText().trim();
        if (outPath.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Invalid Output",
                "Please specify an output file path.");
            return;
        }

        // Validate ffmpeg available (unless PNG sequence)
        auto codec = static_cast<Export::VideoCodec>(codecCombo_.getSelectedId() - 1);
        if (codec != Export::VideoCodec::PNG_Sequence)
        {
            auto ffmpeg = FFmpegProcess::locateFFmpeg();
            if (!ffmpeg.existsAsFile())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "FFmpeg Not Found",
                    "Cannot export video — ffmpeg.exe was not found.\n"
                    "Place ffmpeg.exe next to the application or add it to your PATH.");
                return;
            }
        }

        if (onExport)
            onExport(getSettings());

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };

    addAndMakeVisible(cancelButton_);
    cancelButton_.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };

    // --- FFmpeg status ---
    addAndMakeVisible(ffmpegStatusLabel_);
    ffmpegStatusLabel_.setFont(juce::Font(12.0f));
    checkFFmpeg();

    // ── Post-processing controls ──
    auto addPPSlider = [this](juce::Slider& sl, juce::Label& lbl, const juce::String& text,
                              double min, double max, double step, double val)
    {
        addAndMakeVisible(lbl);
        lbl.setText(text, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(sl);
        sl.setRange(min, max, step);
        sl.setValue(val, juce::dontSendNotification);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 22);
    };

    addAndMakeVisible(caToggle_);
    addPPSlider(caSlider_, caLabel_, "Intensity:", 0.5, 20.0, 0.5, 3.0);

    addAndMakeVisible(vignetteToggle_);
    addPPSlider(vigIntSlider_, vigIntLabel_, "Darkness:", 0.0, 1.0, 0.05, 0.5);
    addPPSlider(vigRadSlider_, vigRadLabel_, "Radius:", 0.1, 1.0, 0.05, 0.8);

    addAndMakeVisible(shakeToggle_);
    addPPSlider(shakeSlider_, shakeLabel_, "Amount:", 1.0, 50.0, 1.0, 5.0);
    addAndMakeVisible(shakeBeatToggle_);
    shakeBeatToggle_.setToggleState(true, juce::dontSendNotification);

    addAndMakeVisible(beatZoomToggle_);
    addPPSlider(beatZoomSlider_, beatZoomLabel_, "Amount:", 0.01, 0.3, 0.01, 0.05);

    // ── Move all controls (except buttons) into a scrollable viewport ──
    juce::Array<juce::Component*> toReparent;
    for (int i = 0; i < getNumChildComponents(); ++i)
    {
        auto* c = getChildComponent(i);
        if (c != &exportButton_ && c != &cancelButton_)
            toReparent.add(c);
    }

    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);

    for (auto* c : toReparent)
        content_.addAndMakeVisible(c);

    // Section header labels inside scrollable content
    auto addHeader = [this](juce::Label& lbl, const juce::String& text)
    {
        content_.addAndMakeVisible(lbl);
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(13.0f, juce::Font::bold));
        lbl.setColour(juce::Label::textColourId,
                      ThemeManager::getInstance().getPalette().bodyText.withAlpha(0.6f));
    };
    addHeader(videoHeader_,   "Video");
    addHeader(audioHeader_,   "Audio");
    addHeader(outputHeader_,  "Output");
    addHeader(effectsHeader_, "Effects");
}

ExportDialog::~ExportDialog() = default;

//==============================================================================
void ExportDialog::paint(juce::Graphics& g)
{
    g.fillAll(ThemeManager::getInstance().getPalette().panelBg);
}

void ExportDialog::resized()
{
    constexpr int bw = 100, bh = 32;
    int btnY = getHeight() - bh - 12;
    exportButton_.setBounds(getWidth() - bw * 2 - 20, btnY, bw, bh);
    cancelButton_.setBounds(getWidth() - bw - 10,      btnY, bw, bh);

    viewport_.setBounds(0, 0, getWidth(), btnY - 8);
    layoutContent();
}

void ExportDialog::layoutContent()
{
    const int w = juce::jmax(kWidth, viewport_.getMaximumVisibleWidth());
    constexpr int lx = 10, lw = 140, cx = 155, rw = 320, rh = 28, gap = 4;
    int y = 10;

    videoHeader_.setBounds(lx, y, 200, 20);
    y += 24;

    auto row = [&](juce::Label& lbl, juce::Component& ctrl)
    {
        lbl.setBounds(lx, y, lw, rh);
        ctrl.setBounds(cx, y, rw, rh);
        y += rh + gap;
    };

    row(resolutionLabel_, resolutionCombo_);

    customSizeLabel_.setBounds(lx, y, lw, rh);
    customWEdit_.setBounds(cx, y, 70, rh);
    customHEdit_.setBounds(cx + 90, y, 70, rh);
    y += rh + gap;

    row(fpsLabel_, fpsCombo_);
    row(codecLabel_, codecCombo_);
    row(qualityLabel_, qualityCombo_);
    row(bitrateLabel_, bitrateSlider_);

    y += 4;
    audioHeader_.setBounds(lx, y, 200, 20);
    y += 24;

    row(audioCodecLabel_, audioCodecCombo_);
    row(audioBitrateLabel_, audioBitrateSlider_);

    y += 4;
    outputHeader_.setBounds(lx, y, 200, 20);
    y += 24;

    outputLabel_.setBounds(lx, y, lw, rh);
    outputPathEdit_.setBounds(cx, y, rw - 90, rh);
    browseButton_.setBounds(cx + rw - 84, y, 84, rh);
    y += rh + gap;

    ffmpegStatusLabel_.setBounds(lx, y + 4, w - 20, 20);
    y += 28;

    y += 4;
    effectsHeader_.setBounds(lx, y, 200, 20);
    y += 24;

    constexpr int ppx = 10;
    constexpr int ppLW = 80;
    constexpr int ppCtrlX = ppx + ppLW + 5;
    constexpr int ppCtrlW = 310;
    constexpr int smallH = 24;

    caToggle_.setBounds(ppx, y, 200, smallH);
    y += smallH + 2;
    caLabel_.setBounds(ppx, y, ppLW, smallH);
    caSlider_.setBounds(ppCtrlX, y, ppCtrlW, smallH);
    y += smallH + 4;

    vignetteToggle_.setBounds(ppx, y, 200, smallH);
    y += smallH + 2;
    vigIntLabel_.setBounds(ppx, y, ppLW, smallH);
    vigIntSlider_.setBounds(ppCtrlX, y, ppCtrlW, smallH);
    y += smallH + 2;
    vigRadLabel_.setBounds(ppx, y, ppLW, smallH);
    vigRadSlider_.setBounds(ppCtrlX, y, ppCtrlW, smallH);
    y += smallH + 4;

    shakeToggle_.setBounds(ppx, y, 200, smallH);
    y += smallH + 2;
    shakeLabel_.setBounds(ppx, y, ppLW, smallH);
    shakeSlider_.setBounds(ppCtrlX, y, ppCtrlW - 110, smallH);
    shakeBeatToggle_.setBounds(ppCtrlX + ppCtrlW - 105, y, 105, smallH);
    y += smallH + 4;

    beatZoomToggle_.setBounds(ppx, y, 200, smallH);
    y += smallH + 2;
    beatZoomLabel_.setBounds(ppx, y, ppLW, smallH);
    beatZoomSlider_.setBounds(ppCtrlX, y, ppCtrlW, smallH);
    y += smallH + 12;

    content_.setSize(w, y);
}

//==============================================================================
Export::Settings ExportDialog::getSettings() const
{
    Export::Settings s;

    // Resolution
    int resId = resolutionCombo_.getSelectedId();
    switch (resId)
    {
        case 1: s.resolution = Export::Resolution::HD_1080p;      break;
        case 2: s.resolution = Export::Resolution::QHD_1440p;     break;
        case 3: s.resolution = Export::Resolution::UHD_4K;        break;
        case 4: s.resolution = Export::Resolution::Vertical_1080; break;
        case 5: s.resolution = Export::Resolution::Custom;        break;
        default: s.resolution = Export::Resolution::HD_1080p;     break;
    }
    s.customWidth  = customWEdit_.getText().getIntValue();
    s.customHeight = customHEdit_.getText().getIntValue();

    // FPS
    s.frameRate = (fpsCombo_.getSelectedId() == 2) ? Export::FrameRate::FPS_60
                                                    : Export::FrameRate::FPS_30;
    // Video codec
    s.videoCodec = static_cast<Export::VideoCodec>(codecCombo_.getSelectedId() - 1);

    // Quality
    switch (qualityCombo_.getSelectedId())
    {
        case 1: s.qualityPreset = Export::QualityPreset::Draft; break;
        case 2: s.qualityPreset = Export::QualityPreset::Good;  break;
        case 3: s.qualityPreset = Export::QualityPreset::Best;  break;
        default: s.qualityPreset = Export::QualityPreset::Good;  break;
    }
    s.bitrateMbps = static_cast<int>(bitrateSlider_.getValue());

    // Audio
    s.audioCodec = static_cast<Export::AudioCodec>(audioCodecCombo_.getSelectedId() - 1);
    s.audioBitrateKbps = static_cast<int>(audioBitrateSlider_.getValue());

    // Files
    s.audioFile  = audioFile_;
    s.outputFile = juce::File(outputPathEdit_.getText());

    // Post-processing
    s.postProcess.chromaticAberration = caToggle_.getToggleState();
    s.postProcess.caIntensity         = static_cast<float>(caSlider_.getValue());
    s.postProcess.vignette            = vignetteToggle_.getToggleState();
    s.postProcess.vignetteIntensity   = static_cast<float>(vigIntSlider_.getValue());
    s.postProcess.vignetteRadius      = static_cast<float>(vigRadSlider_.getValue());
    s.postProcess.screenShake         = shakeToggle_.getToggleState();
    s.postProcess.shakeIntensity      = static_cast<float>(shakeSlider_.getValue());
    s.postProcess.shakeBeatSync       = shakeBeatToggle_.getToggleState();
    s.postProcess.beatZoom            = beatZoomToggle_.getToggleState();
    s.postProcess.beatZoomAmount      = static_cast<float>(beatZoomSlider_.getValue());

    return s;
}

//==============================================================================
void ExportDialog::updateCustomSizeVisibility()
{
    bool custom = (resolutionCombo_.getSelectedId() == 5);
    customSizeLabel_.setVisible(custom);
    customWEdit_.setVisible(custom);
    customHEdit_.setVisible(custom);
}

void ExportDialog::updateFileExtension()
{
    auto codec = static_cast<Export::VideoCodec>(codecCombo_.getSelectedId() - 1);
    juce::String ext = Export::videoCodecExtension(codec);

    auto currentPath = outputPathEdit_.getText();
    if (currentPath.isNotEmpty() && ext.isNotEmpty())
    {
        juce::File current(currentPath);
        auto newPath = current.getParentDirectory()
            .getChildFile(current.getFileNameWithoutExtension() + ext);
        outputPathEdit_.setText(newPath.getFullPathName());
    }
}

void ExportDialog::checkFFmpeg()
{
    auto ffmpeg = FFmpegProcess::locateFFmpeg();
    if (ffmpeg.existsAsFile())
    {
        ffmpegStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::limegreen);
        ffmpegStatusLabel_.setText("FFmpeg found: " + ffmpeg.getFullPathName(),
                                   juce::dontSendNotification);
    }
    else
    {
        ffmpegStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::orangered);
        ffmpegStatusLabel_.setText("FFmpeg not found! Add ffmpeg.exe to PATH or app folder.",
                                   juce::dontSendNotification);
    }
}
