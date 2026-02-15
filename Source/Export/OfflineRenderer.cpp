#include "OfflineRenderer.h"

// Full meter includes for offscreen painting
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
#include "../UI/ShapeComponent.h"
#include "../UI/TextLabelComponent.h"
#include "../UI/ImageLayerComponent.h"
#include "../UI/VideoLayerComponent.h"
#include "../Canvas/CustomPluginComponent.h"

#include <cmath>
#include <algorithm>

//==============================================================================
OfflineRenderer::OfflineRenderer(const Export::Settings& settings,
                                 CanvasModel& canvasModel,
                                 AudioEngine& audioEngine)
    : juce::Thread("OfflineRenderer"),
      settings_(settings),
      canvasModel_(canvasModel),
      audioEngine_(audioEngine),
      offlineFactory_(audioEngine, offlineFft_, offlineLa_, offlineLoud_, offlineStereo_)
{
}

OfflineRenderer::~OfflineRenderer()
{
    alive_->store(false);
    stopThread(10000);
    cleanupOfflinePlugins();
}

//==============================================================================
void OfflineRenderer::run()
{
    //-- 1. Open audio file ---------------------------------------------------
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(settings_.audioFile));

    if (!reader)
    {
        notifyFinished(false, "Cannot open audio file: " + settings_.audioFile.getFullPathName());
        return;
    }

    const double sampleRate   = reader->sampleRate;
    const int64_t totalSamples = static_cast<int64_t>(reader->lengthInSamples);
    const int numChannels     = static_cast<int>(reader->numChannels);

    //-- 2. Initialise offline analysis pipeline  -----------------------------
    offlineFft_.reset();
    offlineLa_.setSampleRate(sampleRate);
    offlineLa_.reset();
    offlineLoud_.setSampleRate(sampleRate);
    offlineLoud_.reset();
    offlineStereo_.setSampleRate(sampleRate);
    offlineStereo_.reset();

    //-- 3. Create offscreen items mirroring canvas  --------------------------
    createOffscreenItems();

    // Set sample rate on any spectrograms
    for (auto& item : offscreenItems_)
    {
        if (item.meterType == MeterType::Spectrogram && item.component)
            static_cast<Spectrogram*>(item.component.get())->setSampleRate(sampleRate);
    }

    //-- 4. Compute video geometry  -------------------------------------------
    const int videoW = settings_.getWidth();
    const int videoH = settings_.getHeight();
    const int fps    = settings_.getFPS();
    const double duration = static_cast<double>(totalSamples) / sampleRate;
    const int totalFrames = static_cast<int>(std::ceil(duration * fps));

    // Store timing info for transport-aware meters
    fps_          = fps;
    fileDuration_ = duration;
    sampleRate_   = sampleRate;
    fileName_     = settings_.audioFile.getFileNameWithoutExtension();

    //-- 4b. Pre-compute render scale (constant across all frames)  -----------
    renderScale_ = 1.0f;
    if (!offscreenItems_.empty())
    {
        juce::Rectangle<float> contentRect = offscreenItems_[0].getBounds();
        for (size_t ci = 1; ci < offscreenItems_.size(); ++ci)
            contentRect = contentRect.getUnion(offscreenItems_[ci].getBounds());
        contentRect = contentRect.expanded(10.0f);
        float sx = static_cast<float>(videoW) / contentRect.getWidth();
        float sy = static_cast<float>(videoH) / contentRect.getHeight();
        renderScale_ = std::min(sx, sy);
    }

    if (totalFrames <= 0)
    {
        notifyFinished(false, "Audio file is too short.");
        return;
    }

    //-- 5. Start FFmpeg (or prepare PNG output dir)  -------------------------
    const bool isPngSequence = (settings_.videoCodec == Export::VideoCodec::PNG_Sequence);

    if (isPngSequence)
    {
        if (!settings_.outputFile.exists())
            settings_.outputFile.createDirectory();
    }
    else
    {
        if (!ffmpeg_.start(settings_))
        {
            notifyFinished(false,
                "Failed to start FFmpeg. Make sure ffmpeg.exe is in your PATH or next to the application.");
            return;
        }
    }

    //-- 5b. Create post-processor  -------------------------------------------
    if (settings_.postProcess.hasAnyEffect())
        postProcessor_ = std::make_unique<Export::PostProcessor>(
            settings_.postProcess, videoW, videoH, fps);

    //-- 6. Allocate RGB buffer for frame output  -----------------------------
    const size_t frameBytes = static_cast<size_t>(videoW) * videoH * 3;
    std::vector<uint8_t> rgbBuffer(frameBytes);

    // Temporary audio buffer (enough for one video frame worth of audio)
    const int samplesPerFrame = static_cast<int>(std::ceil(sampleRate / fps));
    juce::AudioBuffer<float> audioBuf(std::max(numChannels, 2), samplesPerFrame + 512);

    auto startTime = juce::Time::getMillisecondCounterHiRes();

    //-- 7. Main render loop  -------------------------------------------------
    for (int frame = 0; frame < totalFrames; ++frame)
    {
        // Check for cancellation / exit
        if (threadShouldExit() || cancelled_.load())
        {
            cleanupOfflinePlugins();
            notifyFinished(false, "Export cancelled.");
            ffmpeg_.finish();
            return;
        }

        // Handle pause
        while (paused_.load())
        {
            wait(100);
            if (threadShouldExit() || cancelled_.load())
            {
                cleanupOfflinePlugins();
                notifyFinished(false, "Export cancelled.");
                ffmpeg_.finish();
                return;
            }
        }

        //-- 7a. Determine which audio samples belong to this frame -----------
        const int64_t frameSampleStart = static_cast<int64_t>(
            static_cast<double>(frame) * sampleRate / fps);
        const int64_t frameSampleEnd = static_cast<int64_t>(
            static_cast<double>(frame + 1) * sampleRate / fps);
        int samplesToRead = static_cast<int>(
            std::min(frameSampleEnd - frameSampleStart,
                     totalSamples - frameSampleStart));

        if (samplesToRead <= 0)
            samplesToRead = 0;

        //-- 7b. Read & analyse audio  ----------------------------------------
        currentFrame_ = frame;
        if (samplesToRead > 0)
        {
            audioBuf.setSize(std::max(numChannels, 2), samplesToRead, false, false, true);
            audioBuf.clear();
            reader->read(&audioBuf, 0, samplesToRead, frameSampleStart,
                         true, numChannels >= 2);

            processAudioBlock(audioBuf, samplesToRead, sampleRate);
        }

        // Process FFT bins on this thread (normally done on GUI thread)
        while (offlineFft_.processNextBlock()) {}

        //-- 7c. Feed all offscreen meters  -----------------------------------
        feedOffscreenMeters();

        //-- 7c'. Feed custom plugin instances via bridge  --------------------
        feedOfflinePlugins();

        //-- 7d. Render frame to Image  ---------------------------------------
        auto frameImage = renderFrame(videoW, videoH);

        //-- 7d'. Post-processing effects  ------------------------------------
        if (postProcessor_)
        {
            // Feed current audio RMS to drive beat-reactive effects
            float rmsL = offlineLa_.getRMSLeft();
            float rmsR = offlineLa_.getRMSRight();
            postProcessor_->setCurrentRMS((rmsL + rmsR) * 0.5f);
            postProcessor_->processFrame(frameImage);
        }

        //-- 7e. Send to FFmpeg or save PNG  ----------------------------------
        if (isPngSequence)
        {
            auto pngFile = settings_.pngFramePath(frame);
            juce::FileOutputStream fos(pngFile);
            if (fos.openedOk())
            {
                juce::PNGImageFormat png;
                png.writeImageToStream(frameImage, fos);
            }
        }
        else
        {
            imageToRGB24(frameImage, rgbBuffer);
            if (!ffmpeg_.writeFrame(rgbBuffer.data(), frameBytes))
            {
                cleanupOfflinePlugins();
                notifyFinished(false,
                    "FFmpeg pipe write failed at frame " + juce::String(frame)
                    + ". " + ffmpeg_.getErrorOutput());
                ffmpeg_.finish();
                return;
            }
        }

        //-- 7f. Update progress  ---------------------------------------------
        float prog = static_cast<float>(frame + 1) / totalFrames;
        progress_.store(prog);

        double elapsed = (juce::Time::getMillisecondCounterHiRes() - startTime) * 0.001;
        double eta = (elapsed / (frame + 1)) * (totalFrames - frame - 1);

        // Notify every 5 frames (avoid excessive listener calls)
        if ((frame % 5 == 0) || frame == totalFrames - 1)
        {
            if (!isPngSequence)
                ffmpeg_.drainStderr();   // prevent stderr pipe deadlock

            // Store a down-scaled preview frame for the progress window
            {
                const int prevW = std::min(frameImage.getWidth(), 320);
                const float aspect = static_cast<float>(frameImage.getHeight())
                                   / static_cast<float>(frameImage.getWidth());
                const int prevH = static_cast<int>(prevW * aspect);
                auto preview = frameImage.rescaled(prevW, prevH,
                    juce::Graphics::lowResamplingQuality);
                const juce::SpinLock::ScopedLockType sl(previewLock_);
                previewImage_ = std::move(preview);
            }

            notifyProgress(prog, frame + 1, totalFrames, eta);
        }
    }

    //-- 8. Cleanup offline plugin instances  ---------------------------------
    cleanupOfflinePlugins();

    //-- 9. Finish  -----------------------------------------------------------
    if (!isPngSequence)
    {
        int exitCode = ffmpeg_.finish();
        if (exitCode != 0)
        {
            notifyFinished(false,
                "FFmpeg exited with code " + juce::String(exitCode)
                + ". " + ffmpeg_.getErrorOutput());
            return;
        }
    }

    notifyFinished(true, "Export complete! " + juce::String(totalFrames) + " frames rendered.");
}

