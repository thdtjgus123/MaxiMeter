#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <deque>

//==============================================================================
/// LoudnessAnalyzer — ITU-R BS.1770-4 / EBU R128 compliant loudness measurement.
///
/// Implements K-weighting filter (Stage 1: pre-filter, Stage 2: RLB weighting),
/// gated loudness calculation, and LRA measurement.
///
/// Output: Momentary (400ms), Short-term (3s), Integrated LUFS, LRA.
/// Audio thread pushes samples; GUI thread reads results via atomics.
class LoudnessAnalyzer
{
public:
    LoudnessAnalyzer();
    ~LoudnessAnalyzer() = default;

    /// Set sample rate (must be called before processing)
    void setSampleRate(double sr);

    /// Called from the audio thread with interleaved L/R samples
    void processSamples(const float* left, const float* right, int numSamples);

    //--- Results (read from GUI thread) ---
    float getMomentaryLUFS()   const { return momentaryLUFS.load(std::memory_order_relaxed); }
    float getShortTermLUFS()   const { return shortTermLUFS.load(std::memory_order_relaxed); }
    float getIntegratedLUFS()  const { return integratedLUFS.load(std::memory_order_relaxed); }
    float getLRA()             const { return lra.load(std::memory_order_relaxed); }
    float getTruePeakLeft()    const { return truePeakL.load(std::memory_order_relaxed); }
    float getTruePeakRight()   const { return truePeakR.load(std::memory_order_relaxed); }

    /// Reset all measurements
    void reset();

    //--- Target loudness (for display reference) ---
    void setTargetLUFS(float target) { targetLUFS = target; }
    float getTargetLUFS() const { return targetLUFS; }

private:
    double sampleRate = 48000.0;

    //--- K-weighting filter state (per channel) ---
    struct BiquadState
    {
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    };
    struct BiquadCoeffs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    // Stage 1: shelving pre-filter
    BiquadCoeffs preFilterCoeffs;
    BiquadState  preFilterL, preFilterR;

    // Stage 2: RLB weighting (high-pass)
    BiquadCoeffs rlbCoeffs;
    BiquadState  rlbL, rlbR;

    void computeFilterCoeffs();
    float applyBiquad(float sample, BiquadState& state, const BiquadCoeffs& coeffs);

    //--- Gated block measurement (100ms blocks, 75% overlap for Momentary) ---
    static constexpr int kBlockSizeMs = 100;
    int blockSizeSamples = 4800;  // 100ms at 48kHz
    int blockCounter = 0;
    double blockSumL = 0.0;
    double blockSumR = 0.0;

    // Circular buffer of 100ms mean-square values
    std::deque<double> blockPowersL;
    std::deque<double> blockPowersR;

    // For Momentary (400ms = 4 blocks)
    static constexpr int kMomentaryBlocks = 4;

    // For Short-term (3s = 30 blocks)
    static constexpr int kShortTermBlocks = 30;

    // For Integrated (all gated blocks)
    struct GatedBlock { double powerL; double powerR; };
    std::vector<GatedBlock> allBlocks;

    void processBlock(double meanSqL, double meanSqR);
    double computeGatedLoudness() const;
    double computeLRA() const;

    //--- Outputs (atomic for cross-thread reading) ---
    std::atomic<float> momentaryLUFS  { -100.0f };
    std::atomic<float> shortTermLUFS  { -100.0f };
    std::atomic<float> integratedLUFS { -100.0f };
    std::atomic<float> lra            { 0.0f };
    std::atomic<float> truePeakL      { 0.0f };
    std::atomic<float> truePeakR      { 0.0f };

    float targetLUFS = -14.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoudnessAnalyzer)
};
