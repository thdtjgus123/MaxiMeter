#include "MeterFactory.h"
#include "CanvasItem.h"
#include "CustomPluginComponent.h"

// Full includes for all meter types
#include "../UI/MultiBandAnalyzer.h"
#include "../UI/Spectrogram.h"
#include "../UI/Goniometer.h"
#include "../UI/LissajousScope.h"
#include "../UI/LoudnessMeter.h"
#include "../UI/LevelHistogram.h"
#include "../UI/CorrelationMeter.h"
#include "../UI/PeakMeter.h"
#include "../UI/SkinnedSpectrumAnalyzer.h"
#include "../UI/SkinnedVUMeter.h"
#include "../UI/SkinnedOscilloscope.h"
#include "../UI/WinampSkinRenderer.h"
#include "../UI/SkinnedPlayerPanel.h"
#include "../UI/EqualizerPanel.h"
#include "../UI/WaveformView.h"
#include "../UI/ImageLayerComponent.h"
#include "../UI/VideoLayerComponent.h"
#include "../UI/ShapeComponent.h"
#include "../UI/TextLabelComponent.h"

//==============================================================================
MeterFactory::MeterFactory(AudioEngine& ae, FFTProcessor& fft, LevelAnalyzer& la,
                           LoudnessAnalyzer& loud, StereoFieldAnalyzer& stereo)
    : audioEngine(ae), fftProcessor(fft), levelAnalyzer(la),
      loudnessAnalyzer(loud), stereoAnalyzer(stereo)
{
    // Initialize shared memory for zero-copy audio transfer to Python plugins
    shmInitialised = audioSHM.create(fft.getFFTSize(), 1024);
    if (shmInitialised)
        DBG("AudioSharedMemory created successfully (FFT=" + juce::String(fft.getFFTSize()) + ")");
    else
        DBG("AudioSharedMemory creation failed — falling back to JSON-only");
}

//==============================================================================
std::unique_ptr<juce::Component> MeterFactory::createMeter(MeterType type)
{
    switch (type)
    {
        case MeterType::MultiBandAnalyzer:
        {
            auto m = std::make_unique<MultiBandAnalyzer>();
            m->setNumBands(31);
            m->setScaleMode(MultiBandAnalyzer::ScaleMode::Logarithmic);
            m->setBarStyle(MultiBandAnalyzer::BarStyle::Filled);
            return m;
        }
        case MeterType::Spectrogram:
        {
            auto m = std::make_unique<::Spectrogram>();
            m->setColourMap(::Spectrogram::ColourMap::Heat);
            m->setScrollDirection(::Spectrogram::ScrollDirection::Horizontal);
            return m;
        }
        case MeterType::Goniometer:
            return std::make_unique<::Goniometer>();

        case MeterType::LissajousScope:
            return std::make_unique<::LissajousScope>();

        case MeterType::LoudnessMeter:
        {
            auto m = std::make_unique<::LoudnessMeter>();
            m->setTargetLUFS(-14.0f);
            return m;
        }
        case MeterType::LevelHistogram:
            return std::make_unique<::LevelHistogram>();

        case MeterType::CorrelationMeter:
            return std::make_unique<::CorrelationMeter>();

        case MeterType::PeakMeter:
        {
            auto m = std::make_unique<::PeakMeter>();
            m->setNumChannels(2);
            m->setPeakHoldTimeMs(2000.0f);
            m->setShowScale(true);
            return m;
        }
        case MeterType::SkinnedSpectrum:
        {
            auto m = std::make_unique<SkinnedSpectrumAnalyzer>();
            m->setNumBands(20);
            return m;
        }
        case MeterType::SkinnedVUMeter:
        {
            auto m = std::make_unique<::SkinnedVUMeter>();
            m->setBallistic(SkinnedVUMeter::Ballistic::VU);
            return m;
        }
        case MeterType::SkinnedOscilloscope:
            return std::make_unique<::SkinnedOscilloscope>();

        case MeterType::WinampSkin:
            return std::make_unique<WinampSkinRenderer>();

        case MeterType::SkinnedPlayer:
        {
            auto m = std::make_unique<SkinnedPlayerPanel>();
            m->setScale(2);
            // Wire transport callbacks to AudioEngine
            m->onPlayClicked  = [this] { audioEngine.play(); };
            m->onPauseClicked = [this] { audioEngine.pause(); };
            m->onStopClicked  = [this] { audioEngine.stop(); };
            m->onPrevClicked  = [this] { audioEngine.setPosition(0.0); };
            m->onNextClicked  = [this] { /* next track stub */ };
            m->onEjectClicked = [this] {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Open Audio File", juce::File(), "*.wav;*.mp3;*.flac;*.ogg;*.aiff");
                chooser->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, chooser](const juce::FileChooser& fc) {
                        if (fc.getResults().size() > 0)
                        {
                            if (onFileLoadRequested)
                                onFileLoadRequested(fc.getResult());
                            else
                                audioEngine.loadFile(fc.getResult());
                        }
                    });
            };
            m->onPositionChanged = [this](double pos) {
                double len = audioEngine.getLengthInSeconds();
                if (len > 0) audioEngine.setPosition(pos * len);
            };
            m->onVolumeChanged = [this](double vol) {
                audioEngine.setGain(static_cast<float>(vol));
            };
            m->onBalanceChanged = [this](double /*bal*/) {
                // Balance/panning not yet supported in AudioEngine
            };
            return m;
        }

        case MeterType::Equalizer:
        {
            auto m = std::make_unique<EqualizerPanel>();
            m->setScale(2);
            return m;
        }

        case MeterType::WaveformView:
            return std::make_unique<::WaveformView>(audioEngine);

        case MeterType::ImageLayer:
            return std::make_unique<ImageLayerComponent>();

        case MeterType::VideoLayer:
            return std::make_unique<VideoLayerComponent>();

        case MeterType::ShapeRectangle:
            return std::make_unique<ShapeComponent>(ShapeType::Rectangle);
        case MeterType::ShapeEllipse:
            return std::make_unique<ShapeComponent>(ShapeType::Ellipse);
        case MeterType::ShapeTriangle:
            return std::make_unique<ShapeComponent>(ShapeType::Triangle);
        case MeterType::ShapeLine:
            return std::make_unique<ShapeComponent>(ShapeType::Line);
        case MeterType::ShapeStar:
            return std::make_unique<ShapeComponent>(ShapeType::Star);
        case MeterType::ShapeSVG:
            return std::make_unique<ShapeComponent>(ShapeType::SVG);

        case MeterType::TextLabel:
            return std::make_unique<TextLabelComponent>();

        case MeterType::CustomPlugin:
            return std::make_unique<CustomPluginComponent>();

        default:
            return nullptr;
    }
}

