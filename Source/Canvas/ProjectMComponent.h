#pragma once

/**
 * @file ProjectMComponent.h
 * @brief JUCE component wrapping the libprojectM Milkdrop visualizer.
 *
 * Uses a shared off-screen OpenGL context (ProjectMGLRenderer singleton,
 * defined in the .cpp) to render projectM presets.  Like CustomPluginComponent,
 * all actual screen output goes through paint() via a juce::Image readback so
 * this component is a lightweight JUCE child — no native HWND, no z-order issues.
 *
 * Audio PCM is fed by calling AudioEngine::drainStereoFrames() each GL frame
 * and forwarding the resulting stereo-interleaved float data to
 * projectm_pcm_add_float().
 *
 * LIFECYCLE:
 *   1. Created by MeterFactory::createMeter(MeterType::ProjectMVisualizer).
 *   2. setPresetPath() called whenever the user picks a preset folder.
 *   3. Each frame: ProjectMGLRenderer calls renderFrame_GL() on the GL thread.
 *   4. paint() draws the readback juce::Image.
 */

#include <JuceHeader.h>
#include "../UI/MeterBase.h"
#include "../Audio/AudioEngine.h"

//==============================================================================
class ProjectMComponent : public juce::Component,
                          public MeterBase
{
public:
    explicit ProjectMComponent(AudioEngine& ae);
    ~ProjectMComponent() override;

    //-- Settings ---------------------------------------------------------------

    /// Set the folder that will be scanned for .milk preset files.
    /// Can be called from any thread; applied on the next GL frame.
    void setPresetPath(const juce::String& folderPath);
    juce::String getPresetPath() const;

    /// Advance to the next preset in the playlist.
    void nextPreset();

    /// Go back to the previous preset.
    void prevPreset();

    /// Auto-cycle interval in seconds (0 = manual only).
    void setAutoPresetSeconds(int secs);
    int  getAutoPresetSeconds() const;

    /// Returns the display name of the currently playing preset.
    /// Thread-safe (may be called from the GUI thread).
    juce::String getCurrentPresetName() const;

    //-- Component interface ---------------------------------------------------
    void paint(juce::Graphics& g) override;
    void resized() override;

    //-- Called by ProjectMGLRenderer on the GL thread ------------------------
    void initGLResources_GL();
    void renderFrame_GL();
    void releaseGLResources_GL();

    bool glInitialised = false;

private:
    AudioEngine& audioEngine_;

    // Settings (written from GUI, read on GL thread via atomic/lock)
    juce::CriticalSection settingsLock_;
    juce::String          presetPath_;
    std::atomic<bool>     presetPathDirty_   { false };
    std::atomic<bool>     autoPresetDirty_   { false };
    std::atomic<bool>     nextPreset_        { false };
    std::atomic<bool>     prevPreset_        { false };
    std::atomic<int>      autoPresetSecs_    { 0 };
    std::atomic<int>      pendingWidth_    { 0 };
    std::atomic<int>      pendingHeight_   { 0 };
    std::atomic<bool>     sizeChanged_     { false };

    // GL-thread resources (only touched on GL thread)
    void*  pmHandle_       = nullptr;   ///< projectm_handle (void* to avoid header dep)
    void*  playlistHandle_ = nullptr;   ///< projectm_playlist_handle
    int    fboWidth_       = 0;
    int    fboHeight_      = 0;

    // Readback image (written on GL thread, read on GUI thread)
    juce::CriticalSection imageLock_;
    juce::Image           readbackImage_;

    // Current preset name (written on GL thread, read on GUI thread)
    mutable juce::CriticalSection presetNameLock_;
    juce::String                  currentPresetName_;

    // PCM drain buffer (reused each frame, stack-allocated on GL thread)
    static constexpr int kPCMDrainFrames = 2048;

    void applyPresetPath_GL(const juce::String& path);
    void readbackFBO_GL();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectMComponent)
};
