#include "ThreeDOfflineRenderer.h"

//==============================================================================
ThreeDOfflineRenderer::ThreeDOfflineRenderer (const Export::Settings& settings,
                                              ThreeDWebView&           webView)
    : juce::Thread ("ThreeDOfflineRenderer"),
      settings_    (settings),
      webView_     (webView)
{
    formatManager_.registerBasicFormats();
}

ThreeDOfflineRenderer::~ThreeDOfflineRenderer()
{
    alive_->store (false);  // prevent pending callAsync lambdas from touching members
    signalThreadShouldExit();
    frameEvent_.signal();   // unblock wait() if thread is sleeping
    stopThread (8000);
}

//==============================================================================
void ThreeDOfflineRenderer::addListener    (Listener* l) { listeners_.add    (l); }
void ThreeDOfflineRenderer::removeListener (Listener* l) { listeners_.remove (l); }

//==============================================================================
juce::Image ThreeDOfflineRenderer::getLatestPreview()
{
    juce::ScopedLock sl (previewLock_);
    return latestPreview_;
}

//==============================================================================
void ThreeDOfflineRenderer::receiveFrameData (const juce::String& base64png, int /*frameIndex*/)
{
    // Called on the JUCE message thread.
    juce::Image img;

    if (base64png.isNotEmpty())
    {
        juce::MemoryOutputStream mos;
        if (juce::Base64::convertFromBase64 (mos, base64png))
        {
            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            img = juce::ImageFileFormat::loadFrom (mis);
        }
    }

    {
        juce::ScopedLock sl (previewLock_);
        latestPreview_ = img;
    }

    frameEvent_.signal();   // wake background thread
}

//==============================================================================
void ThreeDOfflineRenderer::run()
{
    // ── Open audio file ──────────────────────────────────────────────────────
    reader_.reset (formatManager_.createReaderFor (settings_.audioFile));
    if (!reader_)
    {
        notifyFinished (false, "Cannot open audio file: " +
                                settings_.audioFile.getFullPathName());
        return;
    }

    const double     sampleRate   = reader_->sampleRate;
    const juce::int64 totalSamples = reader_->lengthInSamples;
    const double     duration     = totalSamples / sampleRate;
    const int        fps          = static_cast<int> (settings_.frameRate);
    const int        totalFrames  = (int) std::ceil (duration * fps);
    const int        videoW       = settings_.getWidth();
    const int        videoH       = settings_.getHeight();

    if (totalFrames <= 0)
    {
        notifyFinished (false, "Audio file is too short.");
        return;
    }

    // ── Start FFmpeg ─────────────────────────────────────────────────────────
    FFmpegProcess ffmpeg;
    if (!ffmpeg.start (settings_))
    {
        notifyFinished (false, "Could not start FFmpeg.\n" + ffmpeg.getErrorOutput());
        return;
    }

    // ── Register the onFrameReady callback on the message thread ─────────────
    // (safe because webView_ outlives this thread — caller must ensure that)
    auto alive = alive_;
    juce::MessageManager::callAsync ([this, alive]()
    {
        if (!alive->load()) return;
        webView_.onFrameReady = [this, alive] (const juce::String& base64, int idx)
        {
            if (!alive->load()) return;
            receiveFrameData (base64, idx);
        };
    });

    // Give the message thread time to register the callback before first frame.
    juce::Thread::sleep (50);

    // ── Per-frame loop ───────────────────────────────────────────────────────
    const double startWall = juce::Time::getMillisecondCounterHiRes();

    for (int fi = 0; fi < totalFrames && !threadShouldExit(); ++fi)
    {
        const double timeSec = fi / (double) fps;

        // Build audio data for this frame
        std::vector<float> wave, spec;
        readAudioFrame (fi, fps, sampleRate, totalSamples, wave, spec);

        // Reset event, then ask the JS side to render this frame
        frameEvent_.reset();

        juce::MessageManager::callAsync ([this, alive, fi, totalFrames, timeSec, wave, spec]() mutable
        {
            if (!alive->load()) return;
            if (!threadShouldExit())
                webView_.exportFrame (fi, totalFrames, timeSec, wave, spec);
        });

        // Wait up to 12 seconds for the PNG to come back
        if (!frameEvent_.wait (12000))
        {
            ffmpeg.finish();
            notifyFinished (false, "Frame timeout at frame " + juce::String (fi));
            return;
        }

        if (threadShouldExit())
            break;

        // Retrieve the decoded frame
        juce::Image frame = getLatestPreview();

        if (!frame.isValid())
        {
            ffmpeg.finish();
            notifyFinished (false, "Frame decode failed at frame " + juce::String (fi));
            return;
        }

        // Scale to video resolution if the WebGL canvas is a different size
        juce::Image scaled = frame;
        if (frame.getWidth() != videoW || frame.getHeight() != videoH)
            scaled = frame.rescaled (videoW, videoH,
                                     juce::Graphics::mediumResamplingQuality);

        // Write RGB24 to FFmpeg
        auto rgb24 = imageToRGB24 (scaled);
        if (!ffmpeg.writeFrame (rgb24.data(), rgb24.size()))
        {
            notifyFinished (false, "FFmpeg write error at frame " + juce::String (fi));
            return;
        }

        ffmpeg.drainStderr();

        // Progress callback
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - startWall) / 1000.0;
        const double eta     = (fi > 0) ? elapsed / fi * (totalFrames - fi - 1) : 0.0;
        notifyProgress ((fi + 1.0f) / (float) totalFrames, fi + 1, totalFrames, eta);
    }

    // ── Wrap up ──────────────────────────────────────────────────────────────
    const int exitCode = ffmpeg.finish();

    // Restore JS live mode
    juce::MessageManager::callAsync ([this, alive]()
    {
        if (!alive->load()) return;
        webView_.onFrameReady = nullptr;
        webView_.exportStop();
    });

    if (threadShouldExit())
        notifyFinished (false, "Export cancelled.");
    else if (exitCode != 0)
        notifyFinished (false, "FFmpeg error (exit " + juce::String (exitCode) + "):\n" +
                                ffmpeg.getErrorOutput());
    else
        notifyFinished (true, "Export complete:\n" + settings_.outputFile.getFullPathName());
}

