#pragma once

#include <JuceHeader.h>
#include <memory>
#include "ExportSettings.h"
#include "FFmpegProcess.h"
#include "PostProcessor.h"
#include "../Canvas/CanvasModel.h"
#include "../Canvas/CanvasItem.h"
#include "../Canvas/MeterFactory.h"
#include "../Canvas/PythonPluginBridge.h"
#include "../Canvas/PluginRenderReplayer.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/FFTProcessor.h"
#include "../Audio/LevelAnalyzer.h"
#include "../Audio/LoudnessAnalyzer.h"
#include "../Audio/StereoFieldAnalyzer.h"

//==============================================================================
/// Offline renderer — runs on a background thread, reads audio block-by-block,
/// feeds meters, renders each video frame to an Image, and pipes RGB24 data
/// to FFmpeg (or saves PNG sequence).
class OfflineRenderer : public juce::Thread
{
public:
    //--  Listener for progress / completion  ----------------------------------
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void renderingProgress(float progress, int currentFrame, int totalFrames,
                                       double estimatedSecondsRemaining) = 0;
        virtual void renderingFinished(bool success, const juce::String& message) = 0;
    };

    OfflineRenderer(const Export::Settings& settings,
                    CanvasModel& canvasModel,
                    AudioEngine& audioEngine);
    ~OfflineRenderer() override;

    //-- Thread entry point  ---------------------------------------------------
    void run() override;

    //-- Control  --------------------------------------------------------------
    void pause()    { paused_.store(true); }
    void resume()   { paused_.store(false); }
    void cancel()   { cancelled_.store(true); }

    float getProgress() const   { return progress_.load(); }
    bool  isPaused()    const   { return paused_.load(); }
    bool  isCancelled() const   { return cancelled_.load(); }

    void addListener(Listener* l)    { listeners_.add(l); }
    void removeListener(Listener* l) { listeners_.remove(l); }

    //-- Preview frame (thread-safe)  ------------------------------------------
    /// Returns a downscaled preview of the latest rendered frame, or null Image.
    juce::Image getLatestPreview() const
    {
        const juce::SpinLock::ScopedLockType sl(previewLock_);
        return previewImage_;
    }

private:
    Export::Settings    settings_;
    CanvasModel&        canvasModel_;
    AudioEngine&        audioEngine_;

    // Own offline analysis pipeline (independent from real-time)
    FFTProcessor          offlineFft_;
    LevelAnalyzer         offlineLa_;
    LoudnessAnalyzer      offlineLoud_;
    StereoFieldAnalyzer   offlineStereo_;
    MeterFactory          offlineFactory_;

    // Offscreen items — mirror the canvas layout
    std::vector<CanvasItem> offscreenItems_;

    // FFmpeg process
    FFmpegProcess         ffmpeg_;

    // Post-processing effects
    std::unique_ptr<Export::PostProcessor> postProcessor_;

    // Status
    std::atomic<float>    progress_   { 0.0f };
    std::atomic<bool>     paused_     { false };
    std::atomic<bool>     cancelled_  { false };

    /// Shared flag checked by callAsync lambdas to avoid use-after-free
    std::shared_ptr<std::atomic<bool>> alive_ =
        std::make_shared<std::atomic<bool>>(true);

    juce::ListenerList<Listener> listeners_;

    //-- Internal helpers  -----------------------------------------------------
    void createOffscreenItems();
    void transferComponentSettings(const CanvasItem* src, CanvasItem* dst);
    void processAudioBlock(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate);
    void feedOffscreenMeters();
    void feedOfflinePlugins();
    juce::String buildOfflineAudioJson(const CanvasItem& item);
    void cleanupOfflinePlugins();
    juce::Image renderFrame(int videoWidth, int videoHeight);
    void imageToRGB24(const juce::Image& img, std::vector<uint8_t>& outBuffer);

    void notifyProgress(float prog, int curFrame, int totalFrames, double etaSec);
    void notifyFinished(bool success, const juce::String& msg);

    //-- Offline custom plugin state  ------------------------------------------
    struct OfflinePlugin
    {
        int          itemIndex;
        juce::String offlineInstanceId;
        juce::String manifestId;
        std::vector<PluginRender::RenderCommand> lastCommands;
    };
    std::vector<OfflinePlugin> offlinePlugins_;
    std::vector<float>         offlineWaveformBuf_;  ///< Latest mono waveform for custom plugins
    std::vector<float>         offlineSpectrumBuf_;  ///< Latest spectrum for custom plugins
    float                      renderScale_ = 1.0f;  ///< Content-to-video scale (constant across frames)

    //-- Preview frame  --------------------------------------------------------
    mutable juce::SpinLock     previewLock_;
    juce::Image                previewImage_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OfflineRenderer)
};
