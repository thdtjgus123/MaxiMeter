#pragma once

#include <JuceHeader.h>
#include "../Export/FFmpegProcess.h"
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <fstream>
#include <mutex>

#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

//==============================================================================
// Crash-trace logger — writes to MaxiMeter_crash.log next to the exe
//==============================================================================
namespace VLC_Log {
    inline std::mutex& mut() { static std::mutex m; return m; }
    inline void log(const char* msg)
    {
        std::lock_guard<std::mutex> lk(mut());
        auto p = juce::File::getSpecialLocation(
                     juce::File::currentApplicationFile)
                     .getParentDirectory()
                     .getChildFile("MaxiMeter_crash.log");
        std::ofstream f(p.getFullPathName().toStdString(), std::ios::app);
        f << juce::Time::getCurrentTime().toString(true, true, true, true).toStdString()
          << "  " << msg << "\n";
        f.flush();
    }
}

//==============================================================================
/// Displays an animated GIF or video on the canvas.
class VideoLayerComponent : public juce::Component,
                            public juce::Timer
{
public:
    VideoLayerComponent()  { startTimerHz(30); }

    ~VideoLayerComponent() override
    {
        VLC_Log::log("~VideoLayerComponent BEGIN");
        alive_->store(false);
        stopTimer();
        cancelAndJoin();
        VLC_Log::log("~VideoLayerComponent END");
    }

    //==========================================================================
    //  Loading API
    //==========================================================================

    bool loadFromFile(const juce::File& file)
    {
        VLC_Log::log(("loadFromFile: " + file.getFullPathName()).toRawUTF8());

        VLC_Log::log("loadFromFile: step1 cancelAndJoin");
        cancelAndJoin();
        VLC_Log::log("loadFromFile: step2 resetState");
        resetState(file);
        VLC_Log::log("loadFromFile: step3 repaint");
        isLoading_ = true;
        repaint();

        VLC_Log::log("loadFromFile: step4 getExtension");
        auto ext = file.getFileExtension().toLowerCase();
        VLC_Log::log(("loadFromFile: step5 ext=" + ext).toRawUTF8());

        if (isAnimatedFormat(ext))
        {
            VLC_Log::log("loadFromFile: step6 animated format detected");
            int gen = ++loadGeneration_;
            loadCancelled_.store(false);
            auto aliveRef = alive_;
            VLC_Log::log("loadFromFile: step7 about to create thread");

            try {
                loadThread_ = std::make_unique<std::thread>([this, file, gen, aliveRef]()
                {
                    backgroundLoad(file, gen, aliveRef);
                });
                VLC_Log::log("loadFromFile: step8 thread created OK");
            } catch (const std::exception& e) {
                VLC_Log::log(("loadFromFile: THREAD CREATE EXCEPTION: " + juce::String(e.what())).toRawUTF8());
                isLoading_ = false;
                return false;
            } catch (...) {
                VLC_Log::log("loadFromFile: THREAD CREATE UNKNOWN EXCEPTION");
                isLoading_ = false;
                return false;
            }
            return true;
        }

        // Static image
        VLC_Log::log("loadFromFile: static image, loading inline");
        auto img = juce::ImageFileFormat::loadFrom(file);
        isLoading_ = false;
        if (img.isValid())
        {
            frames_.push_back(img);
            repaint();
            return true;
        }
        return false;
    }

    bool loadFromFileBlocking(const juce::File& file)
    {
        cancelAndJoin();
        resetState(file);
        loadCancelled_.store(false);

        auto ext = file.getFileExtension().toLowerCase();

        if (isAnimatedFormat(ext))
        {
            auto ffPath = FFmpegProcess::locateFFmpeg();
            if (ffPath.existsAsFile())
            {
                std::atomic<bool> noCancel { false };
                extractFrames(ffPath, file, frames_, averageFps_, &noCancel);
                if (!frames_.empty()) return true;
            }
        }

        auto img = juce::ImageFileFormat::loadFrom(file);
        if (img.isValid()) { frames_.push_back(img); return true; }
        return false;
    }

    //==========================================================================
    //  Accessors
    //==========================================================================
    void addFrame(const juce::Image& img)   { if (img.isValid()) frames_.push_back(img); }
    juce::String getFilePath() const        { return filePath_; }
    bool   hasContent() const               { return !frames_.empty(); }
    int    getFrameCount() const            { return static_cast<int>(frames_.size()); }
    float  getAverageFps() const            { return averageFps_; }
    bool   isCurrentlyLoading() const       { return isLoading_; }

    void setFrameRate(int fps)
    {
        averageFps_ = static_cast<float>(fps);
        startTimerHz(fps);
    }

    void setCurrentFrame(int f)
    {
        if (!frames_.empty())
            currentFrame_ = f % static_cast<int>(frames_.size());
    }

    //==========================================================================
    //  Component overrides
    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        if (!frames_.empty())
        {
            auto idx = static_cast<size_t>(
                currentFrame_ % static_cast<int>(frames_.size()));
            g.drawImage(frames_[idx], getLocalBounds().toFloat(),
                        juce::RectanglePlacement::stretchToFit);
        }
        else if (isLoading_)
        {
            g.fillAll(juce::Colour(0xFF1A1A2E));
            g.setColour(juce::Colours::white);
            g.setFont(14.0f);
            g.drawText("Loading...", getLocalBounds(),
                       juce::Justification::centred);
        }
        else
        {
            g.fillAll(juce::Colour(0xFF1A1A2E));
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("Drop video/GIF here", getLocalBounds(),
                       juce::Justification::centred);
        }
    }

    void timerCallback() override
    {
        if (frames_.size() > 1)
        {
            currentFrame_ = (currentFrame_ + 1)
                            % static_cast<int>(frames_.size());
            repaint();
        }
    }

