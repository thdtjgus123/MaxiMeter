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

    //--- Callback for audio blocks (FFT / level analysis) ---
    /// Set a callback that receives raw audio samples from the real-time thread.
    /// The callback MUST be lock-free and non-blocking.
    using AudioBlockCallback = std::function<void(const juce::AudioSourceChannelInfo&)>;
    void setAudioBlockCallback(AudioBlockCallback cb) { audioBlockCallback = std::move(cb); }

    //--- Raw sample snapshot for oscilloscope ---
    /// Copy the latest mono sample snapshot into dest (up to maxSamples).
    /// Returns number of samples actually copied. Thread-safe (SpinLock).
    int getLatestMonoSamples(float* dest, int maxSamples) const;

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

    // Raw sample snapshot for oscilloscope (written by audio thread, read by GUI)
    static constexpr int kRawSnapshotSize = 2048;
    mutable juce::SpinLock         rawSampleLock;
    std::array<float, kRawSnapshotSize> rawSampleSnapshot {};
    int rawSampleCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
