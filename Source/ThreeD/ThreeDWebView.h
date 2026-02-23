#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Embeds a Three.js WebGL scene + Monaco shader editor inside a JUCE
/// WebBrowserComponent (Edge WebView2).
/// Files are served from  <exe>/assets/threed/  via a JUCE ResourceProvider.
class ThreeDWebView : public juce::Component
{
public:
    ThreeDWebView();
    ~ThreeDWebView() override;

    //==========================================================================
    /// Push audio data to the JS scene (call at ~30 Hz from message thread).
    void setAudioData (const std::vector<float>& waveformMono,
                       const std::vector<float>& spectrumMag);

    /// Notify the JS scene of a theme change.
    void applyTheme (bool isDark);

    /// Tell the JS scene to reset the OrbitControls camera.
    void resetCamera();

    /// Request a PNG screenshot of the Three.js canvas from the JS side.
    /// When the frame arrives, onCaptureDone is called on the message thread.
    void requestCapture();

    /// Drive one export frame: inject time + audio, capture, reply via onFrameReady.
    /// Must be called on the JUCE message thread.
    void exportFrame (int frameIndex, int totalFrames, double timeSeconds,
                      const std::vector<float>& waveform,
                      const std::vector<float>& spectrum);

    /// Signal the JS side that export has finished (restores live mode).
    void exportStop();

    /// Set by ThreeDEditor — called when a frame capture completes.
    std::function<void (const juce::Image&)> onCaptureDone;

    /// Set by ThreeDOfflineRenderer — called when an export frame PNG arrives.
    /// Invoked on the JUCE message thread.
    std::function<void (const juce::String& base64png, int frameIndex)> onFrameReady;

    // juce::Component
    void resized() override;

private:
    //==========================================================================
    // Inner WebBrowserComponent subclass so we can override callbacks
    struct Browser : public juce::WebBrowserComponent
    {
        explicit Browser (const Options& opts)
            : juce::WebBrowserComponent (opts) {}

        void pageFinishedLoading (const juce::String& url) override;
        bool pageLoaded_ = false;
    };

    std::unique_ptr<Browser> browser_;
    juce::File assetsDir_;
    bool       bridgeReady_ = false;

    void setupBrowser();

    /// Serve a file from  assetsDir_ / requestPath
    std::optional<juce::WebBrowserComponent::Resource>
        serveResource (const juce::String& requestPath);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeDWebView)
};