private:
    std::vector<juce::Image> frames_;
    int          currentFrame_ = 0;
    float        averageFps_   = 30.0f;
    juce::String filePath_;
    bool         isLoading_    = false;

    std::unique_ptr<std::thread> loadThread_;
    std::atomic<bool> loadCancelled_  { false };
    std::atomic<int>  loadGeneration_ { 0 };
    std::shared_ptr<std::atomic<bool>> alive_ =
        std::make_shared<std::atomic<bool>>(true);

    static constexpr int kMaxFrames = 1800;

    //--------------------------------------------------------------------------
    static bool isAnimatedFormat(const juce::String& ext)
    {
        return ext == ".gif"  || ext == ".mp4"  || ext == ".avi"
            || ext == ".webm" || ext == ".mov"  || ext == ".mkv"
            || ext == ".wmv"  || ext == ".flv"  || ext == ".m4v";
    }

    void resetState(const juce::File& file)
    {
        filePath_ = file.getFullPathName();
        frames_.clear();
        currentFrame_ = 0;
        averageFps_   = 30.0f;
        isLoading_    = false;
    }

    void cancelAndJoin()
    {
        VLC_Log::log("cancelAndJoin: ENTER");
        VLC_Log::log(("cancelAndJoin: this=" + juce::String::toHexString((juce::int64)(uintptr_t)this)).toRawUTF8());
        VLC_Log::log(("cancelAndJoin: loadThread_ ptr=" + juce::String::toHexString((juce::int64)(uintptr_t)loadThread_.get())).toRawUTF8());

        loadCancelled_.store(true);
        VLC_Log::log("cancelAndJoin: loadCancelled set");

        if (loadThread_)
        {
            VLC_Log::log(("cancelAndJoin: joinable=" + juce::String(loadThread_->joinable() ? "YES" : "NO")).toRawUTF8());
            if (loadThread_->joinable())
            {
                try {
                    VLC_Log::log("cancelAndJoin: joining...");
                    loadThread_->join();
                    VLC_Log::log("cancelAndJoin: joined OK");
                } catch (const std::exception& e) {
                    VLC_Log::log(("cancelAndJoin: join exception: " + juce::String(e.what())).toRawUTF8());
                    try { loadThread_->detach(); } catch (...) {}
                } catch (...) {
                    VLC_Log::log("cancelAndJoin: join unknown exception");
                    try { loadThread_->detach(); } catch (...) {}
                }
            }
        }
        else
        {
            VLC_Log::log("cancelAndJoin: loadThread_ is null");
        }
        loadThread_.reset();
        VLC_Log::log("cancelAndJoin: EXIT");
    }

    //==========================================================================
    //  Background Thread
    //==========================================================================
    void backgroundLoad(const juce::File& file, int generation,
                        std::shared_ptr<std::atomic<bool>> alive)
    {
        VLC_Log::log("backgroundLoad: ENTER");
        try
        {
            backgroundLoadImpl(file, generation, alive);
        }
        catch (const std::exception& e)
        {
            VLC_Log::log(("backgroundLoad EXCEPTION: " + juce::String(e.what())).toRawUTF8());
            deliverFailure(alive, generation);
        }
        catch (...)
        {
            VLC_Log::log("backgroundLoad UNKNOWN EXCEPTION");
            deliverFailure(alive, generation);
        }
        VLC_Log::log("backgroundLoad: EXIT");
    }

    void deliverFailure(std::shared_ptr<std::atomic<bool>>& alive, int generation)
    {
        auto* self = this;
        juce::MessageManager::callAsync([self, alive, generation]()
        {
            if (!alive->load()) return;
            if (self->loadGeneration_.load() != generation) return;
            self->isLoading_ = false;
            self->repaint();
        });
    }

    void backgroundLoadImpl(const juce::File& file, int generation,
                            std::shared_ptr<std::atomic<bool>>& alive)
    {
        std::vector<juce::Image> loaded;
        float fps = 30.0f;

        auto ffPath = FFmpegProcess::locateFFmpeg();
        VLC_Log::log(("backgroundLoadImpl: ffmpeg path = " + ffPath.getFullPathName()).toRawUTF8());
        VLC_Log::log(("backgroundLoadImpl: ffmpeg exists = " + juce::String(ffPath.existsAsFile() ? "YES" : "NO")).toRawUTF8());

        if (ffPath.existsAsFile() && !loadCancelled_.load())
        {
            VLC_Log::log("backgroundLoadImpl: calling extractFrames");
            extractFrames(ffPath, file, loaded, fps, &loadCancelled_);
            VLC_Log::log(("backgroundLoadImpl: extractFrames returned, frames=" + juce::String((int)loaded.size())).toRawUTF8());
        }

        // Fallback: single frame via JUCE
        if (loaded.empty() && !loadCancelled_.load())
        {
            VLC_Log::log("backgroundLoadImpl: fallback to JUCE ImageFileFormat");
            auto img = juce::ImageFileFormat::loadFrom(file);
            if (img.isValid()) loaded.push_back(img);
            VLC_Log::log(("backgroundLoadImpl: fallback result valid=" + juce::String(img.isValid() ? "YES" : "NO")).toRawUTF8());
        }

        if (loadCancelled_.load())
        {
            VLC_Log::log("backgroundLoadImpl: cancelled, returning");
            return;
        }

        VLC_Log::log("backgroundLoadImpl: delivering to message thread");

        auto* self  = this;
        auto shared = std::make_shared<std::vector<juce::Image>>(std::move(loaded));

        juce::MessageManager::callAsync([self, alive, shared, fps, generation]()
        {
            if (!alive->load()) return;
            if (self->loadGeneration_.load() != generation) return;

            self->frames_       = std::move(*shared);
            self->currentFrame_ = 0;
            self->averageFps_   = fps;
            self->isLoading_    = false;

            if (fps > 0)
                self->startTimerHz(juce::jmax(1, static_cast<int>(fps)));

            self->repaint();
        });
    }

    //==========================================================================
    //  Extraction
    //==========================================================================
    void extractFrames(const juce::File& ffmpegPath,
                       const juce::File& inputFile,
                       std::vector<juce::Image>& out,
                       float& outFps,
                       std::atomic<bool>* canceller)
    {
        VLC_Log::log("extractFrames: ENTER");

        juce::Random rnd(juce::Time::getMillisecondCounter());
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("MaxiMeter_vf_" + juce::String(rnd.nextInt(99999)));

        if (!tempDir.createDirectory())
        {
            VLC_Log::log("extractFrames: createDirectory FAILED");
            return;
        }
        VLC_Log::log(("extractFrames: tempDir = " + tempDir.getFullPathName()).toRawUTF8());

        bool isGif = (inputFile.getFileExtension().toLowerCase() == ".gif");

        try
        {
            // 1. Probe
            float fps = 0, dur = 0;
            if (!canceller->load())
            {
                VLC_Log::log("extractFrames: calling probeMedia");
                probeMedia(ffmpegPath, inputFile, fps, dur, canceller);
                VLC_Log::log(("extractFrames: probe result fps=" + juce::String(fps,2) + " dur=" + juce::String(dur,2)).toRawUTF8());
            }
            if (fps <= 0) fps = isGif ? 10.0f : 30.0f;
            outFps = fps;

            if (canceller->load()) { tempDir.deleteRecursively(); return; }

            // 2. Prepare filter — adaptive resolution to keep memory usage reasonable
            //    ≤300 frames: 1280×720,  ≤900: 960×540,  ≤1800: 640×360
            juce::String vf;
            if (!isGif && dur > 0)
            {
                float est = fps * dur;
                if (est > 900)
                    vf = "scale=640:360:force_original_aspect_ratio=decrease";
                else if (est > 300)
                    vf = "scale=960:540:force_original_aspect_ratio=decrease";
                else
                    vf = "scale=1280:720:force_original_aspect_ratio=decrease";

                if (est > kMaxFrames)
                {
                    float tFps = static_cast<float>(kMaxFrames) / dur;
                    if (tFps < 1.0f) tFps = 1.0f;
                    vf += ",fps=" + juce::String(tFps, 2);
                    outFps = tFps;
                }
            }
            else
            {
                vf = "scale=1280:720:force_original_aspect_ratio=decrease";
            }

            // 3. Run FFmpeg
            VLC_Log::log("extractFrames: building FFmpeg command");
            juce::StringArray args;
            args.add(ffmpegPath.getFullPathName());
            args.add("-i");         args.add(inputFile.getFullPathName());
            args.add("-vf");        args.add(vf);
            if (isGif) { args.add("-vsync"); args.add("0"); }
            args.add("-frames:v");  args.add(juce::String(kMaxFrames));
            args.add(tempDir.getChildFile("f_%05d.png").getFullPathName());
            args.add("-y");
            args.add("-loglevel");  args.add("quiet");

            VLC_Log::log("extractFrames: calling runProcess");
            bool ok = runProcess(args, 120000, canceller);
            VLC_Log::log(("extractFrames: runProcess returned ok=" + juce::String(ok ? "YES" : "NO")).toRawUTF8());

            if (canceller->load()) { tempDir.deleteRecursively(); return; }

            // 4. Load Frames
            auto pngs = tempDir.findChildFiles(juce::File::findFiles, false, "f_*.png");
            pngs.sort();
            int limit = juce::jmin(static_cast<int>(pngs.size()), kMaxFrames);
            VLC_Log::log(("extractFrames: found " + juce::String(limit) + " PNGs").toRawUTF8());
            out.reserve(static_cast<size_t>(limit));

            for (int i = 0; i < limit && !canceller->load(); ++i)
            {
                try {
                    auto img = juce::ImageFileFormat::loadFrom(pngs[i]);
                    if (img.isValid()) out.push_back(std::move(img));
                } catch (const std::bad_alloc&) {
                    VLC_Log::log(("extractFrames: OOM at frame " + juce::String(i)).toRawUTF8());
                    break;
                }
            }
            VLC_Log::log(("extractFrames: loaded " + juce::String((int)out.size()) + " frames").toRawUTF8());

            if (isGif && dur > 0.0f && !out.empty())
                outFps = static_cast<float>(out.size()) / dur;
        }
        catch (const std::exception& e)
        {
            VLC_Log::log(("extractFrames EXCEPTION: " + juce::String(e.what())).toRawUTF8());
        }
        catch (...)
        {
            VLC_Log::log("extractFrames UNKNOWN EXCEPTION");
        }

        tempDir.deleteRecursively();
        VLC_Log::log("extractFrames: EXIT");
    }

    //==========================================================================
    //  Probing
    //==========================================================================
    static void probeMedia(const juce::File& ffmpegPath,
                           const juce::File& file,
                           float& outFps, float& outDuration,
                           std::atomic<bool>* canceller)
    {
        VLC_Log::log("probeMedia: ENTER");
#ifdef _WIN32
        auto probePath = ffmpegPath.getParentDirectory().getChildFile("ffprobe.exe");
        if (!probePath.existsAsFile())
        {
            VLC_Log::log("probeMedia: ffprobe.exe NOT FOUND, skipping");
            return;
        }
        VLC_Log::log(("probeMedia: ffprobe path = " + probePath.getFullPathName()).toRawUTF8());

        // FPS
        {
            juce::String cmd = "\"" + probePath.getFullPathName() + "\""
                + " -v error -select_streams v:0 -show_entries stream=r_frame_rate -of csv=p=0 "
                + "\"" + file.getFullPathName() + "\"";
            VLC_Log::log("probeMedia: querying fps...");
            auto txt = captureWin32(cmd, 5000, canceller);
            VLC_Log::log(("probeMedia: fps raw = '" + txt.trim() + "'").toRawUTF8());
            auto parts = juce::StringArray::fromTokens(txt.trim(), "/", "");
            if (parts.size() == 2)
            {
                float n = parts[0].getFloatValue();
                float d = parts[1].getFloatValue();
                if (d > 0) outFps = n / d;
            }
        }
        // Duration
        {
            juce::String cmd = "\"" + probePath.getFullPathName() + "\""
                + " -v error -show_entries format=duration -of csv=p=0 "
                + "\"" + file.getFullPathName() + "\"";
            VLC_Log::log("probeMedia: querying duration...");
            auto txt = captureWin32(cmd, 5000, canceller);
            VLC_Log::log(("probeMedia: duration raw = '" + txt.trim() + "'").toRawUTF8());
            outDuration = txt.trim().getFloatValue();
        }
#else
        (void)ffmpegPath; (void)file; (void)outFps; (void)outDuration; (void)canceller;
#endif
        VLC_Log::log("probeMedia: EXIT");
    }

    //==========================================================================
    //  Safe Win32 Capture
    //==========================================================================