//==============================================================================
void OfflineRenderer::createOffscreenItems()
{
    offscreenItems_.clear();
    offlinePlugins_.clear();

    for (int i = 0; i < canvasModel_.getNumItems(); ++i)
    {
        const auto* src = canvasModel_.getItem(i);
        if (!src->visible) continue;

        // Skip WaveformView for offline export (it's a transport-bound component)
        if (src->meterType == MeterType::WaveformView) continue;

        CanvasItem copy;
        copy.meterType     = src->meterType;
        copy.x             = src->x;
        copy.y             = src->y;
        copy.width         = src->width;
        copy.height        = src->height;
        copy.rotation      = src->rotation;
        copy.visible       = true;
        copy.name          = src->name;
        copy.opacity       = src->opacity;
        copy.mediaFilePath = src->mediaFilePath;
        copy.vuChannel     = src->vuChannel;

        // Per-item background
        copy.itemBackground = src->itemBackground;

        // Meter colour overrides
        copy.meterBgColour = src->meterBgColour;
        copy.meterFgColour = src->meterFgColour;
        copy.blendMode     = src->blendMode;

        // Custom plugin IDs
        copy.customPluginId    = src->customPluginId;
        copy.customInstanceId  = src->customInstanceId;

        // Shape properties
        copy.fillColour1       = src->fillColour1;
        copy.fillColour2       = src->fillColour2;
        copy.gradientDirection = src->gradientDirection;
        copy.cornerRadius      = src->cornerRadius;
        copy.strokeColour      = src->strokeColour;
        copy.strokeWidth       = src->strokeWidth;

        // Text properties
        copy.textContent   = src->textContent;
        copy.fontFamily    = src->fontFamily;
        copy.fontSize      = src->fontSize;
        copy.fontBold      = src->fontBold;
        copy.fontItalic    = src->fontItalic;
        copy.textColour    = src->textColour;
        copy.textAlignment = src->textAlignment;

        // For CustomPlugin items, create an offline bridge instance
        // instead of relying on the component's GL pipeline.
        if (src->meterType == MeterType::CustomPlugin
            && src->customPluginId.isNotEmpty())
        {
            auto& bridge = PythonPluginBridge::getInstance();
            if (bridge.isRunning())
            {
                auto offlineId = "offline_" + juce::Uuid().toString();
                bridge.createInstance(src->customPluginId, offlineId);

                // Transfer property values from the live instance
                if (src->component)
                {
                    auto* liveCpc = dynamic_cast<CustomPluginComponent*>(src->component.get());
                    if (liveCpc)
                    {
                        for (auto& prop : liveCpc->getPluginProperties())
                            bridge.setProperty(offlineId, prop.key, prop.defaultVal);
                    }
                }

                OfflinePlugin info;
                info.itemIndex         = static_cast<int>(offscreenItems_.size());
                info.offlineInstanceId = offlineId;
                info.manifestId        = src->customPluginId;
                offlinePlugins_.push_back(std::move(info));

                copy.customInstanceId = offlineId;
            }

            // Create the component and initialise its GL pipeline
            copy.component = offlineFactory_.createMeter(src->meterType);

            // Initialise GL resources so shaders can render offline
            if (auto* cpc = dynamic_cast<CustomPluginComponent*>(copy.component.get()))
            {
                // Apply meter colours so the GL clear uses the right background
                cpc->setMeterBgColour(copy.meterBgColour);
                cpc->setMeterFgColour(copy.meterFgColour);
                cpc->initForOfflineRendering();
            }
        }
        else
        {
            copy.component = offlineFactory_.createMeter(src->meterType);
        }

        // Transfer component-specific settings from the live component
        if (copy.component && src->component)
            transferComponentSettings(src, &copy);

        // Load media files for image/video layers
        if (copy.mediaFilePath.isNotEmpty())
        {
            juce::File mediaFile(copy.mediaFilePath);
            if (mediaFile.existsAsFile())
            {
                if (copy.meterType == MeterType::ImageLayer)
                {
                    if (auto* imgComp = dynamic_cast<ImageLayerComponent*>(copy.component.get()))
                        imgComp->loadFromFile(mediaFile);
                }
                else if (copy.meterType == MeterType::VideoLayer)
                {
                    if (auto* vidComp = dynamic_cast<VideoLayerComponent*>(copy.component.get()))
                        vidComp->loadFromFileBlocking(mediaFile);
                }
            }
        }

        offscreenItems_.push_back(std::move(copy));
    }
}

