#pragma once

#include <JuceHeader.h>
#include "../Export/ExportSettings.h"
#include "../Export/FFmpegProcess.h"
#include "ThreeDWebView.h"

//==============================================================================
/// Background thread that drives frame-by-frame 3D video export.
///
/// For each frame the thread posts an `exportFrame` event to the JS side via
/// the JUCE/WebView bridge, waits for the `frameReady` reply (PNG), decodes it,
/// and pipes raw RGB24 data to an FFmpeg child process.
///
/// Threading contract
/// ------------------
///   * `run()` lives on the background thread.
///   * `receiveFrameData()` is called on the JUCE message thread from the
///     `frameReady` event listener registered in `ThreeDWebView`.
///   * Listener callbacks are dispatched on the message thread.
class ThreeDOfflineRenderer : public juce::Thread
{
public:
    //==========================================================================
    struct Listener
    {
        virtual ~Listener() = default;

        /// Called on the message thread, roughly once per frame.
        virtual void threed_renderProgress (float progress,
                                            int   currentFrame,
                                            int   totalFrames,
                                            double etaSeconds) = 0;

        /// Called on the message thread when the render finishes or fails.
        virtual void threed_renderFinished (bool success,
                                            const juce::String& message) = 0;
    };

    //==========================================================================
    ThreeDOfflineRenderer (const Export::Settings& settings,
                           ThreeDWebView&           webView);
    ~ThreeDOfflineRenderer() override;

    void addListener    (Listener* l);
    void removeListener (Listener* l);

    //--------------------------------------------------------------------------
    /// Called on the message thread by ThreeDWebView's `frameReady` handler.
    void receiveFrameData (const juce::String& base64png, int frameIndex);

    /// Returns the most recently decoded frame (for preview thumbnail).
    juce::Image getLatestPreview();

private:
    //==========================================================================
    void run() override;

    void readAudioFrame (int frameIndex, int fps,
                         double sampleRate, juce::int64 totalSamples,
                         std::vector<float>& waveOut,
                         std::vector<float>& specOut);

    static std::vector<std::uint8_t> imageToRGB24 (const juce::Image& img);

    void notifyProgress (float progress, int currentFrame, int totalFrames, double eta);
    void notifyFinished (bool success, const juce::String& msg);

    //==========================================================================
    Export::Settings settings_;
    ThreeDWebView&   webView_;

    juce::AudioFormatManager               formatManager_;
    std::unique_ptr<juce::AudioFormatReader> reader_;

    // Frame-sync between background thread and message thread
    juce::WaitableEvent frameEvent_  { true /*manualReset*/ };
    juce::CriticalSection previewLock_;
    juce::Image           latestPreview_;

    juce::ListenerList<Listener> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeDOfflineRenderer)
};
