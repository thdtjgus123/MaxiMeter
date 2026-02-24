#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <deque>

//==============================================================================
/// Real-time BPM detector using spectral flux onset detection + autocorrelation.
///
/// Usage:
///   - Call processSamples() from the audio callback thread.
///   - Read getBPM() / getBeatPhase() from the UI thread (atomic, lock-free).
///   - Set onBeat callback to receive beat pulses on the message thread.
///   - Call setManualBPM(bpm) to override detection; call clearManualBPM() to resume.
///   - Call tap() to tap-tempo input.
class BPMDetector
{
public:
    BPMDetector();
    ~BPMDetector() = default;

    //==========================================================================
    /// Called from the audio thread with a mono buffer.
    void processSamples(const float* samples, int numSamples, double sampleRate);

    //==========================================================================
    /// Current estimated BPM (0 if not yet detected). Thread-safe.
    float getBPM() const noexcept { return bpm_.load(std::memory_order_relaxed); }

    /// Beat phase 0..1 (0 = on the beat, 1 = about to beat). Thread-safe.
    float getBeatPhase() const noexcept { return beatPhase_.load(std::memory_order_relaxed); }

    /// True when a valid BPM estimate is available.
    bool  isDetected() const noexcept { return getBPM() > 0.0f; }

    //==========================================================================
    /// Override automatic detection with a fixed BPM. Pass 0 to clear.
    void setManualBPM(float bpm);
    void clearManualBPM() { setManualBPM(0.0f); }
    bool isManualOverride() const noexcept { return manualBPM_ > 0.0f; }

    //==========================================================================
    /// Tap tempo — call on every tap. Averages last 4 taps.
    void tap();

    //==========================================================================
    /// Monotonically increasing beat counter. Poll from a timer (message thread)
    /// to detect new beats by comparing with a cached value. Lock-free.
    int getBeatCount() const noexcept { return beatCount_.load(std::memory_order_acquire); }

private:
    //==========================================================================
    // Onset detection —  spectral flux over a 512-point FFT
    static constexpr int kFFTOrder  = 9;           // 2^9 = 512
    static constexpr int kFFTSize   = 1 << kFFTOrder;
    static constexpr int kHopSize   = kFFTSize / 2;

    juce::dsp::FFT fft_ { kFFTOrder };

    std::array<float, kFFTSize * 2> fftBuf_ {};
    std::array<float, kFFTSize / 2 + 1> prevMag_ {};
    int hopAccum_ = 0;

    // Ring buffer of onset strength values (one per hop)
    static constexpr int kOnsetBuf = 512;
    std::array<float, kOnsetBuf> onsets_ {};
    int onsetWrite_ = 0;
    int onsetCount_ = 0;

    // Autocorrelation scratch buffer
    std::array<float, kOnsetBuf> acf_ {};

    // How many hops between autocorrelation re-runs
    static constexpr int kACFIntervalHops = 32;
    int hopsSinceACF_ = 0;

    //==========================================================================
    // BPM state
    std::atomic<float> bpm_       { 0.0f };
    std::atomic<float> beatPhase_ { 0.0f };
    std::atomic<int>   beatCount_ { 0 };

    float manualBPM_ = 0.0f;

    // Beat tracking — phase accumulator
    double currentBPM_    = 0.0;
    double phaseAccum_    = 0.0;  // beats (fractional)
    double hopRate_       = 0.0;  // hops per second

    //==========================================================================
    // Tap tempo
    static constexpr int kMaxTaps = 8;
    std::deque<double> tapTimes_;

    //==========================================================================
    void runACF(double hopRateHz);
    void updateBeatPhase();
};
