#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

//==============================================================================
/// StereoFieldAnalyzer — real-time stereo correlation and goniometer data.
///
/// Computes:
/// - Phase correlation coefficient (-1 to +1)
/// - Goniometer (L/R scatter) point buffer for rendering
/// - M/S balance
///
/// Audio thread pushes samples; GUI thread reads via atomics + snapshot buffer.
class StereoFieldAnalyzer
{
public:
    StereoFieldAnalyzer();
    ~StereoFieldAnalyzer() = default;

    void setSampleRate(double sr) { sampleRate = sr; }

    /// Called from the audio thread
    void processSamples(const float* left, const float* right, int numSamples);

    //--- Correlation (GUI thread) ---
    float getCorrelation()     const { return correlation.load(std::memory_order_relaxed); }
    float getBalance()         const { return balance.load(std::memory_order_relaxed); }
    float getMidLevel()        const { return midLevel.load(std::memory_order_relaxed); }
    float getSideLevel()       const { return sideLevel.load(std::memory_order_relaxed); }

    /// Set correlation integration time in ms (50-5000)
    void setIntegrationTimeMs(float ms) { integrationMs = juce::jlimit(50.0f, 5000.0f, ms); }

    //--- Goniometer point buffer (GUI thread snapshot) ---
    struct GonioPoint { float x; float y; };

    /// Get a snapshot of the goniometer points for rendering.
    /// Call from GUI thread. Returns the number of points written.
    int getGonioPoints(GonioPoint* dest, int maxPoints) const;

    /// Set how many points are stored for trail/afterglow (256-8192)
    void setTrailLength(int length);

    //--- Lissajous buffer (raw L/R pairs for XY display) ---
    int getLissajousPoints(GonioPoint* dest, int maxPoints) const;

    void reset();

private:
    double sampleRate = 44100.0;
    float integrationMs = 300.0f;

    // Running sums for correlation calculation (audio thread)
    double sumLL = 0, sumRR = 0, sumLR = 0;
    double sumL = 0, sumR = 0;
    int integrationSamples = 0;
    int integrationCounter = 0;

    // Outputs
    std::atomic<float> correlation { 0.0f };
    std::atomic<float> balance     { 0.0f };
    std::atomic<float> midLevel    { 0.0f };
    std::atomic<float> sideLevel   { 0.0f };

    // Goniometer ring buffer (lock-free via atomic index)
    static constexpr int kMaxGonioPoints = 8192;
    std::vector<GonioPoint> gonioBuffer;
    std::atomic<int> gonioWritePos { 0 };
    int gonioTrailLength = 2048;

    // Lissajous ring buffer (raw L/R)
    std::vector<GonioPoint> lissajousBuffer;
    std::atomic<int> lissajousWritePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoFieldAnalyzer)
};