//==============================================================================
void MeterFactory::feedMeter(CanvasItem& item)
{
    if (!item.component || !item.visible) return;

    // Apply meter colours and blend mode to any MeterBase-derived component
    if (auto* mb = dynamic_cast<MeterBase*>(item.component.get()))
    {
        bool changed = (mb->getMeterBgColour() != item.meterBgColour)
                     || (mb->getMeterFgColour() != item.meterFgColour)
                     || (mb->getBlendMode()     != item.blendMode);

        mb->setMeterBgColour(item.meterBgColour);
        mb->setMeterFgColour(item.meterFgColour);
        mb->setBlendMode(item.blendMode);

        // Apply font settings
        mb->setMeterFontSize(item.fontSize);
        mb->setMeterFontFamily(item.fontFamily);

        if (changed)
        {
            item.component->setOpaque(item.blendMode == BlendMode::Normal
                                       && item.meterBgColour.getAlpha() == 0);
            item.component->repaint();
        }
    }

    auto* comp = item.component.get();
    const int specSize = fftProcessor.getSpectrumSize();
    const double sr = audioEngine.getFileSampleRate();

    switch (item.meterType)
    {
        case MeterType::MultiBandAnalyzer:
            if (specSize > 0)
                static_cast<MultiBandAnalyzer*>(comp)->setSpectrumData(
                    fftProcessor.getSpectrumData(), specSize, sr);
            break;

        case MeterType::Spectrogram:
            if (specSize > 0)
                static_cast<::Spectrogram*>(comp)->pushSpectrum(
                    fftProcessor.getSpectrumData(), specSize);
            break;

        case MeterType::Goniometer:
            static_cast<::Goniometer*>(comp)->update(stereoAnalyzer);
            break;

        case MeterType::LissajousScope:
            static_cast<::LissajousScope*>(comp)->update(stereoAnalyzer);
            break;

        case MeterType::LoudnessMeter:
        {
            auto* m = static_cast<::LoudnessMeter*>(comp);
            m->setMomentaryLUFS(loudnessAnalyzer.getMomentaryLUFS());
            m->setShortTermLUFS(loudnessAnalyzer.getShortTermLUFS());
            m->setIntegratedLUFS(loudnessAnalyzer.getIntegratedLUFS());
            m->setLRA(loudnessAnalyzer.getLRA());
            m->setTruePeakL(loudnessAnalyzer.getTruePeakLeft());
            m->setTruePeakR(loudnessAnalyzer.getTruePeakRight());
            break;
        }
        case MeterType::LevelHistogram:
            static_cast<::LevelHistogram*>(comp)->pushLevel(
                levelAnalyzer.getRMSLeft(), levelAnalyzer.getRMSRight());
            break;

        case MeterType::CorrelationMeter:
            static_cast<::CorrelationMeter*>(comp)->setCorrelation(
                stereoAnalyzer.getCorrelation());
            break;

        case MeterType::PeakMeter:
        {
            auto* m = static_cast<::PeakMeter*>(comp);
            m->setLevel(0, levelAnalyzer.getPeakLeft());
            m->setLevel(1, levelAnalyzer.getPeakRight());
            break;
        }
        case MeterType::SkinnedSpectrum:
            if (specSize > 0)
            {
                auto* m = static_cast<SkinnedSpectrumAnalyzer*>(comp);
                int nb = m->getNumBands();
                std::vector<float> bands(static_cast<size_t>(nb));
                fftProcessor.getLogSpectrumBands(bands.data(), nb, sr);
                m->setSpectrumData(bands.data(), nb);
            }
            break;

        case MeterType::SkinnedVUMeter:
            if (item.vuChannel == 1)
                static_cast<::SkinnedVUMeter*>(comp)->setLevel(levelAnalyzer.getRMSRight());
            else
                static_cast<::SkinnedVUMeter*>(comp)->setLevel(levelAnalyzer.getRMSLeft());
            break;

        case MeterType::SkinnedOscilloscope:
        {
            // Feed oscilloscope with latest raw mono samples from AudioEngine
            std::array<float, 2048> oscBuf {};
            int n = audioEngine.getLatestMonoSamples(oscBuf.data(), static_cast<int>(oscBuf.size()));
            if (n > 0)
                static_cast<::SkinnedOscilloscope*>(comp)->pushSamples(oscBuf.data(), n);
            break;
        }

        case MeterType::WinampSkin:
        {
            auto* r = static_cast<WinampSkinRenderer*>(comp);
            if (r->hasSkin())
            {
                WinampSkinRenderer::PlayState ps;
                if (audioEngine.isPlaying())
                    ps = WinampSkinRenderer::PlayState::Playing;
                else if (audioEngine.isPaused())
                    ps = WinampSkinRenderer::PlayState::Paused;
                else
                    ps = WinampSkinRenderer::PlayState::Stopped;
                r->setPlayState(ps);

                if (sr > 0)
                {
                    double pos = audioEngine.getCurrentPosition();
                    r->setTime(static_cast<int>(pos) / 60, static_cast<int>(pos) % 60);
                }
                r->setTitleText(audioEngine.getLoadedFileName());
            }
            break;
        }
        case MeterType::SkinnedPlayer:
        {
            auto* p = static_cast<SkinnedPlayerPanel*>(comp);
            if (p->hasSkin())
            {
                SkinnedPlayerPanel::PlayState sps;
                if (audioEngine.isPlaying())
                    sps = SkinnedPlayerPanel::PlayState::Playing;
                else if (audioEngine.isPaused())
                    sps = SkinnedPlayerPanel::PlayState::Paused;
                else
                    sps = SkinnedPlayerPanel::PlayState::Stopped;
                p->setPlayState(sps);

                double len = audioEngine.getLengthInSeconds();
                if (len > 0)
                {
                    double pos = audioEngine.getCurrentPosition();
                    p->setPosition(pos / len);
                    int minutes = static_cast<int>(pos) / 60;
                    int seconds = static_cast<int>(pos) % 60;
                    p->setTime(minutes, seconds);
                }
                p->setTitleText(audioEngine.getLoadedFileName());

                // Feed spectrum
                if (specSize > 0)
                {
                    float bands20[20] = {};
                    int binsPer = specSize / 20;
                    if (binsPer < 1) binsPer = 1;
                    for (int b = 0; b < 20; ++b)
                    {
                        float sum = 0.0f;
                        for (int j = 0; j < binsPer; ++j)
                        {
                            int idx = b * binsPer + j;
                            if (idx < specSize)
                                sum += fftProcessor.getSpectrumData()[idx];
                        }
                        float avg = sum / static_cast<float>(binsPer);
                        bands20[b] = (avg > 1e-10f) ? 20.0f * std::log10(avg) : -60.0f;
                    }
                    p->setSpectrumData(bands20, 20);
                }

                // Feed oscilloscope
                {
                    float oscBuf[512];
                    int n = audioEngine.getLatestMonoSamples(oscBuf, 512);
                    p->setOscilloscopeData(oscBuf, n);
                }
            }
            break;
        }
        case MeterType::Equalizer:
            // Interactive-only — no audio data feeding needed.
            break;

        case MeterType::WaveformView:
            // Self-driven component — no per-frame feed needed.
            break;

        case MeterType::ImageLayer:
        case MeterType::VideoLayer:
            // Static display — no audio data feeding needed.
            break;

        case MeterType::ShapeRectangle:
        case MeterType::ShapeEllipse:
        case MeterType::ShapeTriangle:
        case MeterType::ShapeLine:
        case MeterType::ShapeStar:
        case MeterType::ShapeSVG:
            // Static shapes — no audio data.
            break;

        case MeterType::TextLabel:
            // Static text — no audio data.
            break;

        case MeterType::CustomPlugin:
        {
            // Feed audio data to custom plugin via JSON and shared memory
            auto* cpc = static_cast<CustomPluginComponent*>(comp);

            // Fetch raw audio data once
            const float* pSpectrum = (specSize > 0) ? fftProcessor.getSpectrumData() : nullptr;
            float waveBuf[1024];
            int waveSamples = audioEngine.getLatestMonoSamples(waveBuf, 1024);
            const float* pWaveform = (waveSamples > 0) ? waveBuf : nullptr;

            // ── Write to shared memory (zero-copy path for Python) ──
            if (shmInitialised)
            {
                // Per-channel level data
                audioSHM.writeChannelData(0,
                    levelAnalyzer.getRMSLeft(), levelAnalyzer.getPeakLeft(),
                    levelAnalyzer.getPeakLeft(),
                    levelAnalyzer.getRMSLeft(), levelAnalyzer.getPeakLeft());
                audioSHM.writeChannelData(1,
                    levelAnalyzer.getRMSRight(), levelAnalyzer.getPeakRight(),
                    levelAnalyzer.getPeakRight(),
                    levelAnalyzer.getRMSRight(), levelAnalyzer.getPeakRight());

                if (pSpectrum)
                    audioSHM.writeSpectrum(pSpectrum, fftProcessor.getSpectrumSize());

                if (pWaveform)
                    audioSHM.writeWaveform(pWaveform, waveSamples);

                // Scalar frame data + increment frame counter
                audioSHM.writeFrame(
                    (float)audioEngine.getFileSampleRate(),
                    2,                                       // numChannels
                    audioEngine.isPlaying(),
                    0.0f, 0.0f,                              // position, duration
                    stereoAnalyzer.getCorrelation(),
                    0.0f,                                    // stereoAngle
                    loudnessAnalyzer.getMomentaryLUFS(),
                    loudnessAnalyzer.getShortTermLUFS(),
                    loudnessAnalyzer.getIntegratedLUFS(),
                    loudnessAnalyzer.getLRA(),
                    0.0f, 0.0f                               // bpm, beatPhase
                );
            }

            // ── JSON path (legacy/fallback + metadata) ──
            // Skip expensive JSON serialisation if the worker is throttled.
            // But we must still call feedAudioData to allow worker result pickup.
            if (cpc->isRenderThrottled())
            {
                cpc->feedAudioData({}, shmInitialised,
                                   pSpectrum, specSize,
                                   pWaveform, waveSamples);
                break;
            }

            // Build a lightweight audio JSON snapshot
            juce::DynamicObject::Ptr audioObj = new juce::DynamicObject();

            // Channels array
            juce::Array<juce::var> channelsArr;
            {
                juce::DynamicObject::Ptr leftCh = new juce::DynamicObject();
                leftCh->setProperty("rms", levelAnalyzer.getRMSLeft());
                leftCh->setProperty("peak", levelAnalyzer.getPeakLeft());
                leftCh->setProperty("true_peak", levelAnalyzer.getPeakLeft());
                leftCh->setProperty("rms_linear", levelAnalyzer.getRMSLeft());
                leftCh->setProperty("peak_linear", levelAnalyzer.getPeakLeft());
                channelsArr.add(juce::var(leftCh.get()));

                juce::DynamicObject::Ptr rightCh = new juce::DynamicObject();
                rightCh->setProperty("rms", levelAnalyzer.getRMSRight());
                rightCh->setProperty("peak", levelAnalyzer.getPeakRight());
                rightCh->setProperty("true_peak", levelAnalyzer.getPeakRight());
                rightCh->setProperty("rms_linear", levelAnalyzer.getRMSRight());
                rightCh->setProperty("peak_linear", levelAnalyzer.getPeakRight());
                channelsArr.add(juce::var(rightCh.get()));
            }
            audioObj->setProperty("channels", channelsArr);
            audioObj->setProperty("num_channels", 2);

            // Loudness
            audioObj->setProperty("lufs_momentary", loudnessAnalyzer.getMomentaryLUFS());
            audioObj->setProperty("lufs_short_term", loudnessAnalyzer.getShortTermLUFS());
            audioObj->setProperty("lufs_integrated", loudnessAnalyzer.getIntegratedLUFS());
            audioObj->setProperty("loudness_range", loudnessAnalyzer.getLRA());

            // Stereo
            audioObj->setProperty("correlation", stereoAnalyzer.getCorrelation());

            // Spectrum & Waveform (always included in JSON as per user request)
            if (specSize > 0)
            {
                juce::Array<juce::var> specArr;
                juce::Array<juce::var> specLinArr;
                int numBins = juce::jmin(specSize, 512);
                for (int b = 0; b < numBins; ++b)
                {
                    float mag = pSpectrum ? pSpectrum[b] : 0.0f;
                    specArr.add(mag);
                    specLinArr.add(juce::jlimit(0.0f, 1.0f, mag));
                }
                audioObj->setProperty("spectrum", specArr);
                audioObj->setProperty("spectrum_linear", specLinArr);
                audioObj->setProperty("fft_size", specSize * 2);
            }

            if (waveSamples > 0)
            {
                juce::Array<juce::var> waveArr;
                for (int s = 0; s < waveSamples; ++s)
                    waveArr.add(pWaveform ? pWaveform[s] : 0.0f);
                audioObj->setProperty("waveform", waveArr);
            }

            // Include meter colours so Python plugins can use them
            {
                auto bgC = item.meterBgColour;
                auto fgC = item.meterFgColour;
                juce::DynamicObject::Ptr bgObj = new juce::DynamicObject();
                bgObj->setProperty("r", bgC.getFloatRed());
                bgObj->setProperty("g", bgC.getFloatGreen());
                bgObj->setProperty("b", bgC.getFloatBlue());
                bgObj->setProperty("a", bgC.getFloatAlpha());
                audioObj->setProperty("bg_color", juce::var(bgObj.get()));

                juce::DynamicObject::Ptr fgObj = new juce::DynamicObject();
                fgObj->setProperty("r", fgC.getFloatRed());
                fgObj->setProperty("g", fgC.getFloatGreen());
                fgObj->setProperty("b", fgC.getFloatBlue());
                fgObj->setProperty("a", fgC.getFloatAlpha());
                audioObj->setProperty("fg_color", juce::var(fgObj.get()));
            }

            auto jsonStr = juce::JSON::toString(juce::var(audioObj.get()), true);
            
            // Pass raw pointers for GPU texture upload (avoids JSON parsing in Component)
            cpc->feedAudioData(jsonStr, shmInitialised,
                               pSpectrum, specSize,
                               pWaveform, waveSamples);
            break;
        }

        default: break;
    }
}

