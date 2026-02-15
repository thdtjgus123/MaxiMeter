#include "LevelAnalyzer.h"
#include <cmath>

//==============================================================================
LevelAnalyzer::LevelAnalyzer() {}

//==============================================================================
void LevelAnalyzer::processSamples(const float* leftChannel,
                                   const float* rightChannel,
                                   int numSamples)
{
    if (numSamples <= 0)
        return;

    float sumSqL = 0.0f, sumSqR = 0.0f;
    float peakL  = 0.0f, peakR  = 0.0f;
    bool  clipL  = false, clipR  = false;

    for (int i = 0; i < numSamples; ++i)
    {
        // Left channel
        if (leftChannel != nullptr)
        {
            float sL = leftChannel[i];
            float absL = std::fabs(sL);
            sumSqL += sL * sL;
            if (absL > peakL)
                peakL = absL;
            if (absL >= 1.0f)
                clipL = true;
        }

        // Right channel
        if (rightChannel != nullptr)
        {
            float sR = rightChannel[i];
            float absR = std::fabs(sR);
            sumSqR += sR * sR;
            if (absR > peakR)
                peakR = absR;
            if (absR >= 1.0f)
                clipR = true;
        }
    }

    // RMS
    float rmsL = std::sqrt(sumSqL / static_cast<float>(numSamples));
    float rmsR = std::sqrt(sumSqR / static_cast<float>(numSamples));

    rmsLeft.store(rmsL, std::memory_order_relaxed);
    rmsRight.store(rmsR, std::memory_order_relaxed);

    // Instantaneous peak (with simple smoothing — take max of current and decayed previous)
    float prevPeakL = peakLeft.load(std::memory_order_relaxed);
    float prevPeakR = peakRight.load(std::memory_order_relaxed);

    // Decay: the amount the peak drops per audio block
    float blockDurationSec = static_cast<float>(numSamples) / static_cast<float>(sampleRate);
    float decayLinear = std::pow(10.0f, -peakDecayDbPerSec * blockDurationSec / 20.0f);

    float newPeakL = std::max(peakL, prevPeakL * decayLinear);
    float newPeakR = std::max(peakR, prevPeakR * decayLinear);

    peakLeft.store(newPeakL, std::memory_order_relaxed);
    peakRight.store(newPeakR, std::memory_order_relaxed);

    // Peak hold
    float holdL = peakHoldLeft.load(std::memory_order_relaxed);
    float holdR = peakHoldRight.load(std::memory_order_relaxed);

    double holdDurationBlocks = (peakHoldTimeMs / 1000.0) * sampleRate / numSamples;

    if (peakL >= holdL)
    {
        peakHoldLeft.store(peakL, std::memory_order_relaxed);
        peakHoldCounterL = 0.0;
    }
    else
    {
        peakHoldCounterL += 1.0;
        if (peakHoldCounterL > holdDurationBlocks)
        {
            float decaiedHold = holdL * decayLinear;
            peakHoldLeft.store(decaiedHold, std::memory_order_relaxed);
        }
    }

    if (peakR >= holdR)
    {
        peakHoldRight.store(peakR, std::memory_order_relaxed);
        peakHoldCounterR = 0.0;
    }
    else
    {
        peakHoldCounterR += 1.0;
        if (peakHoldCounterR > holdDurationBlocks)
        {
            float decayedHold = holdR * decayLinear;
            peakHoldRight.store(decayedHold, std::memory_order_relaxed);
        }
    }

    // Clipping
    if (clipL) clippedLeft.store(true, std::memory_order_relaxed);
    if (clipR) clippedRight.store(true, std::memory_order_relaxed);
}

//==============================================================================
float LevelAnalyzer::toDecibels(float linearLevel)
{
    if (linearLevel <= 0.0f)
        return -100.0f;
    return 20.0f * std::log10(linearLevel);
}

void LevelAnalyzer::resetClip()
{
    clippedLeft.store(false, std::memory_order_relaxed);
    clippedRight.store(false, std::memory_order_relaxed);
}

void LevelAnalyzer::reset()
{
    rmsLeft.store(0.0f, std::memory_order_relaxed);
    rmsRight.store(0.0f, std::memory_order_relaxed);
    peakLeft.store(0.0f, std::memory_order_relaxed);
    peakRight.store(0.0f, std::memory_order_relaxed);
    peakHoldLeft.store(0.0f, std::memory_order_relaxed);
    peakHoldRight.store(0.0f, std::memory_order_relaxed);
    peakHoldCounterL = 0.0;
    peakHoldCounterR = 0.0;
    resetClip();
}
