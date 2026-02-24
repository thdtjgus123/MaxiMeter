#include "BPMDetector.h"
#include <cmath>
#include <numeric>

//==============================================================================
BPMDetector::BPMDetector()
{
    prevMag_.fill(0.0f);
    onsets_.fill(0.0f);
    acf_.fill(0.0f);
}

//==============================================================================
void BPMDetector::processSamples(const float* samples, int numSamples, double sampleRate)
{
    if (sampleRate <= 0.0) return;

    hopRate_ = sampleRate / kHopSize;

    for (int i = 0; i < numSamples; ++i)
    {
        // Accumulate into FFT window (overlap 50%)
        if (hopAccum_ < kFFTSize)
            fftBuf_[static_cast<size_t>(hopAccum_)] = samples[i];

        ++hopAccum_;

        if (hopAccum_ >= kFFTSize)
        {
            // Apply Hann window
            std::array<float, kFFTSize * 2> windowed {};
            for (int n = 0; n < kFFTSize; ++n)
            {
                float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * n / (kFFTSize - 1)));
                windowed[static_cast<size_t>(n)] = fftBuf_[static_cast<size_t>(n)] * w;
            }

            // FFT (in-place, complex interleaved)
            fft_.performFrequencyOnlyForwardTransform(windowed.data());

            // Spectral flux: sum of positive magnitude differences
            float flux = 0.0f;
            for (int b = 1; b < kFFTSize / 2; ++b)
            {
                float mag  = windowed[static_cast<size_t>(b)];
                float diff = mag - prevMag_[static_cast<size_t>(b)];
                if (diff > 0.0f) flux += diff;
                prevMag_[static_cast<size_t>(b)] = mag;
            }

            // Normalise flux and store onset
            flux /= kFFTSize;
            onsets_[static_cast<size_t>(onsetWrite_)] = flux;
            onsetWrite_ = (onsetWrite_ + 1) % kOnsetBuf;
            if (onsetCount_ < kOnsetBuf) ++onsetCount_;

            // Run autocorrelation periodically
            ++hopsSinceACF_;
            if (hopsSinceACF_ >= kACFIntervalHops)
            {
                hopsSinceACF_ = 0;
                if (manualBPM_ <= 0.0f)
                    runACF(hopRate_);
            }

            // Update beat phase accumulator
            updateBeatPhase();

            // 50% overlap: copy second half to first half for next frame
            std::copy(fftBuf_.begin() + kHopSize,
                      fftBuf_.begin() + kFFTSize,
                      fftBuf_.begin());
            hopAccum_ = kHopSize;
        }
    }
}

//==============================================================================
void BPMDetector::runACF(double hopRateHz)
{
    if (onsetCount_ < 32) return;

    // Copy onset ring buffer into linear array
    const int N = juce::jmin(onsetCount_, kOnsetBuf);
    std::array<float, kOnsetBuf> buf {};
    for (int i = 0; i < N; ++i)
        buf[static_cast<size_t>(i)] = onsets_[static_cast<size_t>((onsetWrite_ - N + i + kOnsetBuf) % kOnsetBuf)];

    // Normalise
    float maxV = *std::max_element(buf.begin(), buf.begin() + N);
    if (maxV < 1e-6f) return;
    for (int i = 0; i < N; ++i) buf[static_cast<size_t>(i)] /= maxV;

    // Compute autocorrelation for lags corresponding to 60–200 BPM
    const double lagMin = hopRateHz * 60.0 / 200.0;  // shortest period (200 BPM)
    const double lagMax = hopRateHz * 60.0 / 60.0;   // longest period  (60 BPM)
    const int    iMin   = juce::jmax(1, (int)lagMin);
    const int    iMax   = juce::jmin(N - 1, (int)lagMax);

    float bestACF = -1.0f;
    int   bestLag = 0;

    for (int lag = iMin; lag <= iMax; ++lag)
    {
        float acf = 0.0f;
        for (int i = 0; i + lag < N; ++i)
            acf += buf[static_cast<size_t>(i)] * buf[static_cast<size_t>(i + lag)];
        acf /= (float)(N - lag);

        if (acf > bestACF)
        {
            bestACF = acf;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestACF < 0.01f) return;

    // Convert lag in hops to BPM
    double newBPM = hopRateHz * 60.0 / bestLag;
    if (newBPM < 50.0 || newBPM > 220.0) return;

    // Smooth update (80% old, 20% new)
    if (currentBPM_ <= 0.0)
        currentBPM_ = newBPM;
    else
        currentBPM_ = currentBPM_ * 0.8 + newBPM * 0.2;

    bpm_.store((float)currentBPM_, std::memory_order_relaxed);
}

//==============================================================================
void BPMDetector::updateBeatPhase()
{
    double effectiveBPM = (manualBPM_ > 0.0f) ? (double)manualBPM_ : currentBPM_;
    if (effectiveBPM <= 0.0) return;

    // Advance phase by one hop
    const double beatsPerHop = effectiveBPM / (hopRate_ * 60.0);
    phaseAccum_ += beatsPerHop;

    if (phaseAccum_ >= 1.0)
    {
        phaseAccum_ -= std::floor(phaseAccum_);

        // Increment atomic beat counter (polled by VJBPMSync on message thread)
        beatCount_.fetch_add(1, std::memory_order_release);
    }

    beatPhase_.store((float)phaseAccum_, std::memory_order_relaxed);
}

//==============================================================================
void BPMDetector::setManualBPM(float bpm)
{
    manualBPM_ = juce::jmax(0.0f, bpm);
    if (manualBPM_ > 0.0f)
    {
        currentBPM_ = (double)manualBPM_;
        bpm_.store(manualBPM_, std::memory_order_relaxed);
    }
}

//==============================================================================
void BPMDetector::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    tapTimes_.push_back(now);
    while ((int)tapTimes_.size() > kMaxTaps)
        tapTimes_.pop_front();

    if ((int)tapTimes_.size() < 2) return;

    // Average interval between consecutive taps
    double sumInterval = 0.0;
    for (int i = 1; i < (int)tapTimes_.size(); ++i)
        sumInterval += tapTimes_[static_cast<size_t>(i)] - tapTimes_[static_cast<size_t>(i - 1)];
    double avgInterval = sumInterval / (tapTimes_.size() - 1);

    float tappedBPM = (float)(60.0 / avgInterval);
    if (tappedBPM > 40.0f && tappedBPM < 240.0f)
        setManualBPM(tappedBPM);
}