//==============================================================================
void OfflineRenderer::transferComponentSettings(const CanvasItem* src, CanvasItem* dst)
{
    auto* srcComp = src->component.get();
    auto* dstComp = dst->component.get();
    if (!srcComp || !dstComp) return;

    switch (src->meterType)
    {
        case MeterType::MultiBandAnalyzer:
        {
            auto* s = dynamic_cast<MultiBandAnalyzer*>(srcComp);
            auto* d = dynamic_cast<MultiBandAnalyzer*>(dstComp);
            if (s && d)
            {
                d->setDecayRate(s->getDecayRate());
                d->setDynamicRange(s->getMinDb(), s->getMaxDb());
                d->setNumBands(s->getNumBands());
                d->setScaleMode(s->getScaleMode());
            }
            break;
        }
        case MeterType::Spectrogram:
        {
            auto* s = dynamic_cast<Spectrogram*>(srcComp);
            auto* d = dynamic_cast<Spectrogram*>(dstComp);
            if (s && d)
            {
                d->setColourMap(s->getColourMap());
                d->setScrollDirection(s->getScrollDirection());
                d->setDynamicRange(s->getMinDb(), s->getMaxDb());
            }
            break;
        }
        case MeterType::Goniometer:
        {
            auto* s = dynamic_cast<Goniometer*>(srcComp);
            auto* d = dynamic_cast<Goniometer*>(dstComp);
            if (s && d)
            {
                d->setDotSize(s->getDotSize());
                d->setTrailOpacity(s->getTrailOpacity());
                d->setShowGrid(s->getShowGrid());
            }
            break;
        }
        case MeterType::LissajousScope:
        {
            auto* s = dynamic_cast<LissajousScope*>(srcComp);
            auto* d = dynamic_cast<LissajousScope*>(dstComp);
            if (s && d)
            {
                d->setMode(s->getMode());
                d->setTrailLength(s->getTrailLength());
                d->setShowGrid(s->getShowGrid());
            }
            break;
        }
        case MeterType::LoudnessMeter:
        {
            auto* s = dynamic_cast<LoudnessMeter*>(srcComp);
            auto* d = dynamic_cast<LoudnessMeter*>(dstComp);
            if (s && d)
            {
                d->setTargetLUFS(s->getTargetLUFS());
                d->setShowHistory(s->getShowHistory());
            }
            break;
        }
        case MeterType::LevelHistogram:
        {
            auto* s = dynamic_cast<LevelHistogram*>(srcComp);
            auto* d = dynamic_cast<LevelHistogram*>(dstComp);
            if (s && d)
            {
                d->setDisplayRange(s->getMinDb(), s->getMaxDb());
                d->setBinResolution(s->getBinResolution());
                d->setCumulative(s->getCumulative());
                d->setShowStereo(s->getShowStereo());
            }
            break;
        }
        case MeterType::CorrelationMeter:
        {
            auto* s = dynamic_cast<CorrelationMeter*>(srcComp);
            auto* d = dynamic_cast<CorrelationMeter*>(dstComp);
            if (s && d)
            {
                d->setIntegrationTimeMs(s->getIntegrationTimeMs());
                d->setShowNumeric(s->getShowNumeric());
            }
            break;
        }
        case MeterType::PeakMeter:
        {
            auto* s = dynamic_cast<PeakMeter*>(srcComp);
            auto* d = dynamic_cast<PeakMeter*>(dstComp);
            if (s && d)
            {
                d->setPeakMode(s->getPeakMode());
                d->setPeakHoldTimeMs(s->getPeakHoldTimeMs());
                d->setDecayRateDbPerSec(s->getDecayRateDbPerSec());
                d->setShowClipWarning(s->getShowClipWarning());
            }
            break;
        }
        case MeterType::SkinnedVUMeter:
        {
            auto* s = dynamic_cast<SkinnedVUMeter*>(srcComp);
            auto* d = dynamic_cast<SkinnedVUMeter*>(dstComp);
            if (s && d)
            {
                d->setBallistic(s->getBallistic());
                d->setDecayTimeMs(s->getDecayTimeMs());
                d->setChannelLabel(dst->vuChannel == 1 ? "R" : "L");
            }
            break;
        }
        case MeterType::SkinnedSpectrum:
        {
            auto* s = dynamic_cast<SkinnedSpectrumAnalyzer*>(srcComp);
            auto* d = dynamic_cast<SkinnedSpectrumAnalyzer*>(dstComp);
            if (s && d)
            {
                d->setDecayRate(s->getDecayRate());
                d->setNumBands(s->getNumBands());
            }
            break;
        }
        case MeterType::SkinnedOscilloscope:
        {
            auto* s = dynamic_cast<SkinnedOscilloscope*>(srcComp);
            auto* d = dynamic_cast<SkinnedOscilloscope*>(dstComp);
            if (s && d)
            {
                d->setLineThickness(s->getLineThickness());
                d->setDrawStyle(s->getDrawStyle());
            }
            break;
        }
        case MeterType::ShapeRectangle:
        case MeterType::ShapeEllipse:
        case MeterType::ShapeTriangle:
        case MeterType::ShapeLine:
        case MeterType::ShapeStar:
        {
            auto* d = dynamic_cast<ShapeComponent*>(dstComp);
            if (d)
            {
                d->setFillColour1(dst->fillColour1);
                d->setFillColour2(dst->fillColour2);
                d->setGradientDirection(dst->gradientDirection);
                d->setCornerRadius(dst->cornerRadius);
                d->setStrokeColour(dst->strokeColour);
                d->setStrokeWidth(dst->strokeWidth);
                d->setItemBackground(dst->itemBackground);
                d->setFrostedGlass(dst->frostedGlass);
                d->setBlurRadius(dst->blurRadius);
                d->setFrostTint(dst->frostTint);
                d->setFrostOpacity(dst->frostOpacity);
            }
            break;
        }
        case MeterType::TextLabel:
        {
            auto* d = dynamic_cast<TextLabelComponent*>(dstComp);
            if (d)
            {
                d->setTextContent(dst->textContent);
                d->setFontFamily(dst->fontFamily);
                d->setFontSize(dst->fontSize);
                d->setBold(dst->fontBold);
                d->setItalic(dst->fontItalic);
                d->setTextColour(dst->textColour);
                d->setTextAlignment(dst->textAlignment);
                d->setFillColour1(dst->fillColour1);
                d->setFillColour2(dst->fillColour2);
                d->setGradientDirection(dst->gradientDirection);
                d->setCornerRadius(dst->cornerRadius);
                d->setStrokeColour(dst->strokeColour);
                d->setStrokeWidth(dst->strokeWidth);
                d->setItemBackground(dst->itemBackground);
            }
            break;
        }
        default:
            break;
    }
}