//==============================================================================
void ThreeDOfflineRenderer::readAudioFrame (int frameIndex, int fps,
                                             double sampleRate,
                                             juce::int64 totalSamples,
                                             std::vector<float>& waveOut,
                                             std::vector<float>& specOut)
{
    constexpr int BLOCK = 2048;

    auto startSample = (juce::int64) (frameIndex / (double) fps * sampleRate);
    startSample = juce::jlimit<juce::int64> (0, totalSamples - BLOCK, startSample);

    juce::AudioBuffer<float> buf (2, BLOCK);
    buf.clear();
    reader_->read (&buf, 0, BLOCK, startSample, true, true);

    // Mono mix
    const float* ch0 = buf.getReadPointer (0);
    const float* ch1 = buf.getReadPointer (1 < buf.getNumChannels() ? 1 : 0);
    std::vector<float> mono (BLOCK);
    for (int i = 0; i < BLOCK; i++)
        mono[i] = (ch0[i] + ch1[i]) * 0.5f;

    // Waveform — downsample to 64
    waveOut.resize (64);
    for (int i = 0; i < 64; i++)
        waveOut[i] = mono[(size_t) i * BLOCK / 64];

    // Spectrum via JUCE FFT (order 11 = 2048 points)
    juce::dsp::FFT fft (11);
    std::vector<float> fftBuf (BLOCK * 2, 0.0f);
    for (int i = 0; i < BLOCK; i++)
    {
        // Hann window
        const float hann = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                                     * i / (BLOCK - 1)));
        fftBuf[(size_t) i] = mono[(size_t) i] * hann;
    }
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());

    specOut.resize (64);
    const int binsPerBand = (BLOCK / 2) / 64;
    for (int i = 0; i < 64; i++)
    {
        float sum = 0.0f;
        for (int b = 0; b < binsPerBand; b++)
            sum += fftBuf[(size_t) (i * binsPerBand + b)];
        specOut[(size_t) i] = juce::jlimit (0.0f, 1.0f,
                                            sum / (float) binsPerBand * 0.015f);
    }
}

//==============================================================================
std::vector<std::uint8_t> ThreeDOfflineRenderer::imageToRGB24 (const juce::Image& img)
{
    const int w = img.getWidth();
    const int h = img.getHeight();
    std::vector<std::uint8_t> rgb ((size_t) (w * h * 3));

    juce::Image::BitmapData bmp (img, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            auto px  = bmp.getPixelColour (x, y);
            auto idx = (size_t) ((y * w + x) * 3);
            rgb[idx + 0] = (std::uint8_t) px.getRed();
            rgb[idx + 1] = (std::uint8_t) px.getGreen();
            rgb[idx + 2] = (std::uint8_t) px.getBlue();
        }
    }
    return rgb;
}

//==============================================================================
void ThreeDOfflineRenderer::notifyProgress (float progress, int currentFrame,
                                            int totalFrames, double eta)
{
    auto alive = alive_;
    juce::MessageManager::callAsync ([this, alive, progress, currentFrame, totalFrames, eta]()
    {
        if (!alive->load()) return;
        listeners_.call ([&] (Listener& l)
        {
            l.threed_renderProgress (progress, currentFrame, totalFrames, eta);
        });
    });
}

void ThreeDOfflineRenderer::notifyFinished (bool success, const juce::String& msg)
{
    auto alive = alive_;
    juce::String msgCopy (msg);
    juce::MessageManager::callAsync ([this, alive, success, msgCopy]()
    {
        if (!alive->load()) return;
        listeners_.call ([&] (Listener& l)
        {
            l.threed_renderFinished (success, msgCopy);
        });
    });
}
