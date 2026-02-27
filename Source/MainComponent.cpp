#include "MainComponent.h"
#include "Utils/CrashHandler.h"
#include "UI/SettingsWindow.h"
#include "UI/SplashOverlay.h"
#include "Canvas/CanvasCommands.h"
#include "Project/AppSettings.h"
#include "Export/ExportDialog.h"
#include "Export/ExportProgressWindow.h"
#include "Export/OfflineRenderer.h"
#include "Project/ProjectSerializer.h"
#include "Project/PresetTemplates.h"
#include "UI/ImageLayerComponent.h"
#include "UI/VideoLayerComponent.h"
#include "UI/ShapeComponent.h"
#include "UI/TextLabelComponent.h"
#include "UI/LoudnessMeter.h"
#include "UI/Spectrogram.h"
#include "Canvas/CustomPluginComponent.h"
#include "Canvas/PythonPluginBridge.h"
#include "Canvas/ProjectMComponent.h"
#include "Export/FFmpegProcess.h"

//==============================================================================
MainComponent::MainComponent()
    : transportBar(audioEngine),
      waveformView(audioEngine),
      statusBar(audioEngine, levelAnalyzer),
      canvasEditor(audioEngine, fftProcessor, levelAnalyzer, loudnessAnalyzer, stereoAnalyzer),
      threeDEditor_(audioEngine, fftProcessor),
      vjEditor_(canvasEditor, audioEngine)
{
    // Register as theme listener
    ThemeManager::getInstance().addListener(this);

    // Initialise Crash Handler with emergency save callback
    CrashHandler::init([this]() { emergencySave(); });

    // Wire up the audio analysis callback (runs on audio thread — must be lock-free!)
    audioEngine.setAudioBlockCallback(
        [this](const juce::AudioSourceChannelInfo& info)
        {
            auto* buffer = info.buffer;
            int numSamples = info.numSamples;
            int startSample = info.startSample;

            if (buffer->getNumChannels() >= 1)
            {
                const float* left  = buffer->getReadPointer(0, startSample);
                const float* right = buffer->getNumChannels() >= 2
                                         ? buffer->getReadPointer(1, startSample)
                                         : left;

                // Mix to mono for FFT
                for (int i = 0; i < numSamples; ++i)
                {
                    float mono = (left[i] + right[i]) * 0.5f;
                    fftProcessor.pushSamples(&mono, 1);
                }

                // Feed level analyzer
                levelAnalyzer.processSamples(left, right, numSamples);

                // Feed loudness analyzer (K-weighting + gated integration)
                loudnessAnalyzer.processSamples(left, right, numSamples);

                // Feed stereo field analyzer (correlation + goniometer)
                stereoAnalyzer.processSamples(left, right, numSamples);

                // Feed BPM detector (used in VJ mode)
                if (numSamples > 0)
                {
                    // Stack buffer — avoids heap allocation on real-time thread.
                    // Typical block sizes are 256–1024; clamp to 4096 as safety.
                    constexpr int kMaxBlock = 4096;
                    float monoBlock[kMaxBlock];
                    const int count = juce::jmin(numSamples, kMaxBlock);
                    for (int i = 0; i < count; ++i)
                        monoBlock[i] = (left[i] + right[i]) * 0.5f;
                    bpmDetector_.processSamples(monoBlock, count,
                                                audioEngine.getDeviceSampleRate());
                }
            }
        });

    // Add UI sections
    addAndMakeVisible(transportBar);
    addAndMakeVisible(waveformView);
    addAndMakeVisible(statusBar);
    addAndMakeVisible(canvasEditor);
    addChildComponent(threeDEditor_);  // initially hidden (3D mode off)
    addChildComponent(vjEditor_);      // initially hidden (VJ mode off)

    // Wire VJ editor callbacks
    vjEditor_.onRestoreScene = [this](const juce::String& json)
    {
        // Clear existing canvas items to prevent overlap with previous scene
        auto& model = canvasEditor.getModel();
        while (model.getNumItems() > 0)
            model.removeItem(model.getItem(0)->id);

        auto result = ProjectSerializer::parse(json);
        if (result.success)
            loadProjectResult({}, result);
    };
    vjEditor_.onTapBPM = [this] { bpmDetector_.tap(); };
    vjEditor_.onSetBPM = [this](float bpm) { bpmDetector_.setManualBPM(bpm); };
    vjEditor_.onInputDeviceChanged = [this](const juce::String& name)
    {
        // Enable live input if not already active, then switch device
        if (!audioEngine.isLiveInputEnabled())
            audioEngine.enableLiveInput(true);
        audioEngine.setInputDevice(name);
    };
    vjEditor_.onExitVJ = [this]
    {
        // Exit fullscreen first if active
        if (vjFullscreen_) toggleVJFullscreen();
        setWorkflowMode(WorkflowMode::Mode2D);
    };

    // VJ fullscreen / display selection
    vjEditor_.onToggleFullscreen = [this] { toggleVJFullscreen(); };
    vjEditor_.onSelectDisplay = [this](int idx)
    {
        vjTargetDisplay_ = idx;
        // If already fullscreen, move to the selected display
        if (vjFullscreen_)
        {
            toggleVJFullscreen(); // exit
            toggleVJFullscreen(); // re-enter on new display
        }
    };

    // Attach BPMDetector to VJBPMSync
    vjEditor_.attachBPMDetector(bpmDetector_);

    // Wire the 2D/3D/VJ toolbar toggle
    canvasEditor.getAlignmentToolbar().onModeChanged = [this](WorkflowMode m)
    {
        setWorkflowMode(m);
    };

    // Wire freeze (snowflake) button — default 2D action, overridden in setWorkflowMode
    canvasEditor.getAlignmentToolbar().onFreezeClicked = [this] { canvasEditor.showRenderPreview(); };

    // Attach OpenGL context — JUCE GPU-composites the entire child-component
    // hierarchy via OpenGL textures automatically (no custom renderer needed).
    // This moves the software-blit from CPU to GPU without any manual GL code.
    openGLContext_.attachTo(*this);
    openGLContext_.setContinuousRepainting(false);

    // Pre-populate canvas with a default set of meters
    canvasEditor.addMeter(MeterType::MultiBandAnalyzer, juce::Point<float>(10.f, 10.f));
    canvasEditor.addMeter(MeterType::Spectrogram,       juce::Point<float>(10.f, 310.f));
    canvasEditor.addMeter(MeterType::Goniometer,        juce::Point<float>(520.f, 10.f));
    canvasEditor.addMeter(MeterType::LissajousScope,    juce::Point<float>(520.f, 310.f));
    canvasEditor.addMeter(MeterType::CorrelationMeter,  juce::Point<float>(820.f, 10.f));
    canvasEditor.addMeter(MeterType::LoudnessMeter,     juce::Point<float>(820.f, 110.f));
    canvasEditor.addMeter(MeterType::LevelHistogram,    juce::Point<float>(820.f, 410.f));
    canvasEditor.addMeter(MeterType::PeakMeter,         juce::Point<float>(1120.f, 10.f));
    canvasEditor.addMeter(MeterType::SkinnedVUMeter,    juce::Point<float>(1120.f, 310.f));
    canvasEditor.addMeter(MeterType::SkinnedVUMeter,    juce::Point<float>(1120.f, 510.f));
    canvasEditor.addMeter(MeterType::SkinnedSpectrum,   juce::Point<float>(10.f, 610.f));
    canvasEditor.addMeter(MeterType::SkinnedOscilloscope, juce::Point<float>(520.f, 610.f));

    // Splash Screen
    if (AppSettings::getInstance().getBool(AppSettings::kShowSplashScreen, true))
    {
        auto* splash = new SplashOverlay();
        addAndMakeVisible(splash);
        splashOverlay.reset(splash);
    }

    // Stage 7: Setup keyboard shortcuts
    setupShortcuts();

    // Stage 7: Wire meter settings panel callbacks
    canvasEditor.getMeterSettings().onSkinFileSelected = [this](const juce::File& f) {
        loadSkin(f);
    };
    canvasEditor.getMeterSettings().onAudioFileSelected = [this](const juce::File& f) {
        loadAudioFile(f);
    };

    // Wire MeterFactory eject callback through proper load flow
    canvasEditor.getMeterFactory().onFileLoadRequested = [this](const juce::File& f) {
        loadAudioFile(f);
    };

    // Initial size
    setSize(1440, 900);

    //==================================================================
    // Apply persisted settings on startup
    //==================================================================
    auto& settings = AppSettings::getInstance();

    // Theme
    int savedTheme = settings.getInt(AppSettings::kTheme, 1);
    ThemeManager::getInstance().setTheme(savedTheme == 2 ? AppTheme::Light : AppTheme::Dark);

    // FPS threshold
    canvasEditor.getCanvasView().setFpsThreshold(settings.getFpsThreshold());

    // Placeholder / performance safe-mode
    canvasEditor.getCanvasView().setPlaceholderModeEnabled(
        settings.getBool(AppSettings::kPlaceholderModeEnabled, true));

    // Master gain
    audioEngine.setGain(settings.getMasterGain());

    // Undo history size
    canvasEditor.getModel().undoManager.setMaxNumberOfStoredUnits(settings.getInt(AppSettings::kUndoHistorySize, 100), 0);

    // UI scale
    float uiScale = settings.getUIScale() / 100.0f;
    if (std::abs(uiScale - 1.0f) > 0.01f)
        juce::Desktop::getInstance().setGlobalScaleFactor(uiScale);

    // Status bar visibility
    statusBar.setVisible(settings.getBool(AppSettings::kShowStatusBar, true));

    // MiniMap visibility
    canvasEditor.setMiniMapVisible(settings.getBool(AppSettings::kShowMiniMap, true));

    // Restore grid settings into canvas model
    {
        auto& grid = canvasEditor.getModel().grid;
        grid.enabled     = settings.getBool(AppSettings::kDefaultGridEnabled, true);
        grid.showGrid    = settings.getBool(AppSettings::kDefaultShowGrid,    true);
        grid.showRuler   = settings.getBool(AppSettings::kDefaultShowRuler,   true);
        grid.smartGuides = settings.getBool(AppSettings::kDefaultSmartGuides, true);
        grid.spacing     = settings.getGridSpacing();
        juce::String savedColour = settings.getString(AppSettings::kDefaultGridColour);
        if (savedColour.isNotEmpty())
            grid.gridColour = juce::Colour::fromString(savedColour);
    }

    // Toolbox view mode
    int toolboxMode = settings.getInt(AppSettings::kToolboxViewMode, 1);
    canvasEditor.setToolboxViewMode(toolboxMode);

    // Start GUI refresh timer (use saved rate)
    startTimerHz(settings.getTimerRateHz());

    // Eagerly warm-start the Python plugin bridge so it's ready before the
    // user opens the Toolbox (avoids startup latency on first plugin add).
    juce::MessageManager::callAsync([]()
    {
        auto pluginsDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                              .getParentDirectory()
                              .getChildFile("CustomComponents")
                              .getChildFile("plugins");
        PythonPluginBridge::getInstance().start(pluginsDir);
    });

    // Auto-save timer
    if (settings.getAutoSave())
        startAutoSaveTimer(settings.getAutoSaveIntervalSec());

    // Open last project on startup
    if (settings.getBool(AppSettings::kOpenLastProject, false))
    {
        juce::String lastPath = settings.getString(AppSettings::kLastProjectPath);
        if (lastPath.isNotEmpty())
        {
            juce::File lastFile(lastPath);
            if (lastFile.existsAsFile())
            {
                juce::Component::SafePointer<MainComponent> safeThis(this);
                juce::MessageManager::callAsync([safeThis, lastFile]()
                {
                    if (safeThis == nullptr) return;
                    // Re-use openProject logic with a known file
                    auto result = ProjectSerializer::loadFromFile(lastFile);
                    if (result.success)
                    {
                        safeThis->newProject();  // clear existing items
                        safeThis->loadProjectResult(lastFile, result);
                    }
                });
            }
        }
    }
}