//==============================================================================
void OfflineRenderer::processAudioBlock(juce::AudioBuffer<float>& buffer,
                                         int numSamples, double /*sampleRate*/)
{
    const float* left  = buffer.getReadPointer(0);
    const float* right = buffer.getNumChannels() >= 2
                             ? buffer.getReadPointer(1)
                             : left;

    // Mix to mono for FFT and store waveform for custom plugins
    offlineWaveformBuf_.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float mono = (left[i] + right[i]) * 0.5f;
        offlineFft_.pushSamples(&mono, 1);
        offlineWaveformBuf_[i] = mono;
    }

    // Level analyzer
    offlineLa_.processSamples(left, right, numSamples);

    // Loudness analyzer
    offlineLoud_.processSamples(left, right, numSamples);

    // Stereo field analyzer
    offlineStereo_.processSamples(left, right, numSamples);
}

//==============================================================================
void OfflineRenderer::feedOffscreenMeters()
{
    for (auto& item : offscreenItems_)
    {
        // Skip CustomPlugin items — they are fed via feedOfflinePlugins()
        if (item.meterType == MeterType::CustomPlugin)
            continue;

        // SkinnedOscilloscope: feed offline waveform directly
        if (item.meterType == MeterType::SkinnedOscilloscope && item.component)
        {
            if (!offlineWaveformBuf_.empty())
                static_cast<SkinnedOscilloscope*>(item.component.get())
                    ->pushSamples(offlineWaveformBuf_.data(),
                                  static_cast<int>(offlineWaveformBuf_.size()));

            // Still apply MeterBase colours via factory path below
            // but skip the audioEngine-dependent feedMeter
            if (auto* mb = dynamic_cast<MeterBase*>(item.component.get()))
            {
                mb->setMeterBgColour(item.meterBgColour);
                mb->setMeterFgColour(item.meterFgColour);
                mb->setBlendMode(item.blendMode);
                mb->setMeterFontSize(item.fontSize);
                mb->setMeterFontFamily(item.fontFamily);
            }
            continue;
        }

        // WinampSkin: feed offline transport state + spectrum
        if (item.meterType == MeterType::WinampSkin && item.component)
        {
            auto* r = static_cast<WinampSkinRenderer*>(item.component.get());
            if (r->hasSkin())
            {
                r->setPlayState(WinampSkinRenderer::PlayState::Playing);
                double pos = static_cast<double>(currentFrame_) / fps_;
                r->setTime(static_cast<int>(pos) / 60, static_cast<int>(pos) % 60);
                r->setTitleText(fileName_);
            }
            continue;
        }

        // SkinnedPlayer: feed offline transport state + spectrum
        if (item.meterType == MeterType::SkinnedPlayer && item.component)
        {
            auto* p = static_cast<SkinnedPlayerPanel*>(item.component.get());
            if (p->hasSkin())
            {
                p->setPlayState(SkinnedPlayerPanel::PlayState::Playing);
                double pos = static_cast<double>(currentFrame_) / fps_;
                if (fileDuration_ > 0)
                    p->setPosition(pos / fileDuration_);
                p->setTime(static_cast<int>(pos) / 60, static_cast<int>(pos) % 60);
                p->setTitleText(fileName_);

                // Feed offline spectrum
                const float* specData = offlineFft_.getSpectrumData();
                int specSize = offlineFft_.getSpectrumSize();
                if (specSize > 0 && specData)
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
                            if (idx < specSize) sum += specData[idx];
                        }
                        float avg = sum / static_cast<float>(binsPer);
                        bands20[b] = (avg > 1e-10f) ? 20.0f * std::log10(avg) : -60.0f;
                    }
                    p->setSpectrumData(bands20, 20);
                }
            }
            continue;
        }

        offlineFactory_.feedMeter(item);
    }
}

