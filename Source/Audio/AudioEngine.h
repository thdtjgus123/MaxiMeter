#pragma once

#include <JuceHeader.h>

//==============================================================================
/// AudioEngine manages audio file loading, decoding, and playback.
/// It owns the format manager, transport source, and device manager.
///
/// Thread safety: The audio callback runs on a real-time thread. All state
/// shared with the GUI goes through lock-free mechanisms.
class AudioEngine : public juce::AudioSource,
                    public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    //--- File I/O ---
    bool loadFile(const juce::File& file);
    void unloadFile();
    juce::String getLoadedFileName() const;
    juce::File getLoadedFile() const { return currentFile; }
    double getFileSampleRate() const;
    juce::int64 getTotalLengthInSamples() const;
    double getLengthInSeconds() const;

    //--- Transport ---
    void play();
    void pause();
    void stop();
    void setPosition(double positionInSeconds);
    double getCurrentPosition() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isFileLoaded() const;

    //--- Volume ---
    void setGain(float gain);    ///< 0.0 .. 1.0+
    float getGain() const;

    //--- AudioSource interface ---
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    //--- Audio device ---
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    /// Current audio device sample rate (always valid when device is open).
    double getDeviceSampleRate() const;

    //--- Callback for audio blocks (FFT / level analysis) ---
    /// Set a callback that receives raw audio samples from the real-time thread.
    /// The callback MUST be lock-free and non-blocking.
    using AudioBlockCallback = std::function<void(const juce::AudioSourceChannelInfo&)>;
    void setAudioBlockCallback(AudioBlockCallback cb) { audioBlockCallback = std::move(cb); }

    //--- Live audio input (microphone / DJ interface) ---
    void enableLiveInput(bool enable);
    bool isLiveInputEnabled() const noexcept { return liveInputEnabled_; }
    double getLiveInputLatencyMs() const;

    /// Get available audio input device names for the current device type.
    juce::StringArray getAvailableInputDevices() const;

    /// Set the active input device by name.  Empty string = system default.
    void setInputDevice(const juce::String& deviceName);

    //--- Raw sample snapshot for oscilloscope ---
    /// Copy the latest mono sample snapshot into dest (up to maxSamples).
    /// Returns number of samples actually copied. Thread-safe (SpinLock).
    int getLatestMonoSamples(float* dest, int maxSamples) const;

    //--- Stereo PCM drain for projectM visualizer ---
    /// Drain all samples accumulated since the last call into dest as
    /// stereo-interleaved floats [L0,R0,L1,R1,...].  Returns the number
    /// of stereo *frames* written (not bytes/samples).  Thread-safe.
    int drainStereoFrames(float* destInterleaved, int maxFrames);

    //--- Change listener (transport state) ---
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    /// Listeners for transport state changes
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void transportStateChanged(bool isPlaying) {}
        virtual void fileLoaded(const juce::String& fileName, double lengthSeconds) {}
    };
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    juce::AudioDeviceManager       deviceManager;
    juce::AudioFormatManager       formatManager;
    juce::AudioSourcePlayer        sourcePlayer;

    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource     transportSource;

    juce::File                     currentFile;
    double                         fileSampleRate  = 0.0;
    juce::int64                    totalSamples    = 0;
    bool                           paused_         = false;

    AudioBlockCallback             audioBlockCallback;
    juce::ListenerList<Listener>   listeners;

    // Live input
    bool                           liveInputEnabled_ = false;

    // Live input callback captures hardware input and routes through the same pipeline
    struct LiveInputCallback : juce::AudioIODeviceCallback
    {
        AudioEngine* owner = nullptr;
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* /*outputChannelData*/,
                                              int /*numOutputChannels*/,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext&) override;
        void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
    } liveCallback_;

    void processLiveBlock(const float* left, const float* right, int numSamples);

    // Raw sample snapshot for oscilloscope (written by audio thread, read by GUI)
    static constexpr int kRawSnapshotSize = 2048;
    mutable juce::SpinLock         rawSampleLock;
    std::array<float, kRawSnapshotSize> rawSampleSnapshot {};
    int rawSampleCount = 0;

    // Stereo interleaved ring buffer for projectM (drain-on-read)
    // Stores up to kStereoRingFrames frames as [L,R] pairs.
    static constexpr int kStereoRingFrames = 8192;
    mutable juce::SpinLock         stereoRingLock;
    std::array<float, kStereoRingFrames * 2> stereoRingBuf {};
    int stereoRingWrite = 0;   ///< next write position (in frames)
    int stereoRingCount = 0;   ///< frames currently stored

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
