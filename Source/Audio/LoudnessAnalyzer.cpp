#include "LoudnessAnalyzer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

//==============================================================================
LoudnessAnalyzer::LoudnessAnalyzer()
{
    computeFilterCoeffs();
    allBlocks.reserve(10000);
}

void LoudnessAnalyzer::setSampleRate(double sr)
{
    sampleRate = sr;
    blockSizeSamples = static_cast<int>(sr * kBlockSizeMs / 1000.0);
    computeFilterCoeffs();
    reset();
}

void LoudnessAnalyzer::reset()
{
    preFilterL = preFilterR = {};
    rlbL = rlbR = {};
    blockCounter = 0;
    blockSumL = blockSumR = 0.0;
    blockPowersL.clear();
    blockPowersR.clear();
    allBlocks.clear();
    momentaryLUFS.store(-100.0f, std::memory_order_relaxed);
    shortTermLUFS.store(-100.0f, std::memory_order_relaxed);
    integratedLUFS.store(-100.0f, std::memory_order_relaxed);
    lra.store(0.0f, std::memory_order_relaxed);
    truePeakL.store(0.0f, std::memory_order_relaxed);
    truePeakR.store(0.0f, std::memory_order_relaxed);
}

//==============================================================================
// ITU-R BS.1770-4 K-weighting filter coefficients
// Stage 1: High shelf (pre-filter) — boost high frequencies
// Stage 2: High-pass (RLB weighting) — attenuate below ~60Hz
void LoudnessAnalyzer::computeFilterCoeffs()
{
    double fs = sampleRate;

    // --- Stage 1: Pre-filter (high-shelf) ---
    // Coefficients from ITU-R BS.1770-4 for 48kHz reference
    // For other sample rates, we use the bilinear transform approach
    if (fs == 48000.0)
    {
        preFilterCoeffs.b0 =  1.53512485958697;
        preFilterCoeffs.b1 = -2.69169618940638;
        preFilterCoeffs.b2 =  1.19839281085285;
        preFilterCoeffs.a1 = -1.69065929318241;
        preFilterCoeffs.a2 =  0.73248077421585;
    }
    else
    {
        // Pre-filter adapted for arbitrary sample rate
        double db = 3.999843853973347;
        double f0 = 1681.974450955533;
        double Q  = 0.7071752369554196;
        double K  = std::tan(juce::MathConstants<double>::pi * f0 / fs);
        double Vh = std::pow(10.0, db / 20.0);
        double Vb = std::pow(Vh, 0.4996667741545416);
        double a0_ = 1.0 + K / Q + K * K;
        preFilterCoeffs.b0 = (Vh + Vb * K / Q + K * K) / a0_;
        preFilterCoeffs.b1 = 2.0 * (K * K - Vh) / a0_;
        preFilterCoeffs.b2 = (Vh - Vb * K / Q + K * K) / a0_;
        preFilterCoeffs.a1 = 2.0 * (K * K - 1.0) / a0_;
        preFilterCoeffs.a2 = (1.0 - K / Q + K * K) / a0_;
    }

    // --- Stage 2: RLB weighting (high-pass) ---
    if (fs == 48000.0)
    {
        rlbCoeffs.b0 =  1.0;
        rlbCoeffs.b1 = -2.0;
        rlbCoeffs.b2 =  1.0;
        rlbCoeffs.a1 = -1.99004745483398;
        rlbCoeffs.a2 =  0.99007225036621;
    }
    else
    {
        double f0 = 38.13547087602444;
        double Q  = 0.5003270373238773;
        double K  = std::tan(juce::MathConstants<double>::pi * f0 / fs);
        double a0_ = 1.0 + K / Q + K * K;
        rlbCoeffs.b0 = 1.0 / a0_;
        rlbCoeffs.b1 = -2.0 / a0_;
        rlbCoeffs.b2 = 1.0 / a0_;
        rlbCoeffs.a1 = 2.0 * (K * K - 1.0) / a0_;
        rlbCoeffs.a2 = (1.0 - K / Q + K * K) / a0_;
    }
}

float LoudnessAnalyzer::applyBiquad(float sample, BiquadState& state, const BiquadCoeffs& c)
{
    double x = static_cast<double>(sample);
    double y = c.b0 * x + c.b1 * state.x1 + c.b2 * state.x2
                         - c.a1 * state.y1 - c.a2 * state.y2;
    state.x2 = state.x1;
    state.x1 = x;
    state.y2 = state.y1;
    state.y1 = y;
    return static_cast<float>(y);
}

//==============================================================================
void LoudnessAnalyzer::processSamples(const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float sL = left[i];
        float sR = right[i];

        // True peak tracking
        float absL = std::fabs(sL);
        float absR = std::fabs(sR);
        if (absL > truePeakL.load(std::memory_order_relaxed))
            truePeakL.store(absL, std::memory_order_relaxed);
        if (absR > truePeakR.load(std::memory_order_relaxed))
            truePeakR.store(absR, std::memory_order_relaxed);

        // Apply K-weighting
        float kL = applyBiquad(sL, preFilterL, preFilterCoeffs);
        kL = applyBiquad(kL, rlbL, rlbCoeffs);

        float kR = applyBiquad(sR, preFilterR, preFilterCoeffs);
        kR = applyBiquad(kR, rlbR, rlbCoeffs);

        // Accumulate mean square for this block
        blockSumL += static_cast<double>(kL) * kL;
        blockSumR += static_cast<double>(kR) * kR;
        blockCounter++;

        if (blockCounter >= blockSizeSamples)
        {
            double meanSqL = blockSumL / blockSizeSamples;
            double meanSqR = blockSumR / blockSizeSamples;
            processBlock(meanSqL, meanSqR);
            blockSumL = blockSumR = 0.0;
            blockCounter = 0;
        }
    }
}