//==============================================================================
void OfflineRenderer::feedOfflinePlugins()
{
    auto& bridge = PythonPluginBridge::getInstance();
    if (!bridge.isRunning()) return;

    // Capture spectrum for GL audio texture upload
    const float* specData = offlineFft_.getSpectrumData();
    int specSize = offlineFft_.getSpectrumSize();
    if (specData && specSize > 0)
        offlineSpectrumBuf_.assign(specData, specData + specSize);
    else
        offlineSpectrumBuf_.clear();

    for (auto& plugin : offlinePlugins_)
    {
        if (plugin.offlineInstanceId.isEmpty()) continue;
        if (plugin.itemIndex < 0 || plugin.itemIndex >= (int)offscreenItems_.size()) continue;

        auto& item = offscreenItems_[plugin.itemIndex];
        int w = std::max(1, static_cast<int>(item.width  * renderScale_));
        int h = std::max(1, static_cast<int>(item.height * renderScale_));

        auto audioJson = buildOfflineAudioJson(item);

        try
        {
            plugin.lastCommands = bridge.renderInstance(
                plugin.offlineInstanceId, w, h, audioJson,
                true /* forceJsonAudio — bypass SHM for offline export */);
        }
        catch (...)
        {
            plugin.lastCommands.clear();
        }
    }
}

