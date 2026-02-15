#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

//==============================================================================
/// FFTProcessor performs real-time FFT analysis on incoming audio.
///
/// Audio thread pushes samples into a lock-free FIFO.  The GUI thread calls
/// `getFFTData()` / `getSpectrumData()` to read the latest frequency-domain
/// snapshot.
///
/// Supports configurable FFT orders (10=1024, 11=2048, 12=4096, 13=8192).
class FFTProcessor
{
public:
    static constexpr int kDefaultFFTOrder = 11;  // 2048 samples
    static constexpr int kMaxFFTSize      = 8192;

    FFTProcessor();
    ~FFTProcessor() = default;

    /// Set FFT order (10..13). Resets internal buffers.
    void setFFTOrder(int order);
    int  getFFTOrder() const { return fftOrder; }
    int  getFFTSize() const  { return fftSize; }

    /// Call from the audio thread to push new samples.
    /// Expects interleaved or mono float samples.
    void pushSamples(const float* data, int numSamples);

    /// Call from the GUI thread. Returns true when new FFT data is available.
    bool processNextBlock();

    /// Get the latest magnitude spectrum (linear scale, 0..1 range, fftSize/2 bins).
    const float* getSpectrumData() const { return spectrumData.data(); }
    int getSpectrumSize() const { return fftSize / 2; }

    /// Get the latest magnitude spectrum mapped to logarithmic dB scale (-60..0 dB).
    /// Output is written into `dest`, which must have at least `numBands` elements.
    /// Band boundaries are logarithmically spaced from 20 Hz to 20 kHz.
    void getLogSpectrumBands(float* dest, int numBands, double sampleRate) const;

    /// Reset all buffers
    void reset();

private:
    int fftOrder = kDefaultFFTOrder;
    int fftSize  = 1 << kDefaultFFTOrder;

    std::unique_ptr<juce::dsp::FFT>              fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    // Lock-free FIFO for pushing samples from audio thread
    juce::AbstractFifo fifo { kMaxFFTSize * 2 };
    std::array<float, kMaxFFTSize * 2> fifoBuffer {};

    // FFT working buffers (GUI thread only)
    std::array<float, kMaxFFTSize * 2> fftData {};       // input → output (in-place)
    std::array<float, kMaxFFTSize>     spectrumData {};   // magnitude spectrum

    std::atomic<bool> nextBlockReady { false };

    void computeSpectrum();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFTProcessor)
};
