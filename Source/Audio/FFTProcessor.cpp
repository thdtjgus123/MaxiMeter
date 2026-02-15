#include "FFTProcessor.h"

//==============================================================================
FFTProcessor::FFTProcessor()
{
    setFFTOrder(kDefaultFFTOrder);
}

void FFTProcessor::setFFTOrder(int order)
{
    jassert(order >= 10 && order <= 13);
    fftOrder = juce::jlimit(10, 13, order);
    fftSize  = 1 << fftOrder;

    fft    = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        static_cast<size_t>(fftSize),
        juce::dsp::WindowingFunction<float>::hann
    );

    reset();
}

//==============================================================================
void FFTProcessor::pushSamples(const float* data, int numSamples)
{
    // Write into the lock-free FIFO
    const auto scope = fifo.write(numSamples);

    if (scope.blockSize1 > 0)
        std::memcpy(fifoBuffer.data() + scope.startIndex1, data,
                     sizeof(float) * static_cast<size_t>(scope.blockSize1));
    if (scope.blockSize2 > 0)
        std::memcpy(fifoBuffer.data() + scope.startIndex2,
                     data + scope.blockSize1,
                     sizeof(float) * static_cast<size_t>(scope.blockSize2));
}

//==============================================================================
bool FFTProcessor::processNextBlock()
{
    // Check if we have enough samples for one FFT frame
    if (fifo.getNumReady() < fftSize)
        return false;

    // Read fftSize samples from the FIFO
    {
        const auto scope = fifo.read(fftSize);

        if (scope.blockSize1 > 0)
            std::memcpy(fftData.data(),
                         fifoBuffer.data() + scope.startIndex1,
                         sizeof(float) * static_cast<size_t>(scope.blockSize1));
        if (scope.blockSize2 > 0)
            std::memcpy(fftData.data() + scope.blockSize1,
                         fifoBuffer.data() + scope.startIndex2,
                         sizeof(float) * static_cast<size_t>(scope.blockSize2));
    }

    computeSpectrum();
    return true;
}

//==============================================================================
void FFTProcessor::computeSpectrum()
{
    // Apply windowing function
    window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));

    // Zero-pad the second half (required for in-place real FFT)
    std::fill(fftData.begin() + fftSize, fftData.begin() + fftSize * 2, 0.0f);

    // Perform forward FFT
    fft->performRealOnlyForwardTransform(fftData.data());

    // Compute magnitude spectrum (normalized)
    const float invSize = 1.0f / static_cast<float>(fftSize);
    const int halfSize = fftSize / 2;

    for (int i = 0; i < halfSize; ++i)
    {
        float re = fftData[static_cast<size_t>(i * 2)];
        float im = fftData[static_cast<size_t>(i * 2 + 1)];
        float mag = std::sqrt(re * re + im * im) * invSize * 2.0f;
        spectrumData[static_cast<size_t>(i)] = mag;
    }
}

//==============================================================================
void FFTProcessor::getLogSpectrumBands(float* dest, int numBands, double sampleRate) const
{
    if (sampleRate <= 0.0 || numBands <= 0)
        return;

    const int halfSize = fftSize / 2;
    const double nyquist = sampleRate * 0.5;
    const double minFreq = 20.0;
    const double maxFreq = std::min(20000.0, nyquist);
    const double logMin  = std::log10(minFreq);
    const double logMax  = std::log10(maxFreq);

    for (int band = 0; band < numBands; ++band)
    {
        // Logarithmically spaced band boundaries
        double bandLowFreq  = std::pow(10.0, logMin + (logMax - logMin) * band / numBands);
        double bandHighFreq = std::pow(10.0, logMin + (logMax - logMin) * (band + 1) / numBands);

        int binLow  = static_cast<int>(std::floor(bandLowFreq  / nyquist * halfSize));
        int binHigh = static_cast<int>(std::ceil(bandHighFreq / nyquist * halfSize));

        binLow  = juce::jlimit(0, halfSize - 1, binLow);
        binHigh = juce::jlimit(binLow, halfSize - 1, binHigh);

        // Average the magnitudes in this band
        float sum = 0.0f;
        int count = 0;
        for (int b = binLow; b <= binHigh; ++b)
        {
            sum += spectrumData[static_cast<size_t>(b)];
            ++count;
        }

        float avg = (count > 0) ? (sum / static_cast<float>(count)) : 0.0f;

        // Convert to dB (clamped to -60..0)
        float db = (avg > 0.0f) ? (20.0f * std::log10(avg)) : -60.0f;
        dest[band] = juce::jlimit(-60.0f, 0.0f, db);
    }
}

//==============================================================================
void FFTProcessor::reset()
{
    fifo.reset();
    fifoBuffer.fill(0.0f);
    fftData.fill(0.0f);
    spectrumData.fill(0.0f);
    nextBlockReady.store(false);
}