//==============================================================================
juce::String OfflineRenderer::buildOfflineAudioJson(const CanvasItem& item)
{
    juce::DynamicObject::Ptr audioObj = new juce::DynamicObject();

    // Channels
    juce::Array<juce::var> channelsArr;
    {
        juce::DynamicObject::Ptr leftCh = new juce::DynamicObject();
        leftCh->setProperty("rms",         offlineLa_.getRMSLeft());
        leftCh->setProperty("peak",        offlineLa_.getPeakLeft());
        leftCh->setProperty("true_peak",   offlineLa_.getPeakLeft());
        leftCh->setProperty("rms_linear",  offlineLa_.getRMSLeft());
        leftCh->setProperty("peak_linear", offlineLa_.getPeakLeft());
        channelsArr.add(juce::var(leftCh.get()));

        juce::DynamicObject::Ptr rightCh = new juce::DynamicObject();
        rightCh->setProperty("rms",         offlineLa_.getRMSRight());
        rightCh->setProperty("peak",        offlineLa_.getPeakRight());
        rightCh->setProperty("true_peak",   offlineLa_.getPeakRight());
        rightCh->setProperty("rms_linear",  offlineLa_.getRMSRight());
        rightCh->setProperty("peak_linear", offlineLa_.getPeakRight());
        channelsArr.add(juce::var(rightCh.get()));
    }
    audioObj->setProperty("channels", channelsArr);
    audioObj->setProperty("num_channels", 2);

    // Loudness
    audioObj->setProperty("lufs_momentary",  offlineLoud_.getMomentaryLUFS());
    audioObj->setProperty("lufs_short_term", offlineLoud_.getShortTermLUFS());
    audioObj->setProperty("lufs_integrated", offlineLoud_.getIntegratedLUFS());
    audioObj->setProperty("loudness_range",  offlineLoud_.getLRA());

    // Stereo
    audioObj->setProperty("correlation", offlineStereo_.getCorrelation());

    // Transport / metadata (so plugins see is_playing=true and correct position)
    audioObj->setProperty("sample_rate",       sampleRate_);
    audioObj->setProperty("is_playing",        true);
    audioObj->setProperty("position_seconds",  static_cast<double>(currentFrame_) / fps_);
    audioObj->setProperty("duration_seconds",  fileDuration_);

    // Spectrum
    const float* specData = offlineFft_.getSpectrumData();
    int specSize = offlineFft_.getSpectrumSize();
    if (specSize > 0 && specData)
    {
        juce::Array<juce::var> specArr;
        juce::Array<juce::var> specLinArr;
        int numBins = juce::jmin(specSize, 512);
        for (int b = 0; b < numBins; ++b)
        {
            specArr.add(specData[b]);
            specLinArr.add(juce::jlimit(0.0f, 1.0f, specData[b]));
        }
        audioObj->setProperty("spectrum", specArr);
        audioObj->setProperty("spectrum_linear", specLinArr);
        audioObj->setProperty("fft_size", specSize * 2);
    }

    // Waveform (from latest offline processed block)
    if (!offlineWaveformBuf_.empty())
    {
        juce::Array<juce::var> waveArr;
        for (auto s : offlineWaveformBuf_)
            waveArr.add(s);
        audioObj->setProperty("waveform", waveArr);
    }

    // Meter colours
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

    return juce::JSON::toString(juce::var(audioObj.get()), true);
}