#ifdef _WIN32
    static juce::String captureWin32(const juce::String& cmdLine, int timeoutMs, std::atomic<bool>* canceller)
    {
        VLC_Log::log("captureWin32: ENTER");
        VLC_Log::log(("captureWin32: cmd = " + cmdLine).toRawUTF8());

        // Create a pipe for stdout
        HANDLE hReadOut = NULL, hWriteOut = NULL;
        SECURITY_ATTRIBUTES sa {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        if (!CreatePipe(&hReadOut, &hWriteOut, &sa, 0))
        {
            VLC_Log::log("captureWin32: CreatePipe FAILED");
            return {};
        }
        SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);

        // Create a null handle for stderr (redirect to NUL)
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);

        STARTUPINFOW si {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWriteOut;
        si.hStdError = hNul;
        si.hStdInput = NULL;

        PROCESS_INFORMATION pi {};
        auto wCmd = cmdLine.toWideCharPointer();
        std::vector<wchar_t> buf(wCmd, wCmd + wcslen(wCmd) + 1);

        VLC_Log::log("captureWin32: calling CreateProcessW");
        BOOL created = CreateProcessW(nullptr, buf.data(), nullptr, nullptr,
                                      TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        if (!created)
        {
            DWORD err = GetLastError();
            VLC_Log::log(("captureWin32: CreateProcessW FAILED, err=" + juce::String((int)err)).toRawUTF8());
            CloseHandle(hReadOut);
            CloseHandle(hWriteOut);
            if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
            return {};
        }

        VLC_Log::log("captureWin32: process created, closing write end");
        CloseHandle(hWriteOut);
        hWriteOut = NULL;

        // Read stdout using PeekNamedPipe (non-blocking)
        juce::MemoryBlock output;
        DWORD t0 = GetTickCount();
        bool timedOut = false;

        while (true)
        {
            if (canceller && canceller->load()) break;

            DWORD bytesAvail = 0;
            if (PeekNamedPipe(hReadOut, nullptr, 0, nullptr, &bytesAvail, nullptr) && bytesAvail > 0)
            {
                char tmp[4096];
                DWORD bytesRead = 0;
                if (ReadFile(hReadOut, tmp, sizeof(tmp), &bytesRead, nullptr) && bytesRead > 0)
                {
                    output.append(tmp, bytesRead);
                    continue;
                }
            }

            // Check if process exited
            if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0)
            {
                // Drain remaining
                char tmp[4096];
                DWORD bytesRead = 0;
                while (PeekNamedPipe(hReadOut, nullptr, 0, nullptr, &bytesAvail, nullptr)
                       && bytesAvail > 0
                       && ReadFile(hReadOut, tmp, sizeof(tmp), &bytesRead, nullptr)
                       && bytesRead > 0)
                    output.append(tmp, bytesRead);
                break;
            }

            if ((GetTickCount() - t0) > static_cast<DWORD>(timeoutMs))
            {
                VLC_Log::log("captureWin32: TIMEOUT");
                timedOut = true;
                break;
            }

            Sleep(50);
        }

        if (timedOut || (canceller && canceller->load()))
        {
            VLC_Log::log("captureWin32: terminating process (timeout/cancel)");
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 500);
        }

        CloseHandle(hReadOut);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);

        auto result = juce::String::fromUTF8((const char*)output.getData(), (int)output.getSize());
        VLC_Log::log(("captureWin32: EXIT, outputLen=" + juce::String((int)output.getSize())).toRawUTF8());
        return result;
    }