MainComponent::~MainComponent()
{
    // 1. Stop the GUI timer first so timerCallback() won't fire during teardown.
    stopTimer();

    // 2. Clear the real-time audio callback BEFORE any member is destroyed.
    //    The callback captures [this] and accesses fftProcessor, levelAnalyzer,
    //    bpmDetector_, etc. — all of which are destroyed after audioEngine.
    //    Without this, the audio thread could dereference a dead member.
    audioEngine.setAudioBlockCallback({});

    // 3. Unconditionally deactivate VJ (stops VJEditor timer + live input).
    //    Previous code only ran this when currentMode_ == ModeVJ, but the
    //    timer/callbacks may still be live if VJ was activated earlier.
    vjEditor_.setActive(false);
    audioEngine.enableLiveInput(false);

    // 4. Null VJEditor callbacks — they capture [this] (MainComponent).
    vjEditor_.onRestoreScene       = nullptr;
    vjEditor_.onTapBPM             = nullptr;
    vjEditor_.onSetBPM             = nullptr;
    vjEditor_.onInputDeviceChanged = nullptr;
    vjEditor_.onExitVJ             = nullptr;
    vjEditor_.onToggleFullscreen   = nullptr;
    vjEditor_.onSelectDisplay      = nullptr;

    // 5. Detach OpenGL context before any child components are destroyed.
    openGLContext_.detach();

    // 6. Unregister crash handler — its callback captures [this].
    CrashHandler::init({});

    ThemeManager::getInstance().removeListener(this);
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(ThemeManager::getInstance().getPalette().windowBg);
}

