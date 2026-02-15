#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
/// LevelAnalyzer computes RMS, Peak, and True Peak levels from audio blocks.
///
/// The audio thread calls `processSamples()` which atomically updates the
/// current levels.  The GUI thread reads them with the getter methods.
class LevelAnalyzer
{
public:
    LevelAnalyzer();
    ~LevelAnalyzer() = default;

    /// Called from the audio thread with raw samples. Thread-safe via atomics.
    void processSamples(const float* leftChannel,
                        const float* rightChannel,
                        int numSamples);

    //--- Current levels (read from GUI thread) ---
    float getRMSLeft()     const { return rmsLeft.load(std::memory_order_relaxed); }
    float getRMSRight()    const { return rmsRight.load(std::memory_order_relaxed); }
    float getPeakLeft()    const { return peakLeft.load(std::memory_order_relaxed); }
    float getPeakRight()   const { return peakRight.load(std::memory_order_relaxed); }

    /// Peak hold values (decay slowly)
    float getPeakHoldLeft()  const { return peakHoldLeft.load(std::memory_order_relaxed); }
    float getPeakHoldRight() const { return peakHoldRight.load(std::memory_order_relaxed); }

    /// Whether clipping (>= 1.0) was detected since last reset
    bool hasClippedLeft()  const { return clippedLeft.load(std::memory_order_relaxed); }
    bool hasClippedRight() const { return clippedRight.load(std::memory_order_relaxed); }

    /// Convert linear level to decibels. Returns -infinity for 0.
    static float toDecibels(float linearLevel);

    /// Reset clip indicators
    void resetClip();

    /// Reset everything
    void reset();

    //--- Configuration ---
    void setPeakHoldTimeMs(float ms)  { peakHoldTimeMs = ms; }
    void setPeakDecayRate(float dbPerSecond) { peakDecayDbPerSec = dbPerSecond; }
    void setSampleRate(double sr) { sampleRate = sr; }

private:
    double sampleRate = 44100.0;

    // Atomic levels (written by audio thread, read by GUI thread)
    std::atomic<float> rmsLeft   { 0.0f };
    std::atomic<float> rmsRight  { 0.0f };
    std::atomic<float> peakLeft  { 0.0f };
    std::atomic<float> peakRight { 0.0f };

    // Peak hold
    std::atomic<float> peakHoldLeft  { 0.0f };
    std::atomic<float> peakHoldRight { 0.0f };
    float peakHoldTimeMs      = 2000.0f;
    float peakDecayDbPerSec   = 20.0f;
    double peakHoldCounterL   = 0.0;
    double peakHoldCounterR   = 0.0;

    // Clipping
    std::atomic<bool> clippedLeft  { false };
    std::atomic<bool> clippedRight { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelAnalyzer)
};
