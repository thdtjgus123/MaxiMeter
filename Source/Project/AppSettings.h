#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Persistent application settings — singleton backed by PropertiesFile.
/// Stores user preferences that survive across sessions.
class AppSettings
{
public:
    static AppSettings& getInstance()
    {
        static AppSettings instance;
        return instance;
    }

    //-- Generic accessors ----------------------------------------------------
    int    getInt   (const juce::String& key, int    fallback = 0)     const { return props->getIntValue(key, fallback); }
    bool   getBool  (const juce::String& key, bool   fallback = false) const { return props->getBoolValue(key, fallback); }
    double getDouble(const juce::String& key, double fallback = 0.0)  const { return props->getDoubleValue(key, fallback); }
    juce::String getString(const juce::String& key, const juce::String& fallback = {}) const { return props->getValue(key, fallback); }

    void set(const juce::String& key, int    v) { props->setValue(key, v);      save(); }
    void set(const juce::String& key, bool   v) { props->setValue(key, v);      save(); }
    void set(const juce::String& key, double v) { props->setValue(key, v);      save(); }
    void set(const juce::String& key, const juce::String& v) { props->setValue(key, v); save(); }

    //-- Well-known keys (static strings) -------------------------------------

    // General
    static constexpr const char* kLanguage         = "general.language";
    static constexpr const char* kAutoSave         = "general.autoSave";
    static constexpr const char* kAutoSaveInterval = "general.autoSaveIntervalSec";
    static constexpr const char* kOpenLastProject  = "general.openLastProject";
    static constexpr const char* kLastProjectPath  = "general.lastProjectPath";
    static constexpr const char* kCheckUpdates     = "general.checkUpdates";
    static constexpr const char* kUndoHistorySize  = "general.undoHistorySize";
    static constexpr const char* kShowWelcome      = "general.showWelcome";
    static constexpr const char* kShowSplashScreen = "general.showSplashScreen";

    // Appearance
    static constexpr const char* kTheme            = "appearance.theme";
    static constexpr const char* kAccentColour     = "appearance.accentColour";
    static constexpr const char* kUIScale           = "appearance.uiScale";
    static constexpr const char* kToolboxViewMode  = "appearance.toolboxViewMode";
    static constexpr const char* kShowStatusBar    = "appearance.showStatusBar";
    static constexpr const char* kShowMiniMap      = "appearance.showMiniMap";
    static constexpr const char* kAnimateTransitions = "appearance.animateTransitions";

    // Canvas
    static constexpr const char* kDefaultGridEnabled  = "canvas.gridEnabled";
    static constexpr const char* kDefaultGridSpacing  = "canvas.gridSpacing";
    static constexpr const char* kDefaultShowGrid     = "canvas.showGrid";
    static constexpr const char* kDefaultShowRuler    = "canvas.showRuler";
    static constexpr const char* kDefaultSmartGuides  = "canvas.smartGuides";
    static constexpr const char* kDefaultGridColour   = "canvas.gridColour";
    static constexpr const char* kDefaultCanvasWidth  = "canvas.defaultWidth";
    static constexpr const char* kDefaultCanvasHeight = "canvas.defaultHeight";
    static constexpr const char* kSnapToGrid          = "canvas.snapToGrid";
    static constexpr const char* kHandleSize          = "canvas.handleSize";

    // Performance
    static constexpr const char* kFpsThreshold          = "performance.fpsThreshold";
    static constexpr const char* kTimerRateHz           = "performance.timerRateHz";
    static constexpr const char* kGpuAcceleration       = "performance.gpuAcceleration";
    static constexpr const char* kHiDpiRendering        = "performance.hiDpiRendering";
    static constexpr const char* kPlaceholderModeEnabled = "performance.placeholderModeEnabled";

    // Audio
    static constexpr const char* kAudioDevice       = "audio.device";
    static constexpr const char* kSampleRate        = "audio.sampleRate";
    static constexpr const char* kBufferSize        = "audio.bufferSize";
    static constexpr const char* kMasterGain        = "audio.masterGain";

    // Export
    static constexpr const char* kFFmpegPath         = "export.ffmpegPath";
    static constexpr const char* kDefaultResolution  = "export.defaultResolution";
    static constexpr const char* kDefaultFrameRate   = "export.defaultFrameRate";
    static constexpr const char* kDefaultVideoCodec  = "export.defaultVideoCodec";
    static constexpr const char* kDefaultAudioCodec  = "export.defaultAudioCodec";
    static constexpr const char* kDefaultQuality     = "export.defaultQuality";
    static constexpr const char* kDefaultOutputDir   = "export.defaultOutputDir";

    //-- Convenience typed accessors ------------------------------------------

    float getFpsThreshold()  const { return (float)getDouble(kFpsThreshold, 20.0); }
    int   getTimerRateHz()   const { return getInt(kTimerRateHz, 60); }
    int   getGridSpacing()   const { return getInt(kDefaultGridSpacing, 10); }
    bool  getAutoSave()      const { return getBool(kAutoSave, false); }
    int   getAutoSaveIntervalSec() const { return getInt(kAutoSaveInterval, 300); }
    float getUIScale()       const { return (float)getDouble(kUIScale, 100.0); }
    float getMasterGain()    const { return (float)getDouble(kMasterGain, 1.0); }

    juce::String getFFmpegPath() const { return getString(kFFmpegPath); }

    /// Raw PropertiesFile for juce::AudioDeviceManager state load/save.
    juce::PropertiesFile* getPropertiesFile() { return props.get(); }

    void save() { props->saveIfNeeded(); }

private:
    AppSettings()
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = "MaxiMeter";
        opts.filenameSuffix      = ".settings";
        opts.folderName          = "MaxiMeter";
        opts.osxLibrarySubFolder = "Application Support";
        opts.storageFormat       = juce::PropertiesFile::storeAsXML;

        props = std::make_unique<juce::PropertiesFile>(opts);
    }

    std::unique_ptr<juce::PropertiesFile> props;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
};