void MainComponent::resized()
{
    setupLayout();

    if (splashOverlay)
        splashOverlay->setBounds(getLocalBounds());
}

void MainComponent::setWorkflowMode(WorkflowMode mode)
{
    currentMode_ = mode;

    // Update toolbar toggle state
    canvasEditor.getAlignmentToolbar().setMode(mode);

    // Redirect the freeze / render-preview button to the active editor
    if (mode == WorkflowMode::Mode3D)
        canvasEditor.getAlignmentToolbar().onFreezeClicked = [this] { threeDEditor_.showRenderPreview3D(); };
    else
        canvasEditor.getAlignmentToolbar().onFreezeClicked = [this] { canvasEditor.showRenderPreview(); };

    // JUCE only supports one OpenGLContext per window.
    // Detach when 3D or VJ editor is active; reattach for 2D.
    if (mode == WorkflowMode::Mode3D || mode == WorkflowMode::ModeVJ)
        openGLContext_.detach();
    else
        openGLContext_.attachTo(*this);

    // When leaving VJ mode, disable live input if it was active.
    // When entering VJ mode, keep file playback alive — the user can
    // switch to live input from the VJ control-panel dropdown.
    if (mode != WorkflowMode::ModeVJ && audioEngine.isLiveInputEnabled())
        audioEngine.enableLiveInput(false);

    // Activate/deactivate VJ editor
    vjEditor_.setActive(mode == WorkflowMode::ModeVJ);

    // In VJ mode: suppress all editor overlays (ruler, FPS counter, selection
    // handles) and disable the automatic placeholder/performance mode that
    // hides meter components when FPS drops.  The canvasView still renders
    // normally — VJEditor snapshots it via createComponentSnapshot().
    canvasEditor.getCanvasView().setPreviewMode(mode == WorkflowMode::ModeVJ);
    canvasEditor.getCanvasView().setPlaceholderModeEnabled(mode != WorkflowMode::ModeVJ);

    setupLayout();
}

void MainComponent::setupLayout()
{
    auto area = getLocalBounds();

    if (splashOverlay != nullptr)
        splashOverlay->setBounds(area);

    // In VJ fullscreen mode, hide transport / waveform / status bars
    // and give VJEditor the full window area.
    if (vjFullscreen_ && currentMode_ == WorkflowMode::ModeVJ)
    {
        canvasEditor.setVisible(false);
        canvasEditor.setBounds(area);
        threeDEditor_.setVisible(false);
        vjEditor_.setVisible(true);
        vjEditor_.setBounds(area);
        vjEditor_.toFront(false);
        return;
    }

    // Top: Transport bar (48px)
    if (transportBar.isVisible())
        transportBar.setBounds(area.removeFromTop(48));

    // Bottom: Status bar (24px) — only if visible
    if (statusBar.isVisible())
        statusBar.setBounds(area.removeFromBottom(24));

    // Bottom section: Waveform view (80px)
    if (waveformView.isVisible())
        waveformView.setBounds(area.removeFromBottom(80));

    // In 3D mode the ThreeDEditor sits on top of the canvas body, leaving the
    // 30-px AlignmentToolbar strip uncovered so the user can still click 2D/3D.
    static constexpr int kToolbarH = 30;
    auto bodyArea = area.withTrimmedTop(kToolbarH);

    if (currentMode_ == WorkflowMode::Mode2D)
    {
        canvasEditor.setVisible(true);
        canvasEditor.setBounds(area);
        threeDEditor_.setVisible(false);
        vjEditor_.setVisible(false);
        threeDEditor_.setBounds(bodyArea);
        vjEditor_.setBounds(bodyArea);
    }
    else if (currentMode_ == WorkflowMode::Mode3D)
    {
        canvasEditor.setVisible(true);
        canvasEditor.setBounds(area);
        threeDEditor_.setVisible(true);
        vjEditor_.setVisible(false);
        threeDEditor_.setBounds(bodyArea);
        vjEditor_.setBounds(bodyArea);
    }
    else  // ModeVJ
    {
        // Hide canvasEditor FIRST — its toolbox / property panels must not
        // overlap VJEditor or steal mouse events.  setVisible(false) is called
        // before setBounds so that resized() on canvasEditor never triggers
        // visible child layout.  createComponentSnapshot() still works on
        // invisible components, so the LivePreview snapshot path is unaffected.
        canvasEditor.setVisible(false);
        canvasEditor.setBounds(bodyArea);

        threeDEditor_.setVisible(false);
        vjEditor_.setVisible(true);

        // VJEditor fills the full body area (no 30-px toolbar gap — the
        // AlignmentToolbar belongs to the hidden canvasEditor).
        threeDEditor_.setBounds(bodyArea);
        vjEditor_.setBounds(area);
        vjEditor_.toFront(false);
    }
}

//==============================================================================
void MainComponent::toggleVJFullscreen()
{
    vjFullscreen_ = !vjFullscreen_;

    if (auto* win = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent()))
    {
        if (vjFullscreen_)
        {
            // Move to target display if not primary
            auto& displays = juce::Desktop::getInstance().getDisplays().displays;
            if (vjTargetDisplay_ >= 0 && vjTargetDisplay_ < displays.size())
            {
                auto area = displays[vjTargetDisplay_].userArea;
                win->setBounds(area);
            }

            // Hide transport / waveform / status bars
            transportBar.setVisible(false);
            waveformView.setVisible(false);
            statusBar.setVisible(false);

            win->setFullScreen(true);
        }
        else
        {
            win->setFullScreen(false);

            // Restore transport / waveform / status bars
            transportBar.setVisible(true);
            waveformView.setVisible(true);
            statusBar.setVisible(true);
        }
    }

    // Update control panel button state
    vjEditor_.getControlPanel().setFullscreenState(vjFullscreen_);
    setupLayout();
}