#endif

    //==========================================================================
    //  Run FFmpeg (no pipe, writes to temp files)
    //==========================================================================
    bool runProcess(const juce::StringArray& args, int timeoutMs, std::atomic<bool>* canceller)
    {
        VLC_Log::log("runProcess: ENTER");
#ifdef _WIN32
        juce::String cmdLine;
        for (const auto& a : args) cmdLine += "\"" + a + "\" ";
        VLC_Log::log(("runProcess: cmd = " + cmdLine).toRawUTF8());

        // Redirect stdout+stderr to NUL so FFmpeg doesn't hang on pipe writes
        SECURITY_ATTRIBUTES sa {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);

        STARTUPINFOW si {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hNul;
        si.hStdError  = hNul;
        si.hStdInput  = NULL;

        PROCESS_INFORMATION pi {};
        auto wStr = cmdLine.toWideCharPointer();
        std::vector<wchar_t> buf(wStr, wStr + wcslen(wStr) + 1);

        VLC_Log::log("runProcess: calling CreateProcessW");
        BOOL created = CreateProcessW(nullptr, buf.data(), nullptr, nullptr,
                                      TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (!created)
        {
            DWORD err = GetLastError();
            VLC_Log::log(("runProcess: CreateProcessW FAILED err=" + juce::String((int)err)).toRawUTF8());
            if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
            return false;
        }

        VLC_Log::log("runProcess: process created, waiting...");
        DWORD t0 = GetTickCount();

        while (WaitForSingleObject(pi.hProcess, 500) == WAIT_TIMEOUT)
        {
            if ((canceller && canceller->load()) || (GetTickCount() - t0 > static_cast<DWORD>(timeoutMs)))
            {
                VLC_Log::log("runProcess: terminating (cancel/timeout)");
                TerminateProcess(pi.hProcess, 1);
                WaitForSingleObject(pi.hProcess, 2000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
                return false;
            }
        }

        DWORD ec = 1;
        GetExitCodeProcess(pi.hProcess, &ec);
        VLC_Log::log(("runProcess: process exited, code=" + juce::String((int)ec)).toRawUTF8());

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);

        VLC_Log::log("runProcess: EXIT");
        return ec == 0;
#else
        (void)args; (void)timeoutMs; (void)canceller;
        return false;
#endif
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoLayerComponent)
};
