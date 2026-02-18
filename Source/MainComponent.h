#pragma once

#include <JuceHeader.h>
#include "Audio/AudioEngine.h"
#include "Audio/FFTProcessor.h"
#include "Audio/LevelAnalyzer.h"
#include "Audio/LoudnessAnalyzer.h"
#include "Audio/StereoFieldAnalyzer.h"
#include "UI/TransportBar.h"
#include "UI/WaveformView.h"
#include "UI/StatusBar.h"
#include "UI/WinampSkinRenderer.h"
#include "UI/ThemeManager.h"
#include "UI/KeyboardShortcutManager.h"
#include "UI/SkinnedTitleBarLookAndFeel.h"
#include "Canvas/CanvasEditor.h"
#include "UI/LogWindow.h"
#include "Project/ProjectSerializer.h"

//==============================================================================
/// Main content component — hosts transport, waveform, status bar, and canvas editor.
/// Owns the AudioEngine, analysis pipeline (FFT, Level, Loudness, Stereo), and skin data.
/// Stage 5: All meters are now managed through the CanvasEditor.
/// Stage 6: Video export via FFmpeg pipe.
/// Stage 7: Theme, shortcuts, project save/load, preset templates.
class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::DragAndDropContainer,
                      public juce::FileDragAndDropTarget,
                      public ThemeManager::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;

    /// Load an audio file (called from menu or drag-drop)
    void loadAudioFile(const juce::File& file);

    /// Load a Winamp skin (.wsz or folder)
    void loadSkin(const juce::File& skinFile);

    /// Access the audio engine
    AudioEngine& getAudioEngine() { return audioEngine; }

    /// Access the canvas editor
    CanvasEditor& getCanvasEditor() { return canvasEditor; }

    /// Stage 6: Open export video dialog (Ctrl+E)
    void exportVideo();

    void showSettings();

    /// Apply settings changes live (called from SettingsWindow callback)
    void applyLiveSettings();

    // File drag-and-drop
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // ThemeManager::Listener
    void themeChanged(AppTheme newTheme) override;

    // Stage 7: Project management (public for menu access)
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void loadPresetTemplate(int index);

    /// Emergency save handler
    void emergencySave();
    /// Show the log window
    void showLogWindow() { logWindow.setVisible(true); logWindow.toFront(true); }

private:
    // Splash screen state
    std::unique_ptr<juce::Component> splashOverlay;

    // Audio pipeline
    AudioEngine           audioEngine;
    FFTProcessor          fftProcessor;
    LevelAnalyzer         levelAnalyzer;
    LoudnessAnalyzer      loudnessAnalyzer;
    StereoFieldAnalyzer   stereoAnalyzer;

    // Skin state
    bool                  skinLoaded = false;
    WinampSkinRenderer    winampRenderer;   // kept for skin loading/parsing

    // UI sections
    TransportBar          transportBar;
    WaveformView          waveformView;
    StatusBar             statusBar;

    // Stage 5: Canvas editor (owns all meters)
    CanvasEditor          canvasEditor;
    LogWindow             logWindow;

    // Stage 7: Keyboard shortcuts
    KeyboardShortcutManager shortcutManager;

    // Stage 7: Current project file
    juce::File            currentProjectFile;

    // Auto-save state
    int autoSaveIntervalMs = 0;
    int autoSaveElapsedMs  = 0;

    void setupLayout();
    void showExportDialog();

    // Stage 7: Wire up shortcut actions
    void setupShortcuts();

    /// Shared project-loading logic used by openProject() and startup
    void loadProjectResult(const juce::File& file,
                           const ProjectSerializer::LoadResult& result);

    /// Auto-save timer control
    void startAutoSaveTimer(int intervalSec);
    void stopAutoSaveTimer();

    /// Add an image/video/GIF file to the canvas as a media layer
    void addMediaToCanvas(const juce::File& file, MeterType type);

    /// Look-and-feel for child dialog windows
    SkinnedTitleBarLookAndFeel exportDialogLnf_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