//==============================================================================
void MainComponent::timerCallback()
{
    // Splash logic
    if (splashOverlay)
    {
        auto* splash = dynamic_cast<SplashOverlay*>(splashOverlay.get());
        if (splash && splash->isFinished())
        {
            splashOverlay.reset();
        }
    }

    // Process any pending FFT data on the GUI thread
    while (fftProcessor.processNextBlock()) {}

    // Feed all meters through the canvas editor
    canvasEditor.timerTick();

    // Feed 3D editor when in 3D mode
    if (currentMode_ == WorkflowMode::Mode3D)
        threeDEditor_.timerTick();

    // Feed VJ editor latency readout when in VJ mode
    if (currentMode_ == WorkflowMode::ModeVJ)
    {
        const float bufMs = audioEngine.getLiveInputLatencyMs();
        vjEditor_.setLatencyMs(bufMs, bufMs);   // A→V approx same as buffer latency
    }

    // Feed Winamp renderer with title and state info
    if (winampRenderer.hasSkin())
    {
        double sampleRate = audioEngine.getFileSampleRate();
        auto ps = audioEngine.isPlaying()
            ? WinampSkinRenderer::PlayState::Playing
            : WinampSkinRenderer::PlayState::Stopped;
        winampRenderer.setPlayState(ps);

        if (sampleRate > 0)
        {
            double pos = audioEngine.getCurrentPosition();
            int minutes = static_cast<int>(pos) / 60;
            int seconds = static_cast<int>(pos) % 60;
            winampRenderer.setTime(minutes, seconds);
        }
        winampRenderer.setTitleText(audioEngine.getLoadedFileName());
    }

    openGLContext_.triggerRepaint();

    // Auto-save tick — accumulate elapsed time and trigger save when interval reached
    if (autoSaveIntervalMs > 0 && currentProjectFile.existsAsFile())
    {
        int timerHz = AppSettings::getInstance().getTimerRateHz();
        int msPerTick = (timerHz > 0) ? (1000 / timerHz) : 16;
        autoSaveElapsedMs += msPerTick;
        if (autoSaveElapsedMs >= autoSaveIntervalMs)
        {
            autoSaveElapsedMs = 0;
            saveProject();
        }
    }
}

//==============================================================================
void MainComponent::loadAudioFile(const juce::File& file)
{
    if (audioEngine.loadFile(file))
    {
        double sr = audioEngine.getFileSampleRate();

        levelAnalyzer.setSampleRate(sr);
        levelAnalyzer.reset();
        fftProcessor.reset();
        waveformView.loadThumbnail(file);

        loudnessAnalyzer.setSampleRate(sr);
        loudnessAnalyzer.reset();
        stereoAnalyzer.setSampleRate(sr);
        stereoAnalyzer.reset();

        // Reset meters through canvas editor
        canvasEditor.onFileLoaded(sr);
    }
}

void MainComponent::loadSkin(const juce::File& skinFile)
{
    if (winampRenderer.loadSkin(skinFile))
    {
        skinLoaded = true;
        const auto& skinModel = winampRenderer.getSkinModel();

        // Apply skin to all skinned meter widgets
        canvasEditor.applySkinToAll(&skinModel);

        // Apply skin colors as a full theme override (grid, panels, window, etc.)
        ThemeManager::getInstance().applySkinTheme(skinModel);
    }
}

//==============================================================================
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg"
            || ext == ".aiff" || ext == ".aif"
            || ext == ".wsz" || ext == ".zip"
            || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp"
            || ext == ".gif" || ext == ".mp4" || ext == ".avi" || ext == ".mov"
            || ext == ".webm" || ext == ".webp")
            return true;
    }
    return false;
}

void MainComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        auto ext = file.getFileExtension().toLowerCase();

        if (ext == ".wsz" || ext == ".zip")
            loadSkin(file);
        else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp")
            addMediaToCanvas(file, MeterType::ImageLayer);
        else if (ext == ".gif" || ext == ".mp4" || ext == ".avi" || ext == ".mov" || ext == ".webm")
            addMediaToCanvas(file, MeterType::VideoLayer);
        else
            loadAudioFile(file);
    }
}

void MainComponent::addMediaToCanvas(const juce::File& file, MeterType type)
{
    // For video files, require FFmpeg — show a clear error and bail if missing.
    if (type == MeterType::VideoLayer && !FFmpegProcess::locateFFmpeg().existsAsFile())
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withTitle("FFmpeg Required")
                .withMessage("Loading video files requires FFmpeg, which was not found.\n\n"
                             "Please download FFmpeg from https://ffmpeg.org/download.html\n"
                             "and place ffmpeg.exe in the same folder as MaxiMeter.exe, then try again.")
                .withButton("OK"),
            nullptr);
        return;
    }

    // Add the meter to the canvas centre
    auto centre = canvasEditor.getCanvasView().getLocalBounds().getCentre().toFloat();
    auto* item = canvasEditor.addMeter(type, centre);
    if (item == nullptr) return;

    item->mediaFilePath = file.getFullPathName();

    // Load contents into the component
    if (type == MeterType::ImageLayer)
    {
        if (auto* comp = dynamic_cast<ImageLayerComponent*>(item->component.get()))
            comp->loadFromFile(file);
    }
    else if (type == MeterType::VideoLayer)
    {
        if (auto* comp = dynamic_cast<VideoLayerComponent*>(item->component.get()))
            comp->loadFromFile(file);
    }

    canvasEditor.getCanvasView().updateChildBounds();
    canvasEditor.getCanvasView().repaint();
}

//==============================================================================
// Stage 6 & 7: Keyboard shortcuts
//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    // Delegate to the central shortcut manager
    if (shortcutManager.handleKeyPress(key))
        return true;

    return false;
}

void MainComponent::exportVideo()
{
    if (currentMode_ == WorkflowMode::Mode3D)
    {
        // Ask user whether to export 2D or 3D
        auto opts = std::make_shared<juce::MessageBoxOptions>();
        *opts = juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Export Video")
            .withMessage("Which view do you want to export?")
            .withButton("2D Canvas")
            .withButton("3D View")
            .withButton("Cancel");

        juce::AlertWindow::showAsync(*opts, [this](int result)
        {
            if (result == 1)          // "2D Canvas"
                showExportDialog();
            else if (result == 2)     // "3D View"
                showExportDialog3D();
            // result == 0 → Cancel, do nothing
        });
    }
    else
    {
        showExportDialog();
    }
}

