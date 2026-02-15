#include "StereoFieldAnalyzer.h"
#include <cmath>

//==============================================================================
StereoFieldAnalyzer::StereoFieldAnalyzer()
{
    gonioBuffer.resize(kMaxGonioPoints, { 0.0f, 0.0f });
    lissajousBuffer.resize(kMaxGonioPoints, { 0.0f, 0.0f });
}

void StereoFieldAnalyzer::reset()
{
    sumLL = sumRR = sumLR = sumL = sumR = 0.0;
    integrationCounter = 0;
    correlation.store(0.0f, std::memory_order_relaxed);
    balance.store(0.0f, std::memory_order_relaxed);
    midLevel.store(0.0f, std::memory_order_relaxed);
    sideLevel.store(0.0f, std::memory_order_relaxed);
    gonioWritePos.store(0, std::memory_order_relaxed);
    lissajousWritePos.store(0, std::memory_order_relaxed);
}

void StereoFieldAnalyzer::setTrailLength(int length)
{
    gonioTrailLength = juce::jlimit(256, kMaxGonioPoints, length);
}

//==============================================================================
void StereoFieldAnalyzer::processSamples(const float* left, const float* right, int numSamples)
{
    integrationSamples = static_cast<int>(sampleRate * integrationMs / 1000.0);

    for (int i = 0; i < numSamples; ++i)
    {
        float L = left[i];
        float R = right[i];

        // Accumulate for correlation
        sumLL += static_cast<double>(L) * L;
        sumRR += static_cast<double>(R) * R;
        sumLR += static_cast<double>(L) * R;
        sumL  += L;
        sumR  += R;
        integrationCounter++;

        // Write goniometer point (rotate 45°: x = (R-L)/√2, y = (L+R)/√2)
        {
            float mid  = (L + R) * 0.7071067811865476f;  // 1/√2
            float side = (R - L) * 0.7071067811865476f;
            int wp = gonioWritePos.load(std::memory_order_relaxed);
            gonioBuffer[static_cast<size_t>(wp)] = { side, mid };
            gonioWritePos.store((wp + 1) % kMaxGonioPoints, std::memory_order_relaxed);
        }

        // Write Lissajous point (raw L/R)
        {
            int wp = lissajousWritePos.load(std::memory_order_relaxed);
            lissajousBuffer[static_cast<size_t>(wp)] = { L, R };
            lissajousWritePos.store((wp + 1) % kMaxGonioPoints, std::memory_order_relaxed);
        }

        // Compute correlation when integration window is full
        if (integrationCounter >= integrationSamples)
        {
            double n = static_cast<double>(integrationCounter);

            // Pearson correlation coefficient
            double meanL = sumL / n;
            double meanR = sumR / n;
            double varL = sumLL / n - meanL * meanL;
            double varR = sumRR / n - meanR * meanR;
            double cov  = sumLR / n - meanL * meanR;

            float corr = 0.0f;
            if (varL > 1e-12 && varR > 1e-12)
                corr = static_cast<float>(cov / std::sqrt(varL * varR));

            correlation.store(juce::jlimit(-1.0f, 1.0f, corr), std::memory_order_relaxed);

            // Balance: relative L/R level
            float rmsL = static_cast<float>(std::sqrt(std::max(varL, 0.0)));
            float rmsR = static_cast<float>(std::sqrt(std::max(varR, 0.0)));
            float bal = (rmsL + rmsR > 1e-6f)
                ? (rmsR - rmsL) / (rmsL + rmsR)
                : 0.0f;
            balance.store(bal, std::memory_order_relaxed);

            // M/S levels
            float mid  = (rmsL + rmsR) * 0.5f;
            float side = std::fabs(rmsL - rmsR) * 0.5f;
            midLevel.store(mid, std::memory_order_relaxed);
            sideLevel.store(side, std::memory_order_relaxed);

            // Reset integration
            sumLL = sumRR = sumLR = sumL = sumR = 0.0;
            integrationCounter = 0;
        }
    }
}

//==============================================================================
int StereoFieldAnalyzer::getGonioPoints(GonioPoint* dest, int maxPoints) const
{
    int wp = gonioWritePos.load(std::memory_order_relaxed);
    int count = std::min(maxPoints, gonioTrailLength);

    for (int i = 0; i < count; ++i)
    {
        int idx = (wp - count + i + kMaxGonioPoints) % kMaxGonioPoints;
        dest[i] = gonioBuffer[static_cast<size_t>(idx)];
    }
    return count;
}

int StereoFieldAnalyzer::getLissajousPoints(GonioPoint* dest, int maxPoints) const
{
    int wp = lissajousWritePos.load(std::memory_order_relaxed);
    int count = std::min(maxPoints, gonioTrailLength);

    for (int i = 0; i < count; ++i)
    {
        int idx = (wp - count + i + kMaxGonioPoints) % kMaxGonioPoints;
        dest[i] = lissajousBuffer[static_cast<size_t>(idx)];
    }
    return count;
}