//==============================================================================
void OfflineRenderer::cleanupOfflinePlugins()
{
    // Release GL resources for offline components
    for (auto& item : offscreenItems_)
    {
        if (item.meterType == MeterType::CustomPlugin)
        {
            if (auto* cpc = dynamic_cast<CustomPluginComponent*>(item.component.get()))
                cpc->releaseOfflineRendering();
        }
    }

    // Destroy bridge instances
    auto& bridge = PythonPluginBridge::getInstance();
    if (bridge.isRunning())
    {
        for (auto& plugin : offlinePlugins_)
        {
            if (plugin.offlineInstanceId.isNotEmpty())
            {
                try { bridge.destroyInstance(plugin.offlineInstanceId); }
                catch (...) {}
            }
        }
    }
    offlinePlugins_.clear();
}

//==============================================================================
juce::Image OfflineRenderer::renderFrame(int videoW, int videoH)
{
    juce::Image image(juce::Image::RGB, videoW, videoH, true);
    juce::Graphics g(image);

    // Paint canvas background
    canvasModel_.background.paint(g, juce::Rectangle<float>(0, 0,
        static_cast<float>(videoW), static_cast<float>(videoH)));

    // Compute content bounding box
    juce::Rectangle<float> content;
    if (!offscreenItems_.empty())
    {
        content = offscreenItems_[0].getBounds();
        for (size_t i = 1; i < offscreenItems_.size(); ++i)
            content = content.getUnion(offscreenItems_[i].getBounds());
    }
    else
    {
        return image;  // nothing to render
    }

    // Add a small margin
    content = content.expanded(10.0f);

    // Scale factor: fit content into video frame, preserving aspect ratio
    float scaleX = static_cast<float>(videoW) / content.getWidth();
    float scaleY = static_cast<float>(videoH) / content.getHeight();
    float scale  = std::min(scaleX, scaleY);

    float offsetX = (videoW  - content.getWidth()  * scale) * 0.5f;
    float offsetY = (videoH  - content.getHeight() * scale) * 0.5f;

    // Render each offscreen meter
    for (int idx = 0; idx < (int)offscreenItems_.size(); ++idx)
    {
        auto& item = offscreenItems_[idx];
        if (!item.component) continue;

        float ix = (item.x - content.getX()) * scale + offsetX;
        float iy = (item.y - content.getY()) * scale + offsetY;
        float iw = item.width  * scale;
        float ih = item.height * scale;

        int pw = std::max(1, static_cast<int>(iw));
        int ph = std::max(1, static_cast<int>(ih));

        // Set component size so its paint() uses the correct bounds
        item.component->setSize(pw, ph);

        // Render the component to a sub-image
        juce::Image meterImg(juce::Image::ARGB, pw, ph, true);
        {
            juce::Graphics mg(meterImg);

            // Draw per-item background if set
            if (!item.itemBackground.isTransparent())
            {
                mg.setColour(item.itemBackground);
                mg.fillRect(0, 0, pw, ph);
            }

            // CustomPlugin: render via the full GL pipeline (shaders + 2D)
            if (item.meterType == MeterType::CustomPlugin)
            {
                for (auto& plugin : offlinePlugins_)
                {
                    if (plugin.itemIndex == idx && !plugin.lastCommands.empty())
                    {
                        auto* cpc = dynamic_cast<CustomPluginComponent*>(item.component.get());
                        if (cpc)
                        {
                            auto offlineImg = cpc->renderOfflineFrame(
                                plugin.lastCommands,
                                offlineSpectrumBuf_,
                                offlineWaveformBuf_,
                                pw, ph);
                            if (offlineImg.isValid())
                                mg.drawImage(offlineImg, juce::Rectangle<float>(0, 0, (float)pw, (float)ph));
                        }
                        break;
                    }
                }
            }
            else
            {
                item.component->paint(mg);

                // Also paint children if any
                for (int c = 0; c < item.component->getNumChildComponents(); ++c)
                {
                    auto* child = item.component->getChildComponent(c);
                    if (child && child->isVisible())
                    {
                        mg.saveState();
                        mg.setOrigin(child->getPosition());
                        child->paint(mg);
                        mg.restoreState();
                    }
                }
            }
        }

        // Composite onto main image (with rotation and opacity)
        float alpha = juce::jlimit(0.0f, 1.0f, item.opacity);

        if (item.rotation != 0)
        {
            float cx = ix + iw * 0.5f;
            float cy = iy + ih * 0.5f;
            auto t = juce::AffineTransform::translation(-pw * 0.5f, -ph * 0.5f)
                .rotated(item.rotation * juce::MathConstants<float>::pi / 180.0f)
                .translated(cx, cy);
            g.setOpacity(alpha);
            g.drawImageTransformed(meterImg, t);
            g.setOpacity(1.0f);
        }
        else
        {
            g.setOpacity(alpha);
            g.drawImageAt(meterImg, static_cast<int>(ix), static_cast<int>(iy));
            g.setOpacity(1.0f);
        }
    }

    return image;
}