void MainComponent::showExportDialog()
{
    // Determine the currently loaded audio file
    auto audioFile = audioEngine.getLoadedFile();

    if (!audioFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "No Audio File",
            "Please load an audio file before exporting video.");
        return;
    }

    Export::Settings defaults;
    defaults.audioFile = audioFile;

    // Load defaults from AppSettings
    auto& s = AppSettings::getInstance();
    // Resolution combo is 1-based; Export::Resolution enum is 0-based
    int resIdx = juce::jlimit(0, 3, s.getInt(AppSettings::kDefaultResolution, 1) - 1);
    defaults.resolution = static_cast<Export::Resolution>(resIdx);
    // FrameRate combo: ID 5 = 60fps, anything else = 30fps
    int fpsId = s.getInt(AppSettings::kDefaultFrameRate, 3);
    defaults.frameRate = (fpsId == 5) ? Export::FrameRate::FPS_60 : Export::FrameRate::FPS_30;

    auto* dialog = new ExportDialog(defaults, audioFile);
    dialog->onExport = [this](const Export::Settings& settings)
    {
        // Freeze the canvas viewport during export
        canvasEditor.setExportOverlay(true);

        // Create an offline renderer and launch the progress window
        auto renderer = std::make_unique<OfflineRenderer>(
            settings,
            canvasEditor.getModel(),
            audioEngine);

        // The ExportProgressWindow takes ownership and self-deletes on close
        auto* progressWin = new ExportProgressWindow(std::move(renderer));
        progressWin->onClose = [this]()
        {
            canvasEditor.setExportOverlay(false);
        };
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle          = "Export Video";
    options.dialogBackgroundColour = ThemeManager::getInstance().getPalette().panelBg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar    = false;
    options.resizable            = true;

    auto* win = options.create();
    win->setLookAndFeel(&exportDialogLnf_);
    win->setTitleBarHeight(32);
    win->setVisible(true);
}

void MainComponent::showExportDialog3D()
{
    // Require an audio file so we know the duration / sample data
    auto audioFile = audioEngine.getLoadedFile();
    if (!audioFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "No Audio File",
            "Please load an audio file before exporting 3D video.");
        return;
    }

    Export::Settings defaults;
    defaults.audioFile = audioFile;

    // Reuse saved resolution / fps preferences
    auto& s = AppSettings::getInstance();
    int resIdx = juce::jlimit (0, 3, s.getInt (AppSettings::kDefaultResolution, 1) - 1);
    defaults.resolution = static_cast<Export::Resolution> (resIdx);
    int fpsId = s.getInt (AppSettings::kDefaultFrameRate, 3);
    defaults.frameRate  = (fpsId == 5) ? Export::FrameRate::FPS_60 : Export::FrameRate::FPS_30;

    auto* dialog = new ExportDialog (defaults, audioFile);
    dialog->onExport = [this] (const Export::Settings& settings)
    {
        // Launch the 3D offline renderer + progress window
        threeDEditor_.startVideoExport (settings);
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (dialog);
    options.dialogTitle                  = "Export 3D Video";
    options.dialogBackgroundColour       = ThemeManager::getInstance().getPalette().panelBg;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = false;
    options.resizable                    = true;

    auto* win = options.create();
    win->setLookAndFeel (&exportDialogLnf_);
    win->setTitleBarHeight (32);
    win->setVisible (true);
}

//==============================================================================
// Stage 7: Theme change callback
//==============================================================================
void MainComponent::themeChanged(AppTheme /*newTheme*/)
{
    // Propagate theme to all sub-panels that cache widget colors
    canvasEditor.applyThemeToAllPanels();

    // Recursively repaint the entire component tree
    sendLookAndFeelChange();
    
    std::function<void(juce::Component*)> repaintAll = [&](juce::Component* comp)
    {
        comp->repaint();
        for (int i = 0; i < comp->getNumChildComponents(); ++i)
            repaintAll(comp->getChildComponent(i));
    };
    repaintAll(this);
}

//==============================================================================
// Stage 7: Project management
//==============================================================================
void MainComponent::newProject()
{
    // Clear all items from the canvas
    auto& model = canvasEditor.getModel();
    while (model.getNumItems() > 0)
        model.removeItem(model.getItem(0)->id);

    currentProjectFile = {};

    // Apply New Project defaults from Settings
    auto& s = AppSettings::getInstance();
    model.grid.spacing = s.getInt(AppSettings::kDefaultGridSpacing, 50);
    model.grid.showGrid = s.getBool(AppSettings::kDefaultShowGrid, true);
    model.grid.smartGuides = s.getBool(AppSettings::kDefaultSmartGuides, true);
    
    // Note: Rulers are part of CanvasView/Editor settings, not strictly model, 
    // but often desired to be reset or kept.
    // For now we assume they persist per-session or we could reset them here.
    
    // Reset view (zoom/pan)
    model.setZoom(1.0f);
    model.panX = 0;
    model.panY = 0;
    
    canvasEditor.getCanvasView().repaint();
}

void MainComponent::openProject()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Open Project", juce::File(), "*.mmproj");

    chooser->launchAsync(juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (!file.existsAsFile()) return;

            auto result = ProjectSerializer::loadFromFile(file);
            if (!result.success)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Load Error", result.errorMessage);
                return;
            }

            // Clear existing items
            newProject();

            // Apply load result
            loadProjectResult(file, result);
        });
}

void MainComponent::saveProject()
{
    if (currentProjectFile.existsAsFile())
    {
        juce::File skinFile;
        if (skinLoaded && winampRenderer.hasSkin())
            skinFile = {}; // No easy way to get skin file back — save empty

        ProjectSerializer::saveToFile(currentProjectFile,
                                      canvasEditor.getModel(),
                                      skinFile,
                                      audioEngine.getLoadedFile());
        AppSettings::getInstance().set(AppSettings::kLastProjectPath,
                                       currentProjectFile.getFullPathName());
    }
    else
    {
        saveProjectAs();
    }
}

void MainComponent::saveProjectAs()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Project As", juce::File(), "*.mmproj");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                       | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;

            // Ensure extension
            if (!file.hasFileExtension(".mmproj"))
                file = file.withFileExtension(".mmproj");

            juce::File skinFile;
            ProjectSerializer::saveToFile(file,
                                          canvasEditor.getModel(),
                                          skinFile,
                                          audioEngine.getLoadedFile());
            currentProjectFile = file;
            AppSettings::getInstance().set(AppSettings::kLastProjectPath,
                                           file.getFullPathName());
        });
}

