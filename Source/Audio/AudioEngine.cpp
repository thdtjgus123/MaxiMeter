#include "AudioEngine.h"

//==============================================================================
AudioEngine::AudioEngine()
{
    // Register all built-in audio formats (WAV, AIFF, FLAC, OGG, + platform-specific)
    formatManager.registerBasicFormats();

    // Set up audio device with default stereo output
    auto err = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (err.isNotEmpty())
        DBG("Audio device init error: " + err);

    deviceManager.addAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(this);

    transportSource.addChangeListener(this);
}

AudioEngine::~AudioEngine()
{
    transportSource.removeChangeListener(this);
    sourcePlayer.setSource(nullptr);
    deviceManager.removeAudioCallback(&sourcePlayer);
    transportSource.setSource(nullptr);
    readerSource.reset();
}

//==============================================================================
bool AudioEngine::loadFile(const juce::File& file)
{
    // Stop current playback
    stop();
    paused_ = false;
    transportSource.setSource(nullptr);
    readerSource.reset();

    // Try to create a reader for this file
    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
    {
        DBG("Failed to create reader for: " + file.getFullPathName());
        return false;
    }

    currentFile    = file;
    fileSampleRate = reader->sampleRate;
    totalSamples   = reader->lengthInSamples;

    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);

    DBG("Loaded: " + file.getFileName()
        + " | SR: " + juce::String(fileSampleRate)
        + " | Samples: " + juce::String(totalSamples)
        + " | Duration: " + juce::String(getLengthInSeconds(), 2) + "s");

    listeners.call([&](Listener& l) {
        l.fileLoaded(file.getFileName(), getLengthInSeconds());
    });

    return true;
}

void AudioEngine::unloadFile()
{
    stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
    currentFile = {};
    fileSampleRate = 0.0;
    totalSamples = 0;
}

juce::String AudioEngine::getLoadedFileName() const
{
    return currentFile.getFileName();
}

double AudioEngine::getFileSampleRate() const { return fileSampleRate; }
juce::int64 AudioEngine::getTotalLengthInSamples() const { return totalSamples; }

double AudioEngine::getLengthInSeconds() const
{
    if (fileSampleRate > 0.0)
        return static_cast<double>(totalSamples) / fileSampleRate;
    return 0.0;
}

//==============================================================================
void AudioEngine::play()
{
    if (readerSource != nullptr)
    {
        transportSource.start();
        paused_ = false;
    }
}

void AudioEngine::pause()
{
    if (transportSource.isPlaying())
    {
        transportSource.stop();
        paused_ = true;
    }
}

void AudioEngine::stop()
{
    transportSource.stop();
    transportSource.setPosition(0.0);
    paused_ = false;
}

void AudioEngine::setPosition(double positionInSeconds)
{
    transportSource.setPosition(positionInSeconds);
}

double AudioEngine::getCurrentPosition() const
{
    return transportSource.getCurrentPosition();
}

bool AudioEngine::isPlaying() const
{
    return transportSource.isPlaying();
}

bool AudioEngine::isPaused() const
{
    return paused_ && !transportSource.isPlaying();
}

bool AudioEngine::isFileLoaded() const
{
    return readerSource != nullptr;
}

//==============================================================================
void AudioEngine::setGain(float gain)
{
    transportSource.setGain(gain);
}

float AudioEngine::getGain() const
{
    return transportSource.getGain();
}

//==============================================================================
void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioEngine::releaseResources()
{
    transportSource.releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transportSource.getNextAudioBlock(bufferToFill);

    // Store raw mono sample snapshot for oscilloscope
    {
        auto* buffer = bufferToFill.buffer;
        int numSamples = bufferToFill.numSamples;
        int startSample = bufferToFill.startSample;
        if (buffer != nullptr && buffer->getNumChannels() >= 1 && numSamples > 0)
        {
            const float* left  = buffer->getReadPointer(0, startSample);
            const float* right = buffer->getNumChannels() >= 2
                                     ? buffer->getReadPointer(1, startSample)
                                     : left;
            int count = juce::jmin(numSamples, kRawSnapshotSize);
            const juce::SpinLock::ScopedLockType lock(rawSampleLock);
            for (int i = 0; i < count; ++i)
                rawSampleSnapshot[static_cast<size_t>(i)] = (left[i] + right[i]) * 0.5f;
            rawSampleCount = count;
        }
    }

    // Forward audio data to analysis callback (FFT, levels, etc.)
    if (audioBlockCallback)
        audioBlockCallback(bufferToFill);
}

//==============================================================================
void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    bool playing = transportSource.isPlaying();
    listeners.call([playing](Listener& l) {
        l.transportStateChanged(playing);
    });
}

//==============================================================================
int AudioEngine::getLatestMonoSamples(float* dest, int maxSamples) const
{
    if (dest == nullptr || maxSamples <= 0) return 0;
    const juce::SpinLock::ScopedLockType lock(rawSampleLock);
    int count = juce::jmin(rawSampleCount, maxSamples);
    for (int i = 0; i < count; ++i)
        dest[i] = rawSampleSnapshot[static_cast<size_t>(i)];
    return count;
}