//==============================================================================
void OfflineRenderer::imageToRGB24(const juce::Image& img, std::vector<uint8_t>& outBuffer)
{
    const int w = img.getWidth();
    const int h = img.getHeight();
    outBuffer.resize(static_cast<size_t>(w) * h * 3);

    juce::Image::BitmapData bmp(img, juce::Image::BitmapData::readOnly);
    const int pixStride = bmp.pixelStride;

    for (int y = 0; y < h; ++y)
    {
        const uint8_t* src = bmp.getLinePointer(y);
        uint8_t* dest = outBuffer.data() + static_cast<size_t>(y) * w * 3;

        if (pixStride == 3)
        {
            // RGB format: stored as BGR
            for (int x = 0; x < w; ++x)
            {
                dest[0] = src[2];   // R
                dest[1] = src[1];   // G
                dest[2] = src[0];   // B
                src  += 3;
                dest += 3;
            }
        }
        else // ARGB format: stored as BGRA (pixelStride == 4)
        {
            for (int x = 0; x < w; ++x)
            {
                dest[0] = src[2];   // R
                dest[1] = src[1];   // G
                dest[2] = src[0];   // B
                src  += pixStride;
                dest += 3;
            }
        }
    }
}

//==============================================================================
void OfflineRenderer::notifyProgress(float prog, int curFrame, int totalFrames, double etaSec)
{
    auto aliveFlag = alive_;
    auto* self = this;
    juce::MessageManager::callAsync([aliveFlag, self, prog, curFrame, totalFrames, etaSec]()
    {
        if (aliveFlag->load())
            self->listeners_.call(&Listener::renderingProgress, prog, curFrame, totalFrames, etaSec);
    });
}

void OfflineRenderer::notifyFinished(bool success, const juce::String& msg)
{
    auto aliveFlag = alive_;
    auto* self = this;
    juce::MessageManager::callAsync([aliveFlag, self, success, msg]()
    {
        if (aliveFlag->load())
            self->listeners_.call(&Listener::renderingFinished, success, msg);
    });
}
