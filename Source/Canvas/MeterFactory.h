#pragma once

#include <JuceHeader.h>
#include "CanvasItem.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/FFTProcessor.h"
#include "../Audio/LevelAnalyzer.h"
#include "../Audio/LoudnessAnalyzer.h"
#include "../Audio/StereoFieldAnalyzer.h"
#include "../Skin/SkinModel.h"
#include "PythonPluginBridge.h"  // for AudioSharedMemory

// Forward-declare all meter types to avoid heavy includes in the header.
class MultiBandAnalyzer;
class Spectrogram;
class Goniometer;
class LissajousScope;
class LoudnessMeter;
class LevelHistogram;
class CorrelationMeter;
class PeakMeter;
class SkinnedSpectrumAnalyzer;
class SkinnedVUMeter;
class SkinnedOscilloscope;
class WinampSkinRenderer;
class WaveformView;

//==============================================================================
/// Creates Component instances for each MeterType and provides a
/// per-frame update method that pushes current audio analysis data
/// into every live meter.
class MeterFactory
{
public:
    MeterFactory(AudioEngine& ae, FFTProcessor& fft, LevelAnalyzer& la,
                 LoudnessAnalyzer& loud, StereoFieldAnalyzer& stereo);

    /// Create a new Component for the given meter type.
    /// The caller takes ownership. Returns nullptr on failure.
    std::unique_ptr<juce::Component> createMeter(MeterType type);

    /// Push current frame data into a single item's component.
    void feedMeter(CanvasItem& item);

    /// Apply skin to skinned meters.
    void applySkin(CanvasItem& item, const Skin::SkinModel* skin);

    /// Called when a new audio file is loaded — resets meters that need it.
    void onFileLoaded(double sampleRate);

    /// Callback for when a file needs to be loaded (e.g. eject button).
    /// Set by the owner (MainComponent) to route through proper load flow.
    std::function<void(const juce::File&)> onFileLoadRequested;

private:
    AudioEngine&         audioEngine;
    FFTProcessor&        fftProcessor;
    LevelAnalyzer&       levelAnalyzer;
    LoudnessAnalyzer&    loudnessAnalyzer;
    StereoFieldAnalyzer& stereoAnalyzer;

    /// Shared memory for zero-copy audio transfer to Python plugins
    AudioSharedMemory    audioSHM;
    bool                 shmInitialised = false;
};