void MainComponent::loadPresetTemplate(int index)
{
    auto tmpl = PresetTemplates::getTemplate(index);
    if (tmpl.meters.empty()) return;

    // Clear canvas
    newProject();

    // Populate with preset meters
    for (const auto& m : tmpl.meters)
    {
        auto canvasPos = juce::Point<float>(m.x + m.w * 0.5f, m.y + m.h * 0.5f);
        auto* item = canvasEditor.addMeter(m.type, canvasPos);
        if (item)
        {
            item->x      = m.x;
            item->y      = m.y;
            item->width  = m.w;
            item->height = m.h;
        }
    }

    canvasEditor.getModel().notifyItemsChanged();
}

void MainComponent::showSettings()
{
    auto* w = new SettingsWindow(canvasEditor, audioEngine, shortcutManager);
    w->onSettingsChanged = [this]()
    {
        applyLiveSettings();
    };
    w->setVisible(true);
}

void MainComponent::applyLiveSettings()
{
    auto& s = AppSettings::getInstance();

    // Theme (Accent Colour)
    // Note: Theme ID changes are usually immediate in SettingsWindow via ThemeManager, 
    // but Accent Colour might need this push if not handled there.
    juce::String accentStr = s.getString(AppSettings::kAccentColour);
    if (accentStr.isNotEmpty())
        ThemeManager::getInstance().setCustomAccentColor(juce::Colour::fromString(accentStr));
    else
        ThemeManager::getInstance().setCustomAccentColor(std::nullopt);

    // Status bar
    statusBar.setVisible(s.getBool(AppSettings::kShowStatusBar, true));

    // MiniMap
    canvasEditor.setMiniMapVisible(s.getBool(AppSettings::kShowMiniMap, true));

    // Toolbox view
    canvasEditor.setToolboxViewMode(s.getInt(AppSettings::kToolboxViewMode, 1));

    // Undo history
    canvasEditor.getModel().undoManager.setMaxNumberOfStoredUnits(
        s.getInt(AppSettings::kUndoHistorySize, 100), 0);

    // UI scale
    float scale = s.getUIScale() / 100.0f;
    juce::Desktop::getInstance().setGlobalScaleFactor(scale);

    // Auto-save timer
    stopAutoSaveTimer();
    if (s.getAutoSave())
        startAutoSaveTimer(s.getAutoSaveIntervalSec());

    // Restart timer with updated rate
    stopTimer();
    startTimerHz(s.getTimerRateHz());

    // Re-layout since visibility may have changed
    setupLayout();
    repaint();
}

//==============================================================================
// Stage 7: Wire shortcut actions
//==============================================================================
void MainComponent::setupShortcuts()
{
    // File
    shortcutManager.setAction(ShortcutId::NewProject,    [this]() { newProject(); });
    shortcutManager.setAction(ShortcutId::OpenProject,   [this]() { openProject(); });
    shortcutManager.setAction(ShortcutId::SaveProject,   [this]() { saveProject(); });
    shortcutManager.setAction(ShortcutId::SaveProjectAs, [this]() { saveProjectAs(); });
    shortcutManager.setAction(ShortcutId::ExportVideo,   [this]() { exportVideo(); });

    // Edit — delegate to canvas model
    shortcutManager.setAction(ShortcutId::Undo, [this]() {
        canvasEditor.getModel().undoManager.undo();
    });
    shortcutManager.setAction(ShortcutId::Redo, [this]() {
        canvasEditor.getModel().undoManager.redo();
    });
    shortcutManager.setAction(ShortcutId::Copy, [this]() {
        canvasEditor.getModel().copySelection();
    });
    shortcutManager.setAction(ShortcutId::Paste, [this]() {
        auto centre = canvasEditor.getModel().screenToCanvas(
            canvasEditor.getCanvasView().getLocalBounds().getCentre().toFloat());
        canvasEditor.pasteItems(centre);
    });
    shortcutManager.setAction(ShortcutId::Duplicate, [this]() {
        canvasEditor.duplicateItems();
    });
    shortcutManager.setAction(ShortcutId::Delete, [this]() {
        auto sel = canvasEditor.getModel().getSelectedItems();
        for (auto* item : sel)
            canvasEditor.getModel().removeItem(item->id);
    });
    shortcutManager.setAction(ShortcutId::SelectAll, [this]() {
        canvasEditor.getModel().selectAll();
    });

    // Canvas z-order
    shortcutManager.setAction(ShortcutId::BringToFront, [this]() {
        auto& m = canvasEditor.getModel();
        auto sel = m.getSelectedItems();
        if (sel.empty()) return;
        int maxZ = 0;
        for (int i = 0; i < m.getNumItems(); ++i)
            maxZ = std::max(maxZ, m.getItem(i)->zOrder);
        m.undoManager.beginNewTransaction("Bring to Front");
        for (auto* item : sel)
            m.undoManager.perform(new ChangeZOrderAction(m, item->id, item->zOrder, ++maxZ), "Front");
    });
    shortcutManager.setAction(ShortcutId::SendToBack, [this]() {
        auto& m = canvasEditor.getModel();
        auto sel = m.getSelectedItems();
        if (sel.empty()) return;
        int minZ = 0;
        for (int i = 0; i < m.getNumItems(); ++i)
            minZ = std::min(minZ, m.getItem(i)->zOrder);
        m.undoManager.beginNewTransaction("Send to Back");
        for (auto* item : sel)
            m.undoManager.perform(new ChangeZOrderAction(m, item->id, item->zOrder, --minZ), "Back");
    });

    // View
    shortcutManager.setAction(ShortcutId::ZoomIn, [this]() {
        auto& m = canvasEditor.getModel();
        m.setZoom(m.zoom * 1.25f);
    });
    shortcutManager.setAction(ShortcutId::ZoomOut, [this]() {
        auto& m = canvasEditor.getModel();
        m.setZoom(m.zoom * 0.8f);
    });
    shortcutManager.setAction(ShortcutId::ZoomReset, [this]() {
        canvasEditor.getModel().setZoom(1.0f);
    });
    shortcutManager.setAction(ShortcutId::ZoomToFit, [this]() {
        canvasEditor.getModel().frameToAll(canvasEditor.getCanvasView().getBounds());
    });
    shortcutManager.setAction(ShortcutId::ToggleGrid, [this]() {
        auto& g = canvasEditor.getModel().grid;
        g.showGrid = !g.showGrid;
        canvasEditor.getModel().notifyZoomPanChanged();
    });
    shortcutManager.setAction(ShortcutId::ToggleSnap, [this]() {
        auto& g = canvasEditor.getModel().grid;
        g.enabled = !g.enabled;
    });
    shortcutManager.setAction(ShortcutId::ToggleTheme, [this]() {
        ThemeManager::getInstance().toggleTheme();
    });

    // Transport
    shortcutManager.setAction(ShortcutId::PlayPause, [this]() {
        if (audioEngine.isPlaying())
            audioEngine.stop();
        else
            audioEngine.play();
    });
    shortcutManager.setAction(ShortcutId::Stop, [this]() {
        audioEngine.stop();
    });
    shortcutManager.setAction(ShortcutId::Rewind, [this]() {
        audioEngine.setPosition(0.0);
    });

    // Alignment
    shortcutManager.setAction(ShortcutId::AlignLeft, [this]() {
        canvasEditor.getModel().alignSelection(CanvasModel::AlignEdge::Left);
    });
    shortcutManager.setAction(ShortcutId::AlignRight, [this]() {
        canvasEditor.getModel().alignSelection(CanvasModel::AlignEdge::Right);
    });
    shortcutManager.setAction(ShortcutId::AlignTop, [this]() {
        canvasEditor.getModel().alignSelection(CanvasModel::AlignEdge::Top);
    });
    shortcutManager.setAction(ShortcutId::AlignBottom, [this]() {
        canvasEditor.getModel().alignSelection(CanvasModel::AlignEdge::Bottom);
    });
    shortcutManager.setAction(ShortcutId::DistributeH, [this]() {
        canvasEditor.getModel().distributeSelectionH();
    });
    shortcutManager.setAction(ShortcutId::DistributeV, [this]() {
        canvasEditor.getModel().distributeSelectionV();
    });

    // VJ fullscreen (F11)
    shortcutManager.setAction(ShortcutId::ToggleFullscreen, [this]() {
        if (currentMode_ == WorkflowMode::ModeVJ)
            toggleVJFullscreen();
    });
}

