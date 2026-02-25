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
    // Remove live-input callback if still active
    if (liveInputEnabled_)
        deviceManager.removeAudioCallback(&liveCallback_);

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

    // Store stereo interleaved frames for projectM (ring buffer, drain-on-read)
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
            const juce::SpinLock::ScopedLockType lock(stereoRingLock);
            for (int i = 0; i < numSamples; ++i)
            {
                int pos = stereoRingWrite % kStereoRingFrames;
                stereoRingBuf[static_cast<size_t>(pos * 2)]     = left[i];
                stereoRingBuf[static_cast<size_t>(pos * 2 + 1)] = right[i];
                stereoRingWrite = (stereoRingWrite + 1) % kStereoRingFrames;
                if (stereoRingCount < kStereoRingFrames)
                    ++stereoRingCount;
            }
        }
    }

    // Forward audio data to analysis callback (FFT, levels, etc.)
    {
        const juce::SpinLock::ScopedLockType lock(callbackLock_);
        if (audioBlockCallback)
            audioBlockCallback(bufferToFill);
    }
}

//==============================================================================
void AudioEngine::enableLiveInput(bool enable)
{
    if (enable == liveInputEnabled_) return;
    liveInputEnabled_ = enable;
    liveCallback_.owner = this;

    if (enable)
    {
        // Remove file-playback callback so we don't double-feed analyzers
        deviceManager.removeAudioCallback(&sourcePlayer);

        // Re-init with stereo input enabled
        deviceManager.closeAudioDevice();
        auto err = deviceManager.initialiseWithDefaultDevices(2, 2);
        if (err.isNotEmpty())
            DBG("Live input init error: " + err);

        // Only add the live-input callback (not sourcePlayer)
        deviceManager.addAudioCallback(&liveCallback_);
    }
    else
    {
        deviceManager.removeAudioCallback(&liveCallback_);

        // Restore file-playback callback
        deviceManager.addAudioCallback(&sourcePlayer);
    }
}

double AudioEngine::getLiveInputLatencyMs() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        double inputLat  = device->getInputLatencyInSamples() /
                           juce::jmax(1.0, device->getCurrentSampleRate()) * 1000.0;
        double outputLat = device->getOutputLatencyInSamples() /
                           juce::jmax(1.0, device->getCurrentSampleRate()) * 1000.0;
        return inputLat + outputLat;
    }
    return 0.0;
}

double AudioEngine::getDeviceSampleRate() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();
    return 44100.0;
}

//==============================================================================
juce::StringArray AudioEngine::getAvailableInputDevices() const
{
    juce::StringArray names;
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        names = type->getDeviceNames(true);  // true = input devices
    return names;
}

void AudioEngine::setInputDevice(const juce::String& deviceName)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = deviceName;
    deviceManager.setAudioDeviceSetup(setup, true);
}

//==============================================================================
void AudioEngine::processLiveBlock(const float* left, const float* right, int numSamples)
{
    if (numSamples <= 0) return;

    // Raw mono snapshot
    {
        int count = juce::jmin(numSamples, kRawSnapshotSize);
        const juce::SpinLock::ScopedLockType lock(rawSampleLock);
        for (int i = 0; i < count; ++i)
            rawSampleSnapshot[static_cast<size_t>(i)] = (left[i] + right[i]) * 0.5f;
        rawSampleCount = count;
    }

    // Stereo ring buffer
    {
        const juce::SpinLock::ScopedLockType lock(stereoRingLock);
        for (int i = 0; i < numSamples; ++i)
        {
            int pos = stereoRingWrite % kStereoRingFrames;
            stereoRingBuf[static_cast<size_t>(pos * 2)]     = left[i];
            stereoRingBuf[static_cast<size_t>(pos * 2 + 1)] = right[i];
            stereoRingWrite = (stereoRingWrite + 1) % kStereoRingFrames;
            if (stereoRingCount < kStereoRingFrames)
                ++stereoRingCount;
        }
    }

    // Wrap in AudioSourceChannelInfo and forward to analysis callback
    {
        const juce::SpinLock::ScopedLockType lock(callbackLock_);
        if (audioBlockCallback)
        {
            juce::AudioBuffer<float> buf(2, numSamples);
            buf.copyFrom(0, 0, left,  numSamples);
            buf.copyFrom(1, 0, right, numSamples);
            juce::AudioSourceChannelInfo info(&buf, 0, numSamples);
            audioBlockCallback(info);
        }
    }
}

//==============================================================================
void AudioEngine::LiveInputCallback::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* /*outputChannelData*/,
    int /*numOutputChannels*/,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&)
{
    if (owner == nullptr || numInputChannels < 1 || numSamples <= 0) return;
    const float* left  = inputChannelData[0];
    const float* right = numInputChannels >= 2 ? inputChannelData[1] : inputChannelData[0];
    owner->processLiveBlock(left, right, numSamples);
}

void AudioEngine::LiveInputCallback::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) {}
void AudioEngine::LiveInputCallback::audioDeviceStopped() {}

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

//==============================================================================
int AudioEngine::drainStereoFrames(float* destInterleaved, int maxFrames)
{
    if (destInterleaved == nullptr || maxFrames <= 0) return 0;
    const juce::SpinLock::ScopedLockType lock(stereoRingLock);
    int count = juce::jmin(stereoRingCount, maxFrames);
    if (count == 0) return 0;
    // Read from the oldest frame (write ptr - count), wrapping
    int readStart = (stereoRingWrite - count + kStereoRingFrames) % kStereoRingFrames;
    for (int i = 0; i < count; ++i)
    {
        int pos = (readStart + i) % kStereoRingFrames;
        destInterleaved[i * 2]     = stereoRingBuf[static_cast<size_t>(pos * 2)];
        destInterleaved[i * 2 + 1] = stereoRingBuf[static_cast<size_t>(pos * 2 + 1)];
    }
    stereoRingCount = 0;  // drain: next caller starts fresh
    return count;
}