//==============================================================================
void MeterFactory::applySkin(CanvasItem& item, const Skin::SkinModel* skin)
{
    if (!item.component || skin == nullptr) return;

    switch (item.meterType)
    {
        case MeterType::MultiBandAnalyzer:
            static_cast<MultiBandAnalyzer*>(item.component.get())->setSkin(skin);
            break;
        case MeterType::SkinnedSpectrum:
            static_cast<SkinnedSpectrumAnalyzer*>(item.component.get())->setSkin(skin);
            break;
        case MeterType::SkinnedVUMeter:
            static_cast<::SkinnedVUMeter*>(item.component.get())->setSkin(skin);
            break;
        case MeterType::SkinnedOscilloscope:
            static_cast<::SkinnedOscilloscope*>(item.component.get())->setSkin(skin);
            break;
        case MeterType::SkinnedPlayer:
            static_cast<SkinnedPlayerPanel*>(item.component.get())->setSkinModel(skin);
            break;
        case MeterType::WinampSkin:
            static_cast<WinampSkinRenderer*>(item.component.get())->setSkinModel(skin);
            break;
        case MeterType::Equalizer:
            static_cast<EqualizerPanel*>(item.component.get())->setSkinModel(skin);
            break;
        default: break;
    }
}

//==============================================================================
void MeterFactory::onFileLoaded(double sampleRate)
{
    // This is called externally — the CanvasEditor iterates items
    // and calls feedMeter already. Here we'd reset spectrograms etc.
    juce::ignoreUnused(sampleRate);
}