//==============================================================================
// loadProjectResult — shared project-loading logic used by openProject() and
// startup open-last-project
//==============================================================================
void MainComponent::loadProjectResult(const juce::File& file,
                                       const ProjectSerializer::LoadResult& result)
{
    // Re-create items from loaded data
    for (const auto& desc : result.items)
    {
        auto canvasPos = juce::Point<float>(
            desc.x + desc.w * 0.5f,
            desc.y + desc.h * 0.5f);
        auto* item = canvasEditor.addMeter(desc.type, canvasPos);
        if (item)
        {
            item->x        = desc.x;
            item->y        = desc.y;
            item->width    = desc.w;
            item->height   = desc.h;
            item->rotation = desc.rotation;
            item->zOrder   = desc.zOrder;
            item->locked   = desc.locked;
            item->visible  = desc.visible;
            item->name     = desc.name;
            item->aspectLock = desc.aspectLock;
            item->opacity  = desc.opacity;
            item->mediaFilePath = desc.mediaFilePath;
            item->vuChannel = desc.vuChannel;
            item->itemBackground = desc.itemBackground;

            // Meter colour overrides
            item->meterBgColour = desc.meterBgColour;
            item->meterFgColour = desc.meterFgColour;
            item->blendMode     = desc.blendMode;

            // Custom plugin IDs
            item->customPluginId    = desc.customPluginId;
            item->customInstanceId  = desc.customInstanceId;

            // Shape properties
            item->fillColour1       = desc.fillColour1;
            item->fillColour2       = desc.fillColour2;
            item->gradientDirection = desc.gradientDirection;
            item->cornerRadius      = desc.cornerRadius;
            item->strokeColour      = desc.strokeColour;
            item->strokeWidth       = desc.strokeWidth;
            item->strokeAlignment   = desc.strokeAlignment;
            item->lineCap           = desc.lineCap;
            item->starPoints        = desc.starPoints;
            item->triangleRoundness = desc.triangleRoundness;

            // SVG shape
            item->svgPathData = desc.svgPathData;
            item->svgFilePath = desc.svgFilePath;

            // Loudness meter
            item->targetLUFS          = desc.targetLUFS;
            item->loudnessShowHistory = desc.loudnessShowHistory;

            // Spectrogram
            item->spectrogramReassigned = desc.spectrogramReassigned;
            item->spectrogramType = desc.spectrogramType;

            // Frosted glass
            item->frostedGlass = desc.frostedGlass;
            item->blurRadius   = desc.blurRadius;
            item->frostTint    = desc.frostTint;
            item->frostOpacity = desc.frostOpacity;

            // Text properties
            item->textContent   = desc.textContent;
            item->fontFamily    = desc.fontFamily;
            item->fontSize      = desc.fontSize;
            item->fontBold      = desc.fontBold;
            item->fontItalic    = desc.fontItalic;
            item->textColour    = desc.textColour;
            item->textAlignment = desc.textAlignment;

            // Water Reflection
            item->waterSpeed          = desc.waterSpeed;
            item->waterIntensity      = desc.waterIntensity;
            item->waterBlur           = desc.waterBlur;
            item->waterWaveScale      = desc.waterWaveScale;
            item->waterDesaturation   = desc.waterDesaturation;
            item->waterMistOpacity    = desc.waterMistOpacity;
            item->waterShimmerCount   = desc.waterShimmerCount;
            item->waterReflectOpacity = desc.waterReflectOpacity;
            item->waterDepthFade      = desc.waterDepthFade;
            item->waterPerspective    = desc.waterPerspective;
            item->waterTintColour     = desc.waterTintColour;

            // Grouping
            if (desc.groupId.isNotEmpty())
                item->groupId = juce::Uuid(desc.groupId);

            // Push visual properties to the component
            if (item->component)
            {
                if (auto* shape = dynamic_cast<ShapeComponent*>(item->component.get()))
                {
                    shape->setFillColour1(item->fillColour1);
                    shape->setFillColour2(item->fillColour2);
                    shape->setGradientDirection(item->gradientDirection);
                    shape->setCornerRadius(item->cornerRadius);
                    shape->setStrokeColour(item->strokeColour);
                    shape->setStrokeWidth(item->strokeWidth);
                    shape->setStrokeAlignment(static_cast<StrokeAlignment>(item->strokeAlignment));
                    shape->setLineCap(static_cast<LineCap>(item->lineCap));
                    shape->setStarPoints(item->starPoints);
                    shape->setTriangleRoundness(item->triangleRoundness);
                    if (item->svgPathData.isNotEmpty())
                        shape->setSvgPathData(item->svgPathData);
                    shape->setItemBackground(item->itemBackground);
                    shape->setFrostedGlass(item->frostedGlass);
                    shape->setBlurRadius(item->blurRadius);
                    shape->setFrostTint(item->frostTint);
                    shape->setFrostOpacity(item->frostOpacity);
                }
                else if (auto* spectrogram = dynamic_cast<Spectrogram*>(item->component.get()))
                {
                    auto t = static_cast<Spectrogram::SpectrogramType>(
                        juce::jlimit(0, 4, item->spectrogramType));
                    spectrogram->setSpectrogramType(t);
                }
                else if (auto* loudness = dynamic_cast<LoudnessMeter*>(item->component.get()))
                {
                    loudness->setTargetLUFS(item->targetLUFS);
                    loudness->setShowHistory(item->loudnessShowHistory);
                }
                else if (auto* text = dynamic_cast<TextLabelComponent*>(item->component.get()))
                {
                    text->setTextContent(item->textContent);
                    text->setFontFamily(item->fontFamily);
                    text->setFontSize(item->fontSize);
                    text->setBold(item->fontBold);
                    text->setItalic(item->fontItalic);
                    text->setTextColour(item->textColour);
                    text->setTextAlignment(item->textAlignment);
                    text->setFillColour1(item->fillColour1);
                    text->setFillColour2(item->fillColour2);
                    text->setGradientDirection(item->gradientDirection);
                    text->setCornerRadius(item->cornerRadius);
                    text->setStrokeColour(item->strokeColour);
                    text->setStrokeWidth(item->strokeWidth);
                    text->setItemBackground(item->itemBackground);
                }
            }

            // Load media file into the component if applicable
            if (desc.mediaFilePath.isNotEmpty())
            {
                juce::File mediaFile(desc.mediaFilePath);
                if (mediaFile.existsAsFile())
                {
                    if (desc.type == MeterType::ImageLayer)
                    {
                        if (auto* comp = dynamic_cast<ImageLayerComponent*>(item->component.get()))
                            comp->loadFromFile(mediaFile);
                    }
                    else if (desc.type == MeterType::VideoLayer)
                    {
                        if (auto* comp = dynamic_cast<VideoLayerComponent*>(item->component.get()))
                            comp->loadFromFile(mediaFile);
                    }
                }
            }

            // Initialise CustomPlugin bridge instance
            if (desc.type == MeterType::CustomPlugin
                && desc.customPluginId.isNotEmpty()
                && item->component)
            {
                auto& bridge = PythonPluginBridge::getInstance();
                if (!bridge.isRunning())
                {
                    auto pluginsDir = juce::File::getSpecialLocation(
                        juce::File::currentExecutableFile).getParentDirectory()
                        .getChildFile("CustomComponents").getChildFile("plugins");
                    bridge.start(pluginsDir);
                }

                auto* cpc = dynamic_cast<CustomPluginComponent*>(item->component.get());
                if (cpc)
                {
                    // Use original instance ID or generate a new one
                    auto instanceId = desc.customInstanceId.isNotEmpty()
                                        ? desc.customInstanceId
                                        : juce::Uuid().toString();
                    item->customInstanceId = instanceId;

                    if (bridge.isRunning())
                    {
                        auto props = bridge.createInstance(desc.customPluginId, instanceId);
                        if (!props.empty())
                            cpc->setPluginProperties(props);

                        // Restore saved property values
                        for (auto& [key, val] : desc.customPluginPropertyValues)
                        {
                            bridge.setProperty(instanceId, key, val);
                            cpc->updatePropertyValue(key, val);
                        }
                    }
                    cpc->setPluginId(desc.customPluginId, instanceId);
                }
            }

            // Apply projectM preset path/settings on project load
            if (desc.type == MeterType::ProjectMVisualizer && item->component)
            {
                item->projectmPresetPath          = desc.projectmPresetPath;
                item->projectmAutoPresetSeconds   = desc.projectmAutoPresetSeconds;
                if (auto* pmc = dynamic_cast<ProjectMComponent*>(item->component.get()))
                {
                    pmc->setAutoPresetSeconds(desc.projectmAutoPresetSeconds);
                    if (desc.projectmPresetPath.isNotEmpty())
                        pmc->setPresetPath(desc.projectmPresetPath);
                }
            }
        }
    }

    // Restore background
    auto& model = canvasEditor.getModel();
    model.background.mode    = result.bgMode;
    model.background.colour1 = result.bgColour1;
    model.background.colour2 = result.bgColour2;
    model.background.angle   = result.bgAngle;
    if (result.bgImagePath.isNotEmpty())
        model.background.imageFile = juce::File(result.bgImagePath);

    // Restore grid
    model.grid.enabled     = result.gridEnabled;
    model.grid.spacing     = result.gridSpacing;
    model.grid.showGrid    = result.showGrid;
    model.grid.showRuler   = result.showRuler;
    model.grid.smartGuides = result.smartGuides;

    model.sortByZOrder();
    model.notifyItemsChanged();
    model.notifyBackgroundChanged();

    currentProjectFile = file;
    AppSettings::getInstance().set(AppSettings::kLastProjectPath,
                                   file.getFullPathName());

    // Re-load skin if referenced
    if (result.skinFilePath.isNotEmpty())
    {
        juce::File skinFile(result.skinFilePath);
        if (skinFile.existsAsFile())
            loadSkin(skinFile);
    }

    // Re-load audio if referenced
    if (result.audioFilePath.isNotEmpty())
    {
        juce::File audioFile(result.audioFilePath);
        if (audioFile.existsAsFile())
            loadAudioFile(audioFile);
    }

    // Frame view to show all loaded elements.
    // Deferred so the canvas view has its final bounds before we compute zoom/pan.
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis]() {
        if (safeThis == nullptr) return;
        safeThis->canvasEditor.getModel().frameToAll(safeThis->canvasEditor.getCanvasView().getBounds());
    });
}

//==============================================================================
// Auto-save timer — uses an elapsed counter inside timerCallback
//==============================================================================
void MainComponent::startAutoSaveTimer(int intervalSec)
{
    autoSaveIntervalMs = intervalSec * 1000;
    autoSaveElapsedMs  = 0;
}

void MainComponent::stopAutoSaveTimer()
{
    autoSaveIntervalMs = 0;
    autoSaveElapsedMs  = 0;
}

void MainComponent::emergencySave()
{
    // Save to a recovery file in the same directory as the executable
    juce::File recoveryFile = juce::File(CrashHandler::getRecoveryFilePath());
    
    // We try to save ONLY the model logic, ignoring skin/audio if complex
    ProjectSerializer::saveToFile(recoveryFile, 
                                  canvasEditor.getModel(), 
                                  juce::File(), 
                                  audioEngine.getLoadedFile());
                                  
    CrashHandler::log("Canvas Model saved to: " + recoveryFile.getFullPathName().toStdString());
}