void LoudnessAnalyzer::processBlock(double meanSqL, double meanSqR)
{
    blockPowersL.push_back(meanSqL);
    blockPowersR.push_back(meanSqR);

    // Keep enough blocks for short-term (30 blocks = 3s)
    while (static_cast<int>(blockPowersL.size()) > kShortTermBlocks)
    {
        blockPowersL.pop_front();
        blockPowersR.pop_front();
    }

    // --- Momentary LUFS (400ms = 4 blocks) ---
    {
        int n = std::min(kMomentaryBlocks, static_cast<int>(blockPowersL.size()));
        double sumL = 0, sumR = 0;
        for (int i = static_cast<int>(blockPowersL.size()) - n;
             i < static_cast<int>(blockPowersL.size()); ++i)
        {
            sumL += blockPowersL[static_cast<size_t>(i)];
            sumR += blockPowersR[static_cast<size_t>(i)];
        }
        double meanPower = (sumL + sumR) / n;
        float lufs = (meanPower > 1e-10)
            ? static_cast<float>(-0.691 + 10.0 * std::log10(meanPower))
            : -100.0f;
        momentaryLUFS.store(lufs, std::memory_order_relaxed);
    }

    // --- Short-term LUFS (3s = 30 blocks) ---
    {
        int n = static_cast<int>(blockPowersL.size());
        double sumL = 0, sumR = 0;
        for (int i = 0; i < n; ++i)
        {
            sumL += blockPowersL[static_cast<size_t>(i)];
            sumR += blockPowersR[static_cast<size_t>(i)];
        }
        double meanPower = (sumL + sumR) / n;
        float lufs = (meanPower > 1e-10)
            ? static_cast<float>(-0.691 + 10.0 * std::log10(meanPower))
            : -100.0f;
        shortTermLUFS.store(lufs, std::memory_order_relaxed);
    }

    // --- Integrated (gated) ---
    allBlocks.push_back({ meanSqL, meanSqR });

    double integrated = computeGatedLoudness();
    integratedLUFS.store(static_cast<float>(integrated), std::memory_order_relaxed);

    double lraVal = computeLRA();
    lra.store(static_cast<float>(lraVal), std::memory_order_relaxed);
}

//==============================================================================
// EBU R128 gated loudness: two-pass gating
double LoudnessAnalyzer::computeGatedLoudness() const
{
    if (allBlocks.empty()) return -100.0;

    // Pass 1: absolute gate at -70 LUFS
    double absGateThreshold = -70.0;
    std::vector<double> ungatedPowers;
    ungatedPowers.reserve(allBlocks.size());

    for (const auto& b : allBlocks)
    {
        double power = b.powerL + b.powerR;
        double lufs = -0.691 + 10.0 * std::log10(std::max(power, 1e-20));
        if (lufs > absGateThreshold)
            ungatedPowers.push_back(power);
    }

    if (ungatedPowers.empty()) return -100.0;

    // Mean of ungated blocks
    double meanPower = std::accumulate(ungatedPowers.begin(), ungatedPowers.end(), 0.0) / ungatedPowers.size();
    double relativeGateThreshold = -0.691 + 10.0 * std::log10(std::max(meanPower, 1e-20)) - 10.0;

    // Pass 2: relative gate at (ungated mean - 10 LU)
    double gatedSum = 0;
    int gatedCount = 0;
    for (const auto& b : allBlocks)
    {
        double power = b.powerL + b.powerR;
        double lufs = -0.691 + 10.0 * std::log10(std::max(power, 1e-20));
        if (lufs > relativeGateThreshold)
        {
            gatedSum += power;
            gatedCount++;
        }
    }

    if (gatedCount == 0) return -100.0;
    double gatedMean = gatedSum / gatedCount;
    return -0.691 + 10.0 * std::log10(std::max(gatedMean, 1e-20));
}

// LRA = difference between 10th and 95th percentile of short-term block loudness
double LoudnessAnalyzer::computeLRA() const
{
    if (allBlocks.size() < 2) return 0.0;

    // Compute short-term loudness for each position (30-block sliding window)
    std::vector<double> stLoudnesses;
    stLoudnesses.reserve(allBlocks.size());

    for (size_t end = 0; end < allBlocks.size(); ++end)
    {
        size_t start = (end >= static_cast<size_t>(kShortTermBlocks))
            ? end - static_cast<size_t>(kShortTermBlocks) + 1 : 0;
        size_t count = end - start + 1;

        double sumP = 0;
        for (size_t i = start; i <= end; ++i)
            sumP += allBlocks[i].powerL + allBlocks[i].powerR;

        double meanP = sumP / static_cast<double>(count);
        double lufs = -0.691 + 10.0 * std::log10(std::max(meanP, 1e-20));

        // Apply absolute gate at -70 LUFS
        if (lufs > -70.0)
            stLoudnesses.push_back(lufs);
    }

    if (stLoudnesses.size() < 2) return 0.0;

    // Apply relative gate at (ungated mean - 20 LU)
    double meanST = std::accumulate(stLoudnesses.begin(), stLoudnesses.end(), 0.0) / stLoudnesses.size();
    double relGate = meanST - 20.0;

    std::vector<double> gated;
    gated.reserve(stLoudnesses.size());
    for (double l : stLoudnesses)
    {
        if (l > relGate)
            gated.push_back(l);
    }

    if (gated.size() < 2) return 0.0;

    std::sort(gated.begin(), gated.end());

    size_t p10 = static_cast<size_t>(gated.size() * 0.10);
    size_t p95 = static_cast<size_t>(gated.size() * 0.95);
    p10 = std::min(p10, gated.size() - 1);
    p95 = std::min(p95, gated.size() - 1);

    return gated[p95] - gated[p10];
}
