#include "FFmpegProcess.h"

//==============================================================================
// FFmpeg locator —————————————————————————————————————————————————————————————
juce::File FFmpegProcess::locateFFmpeg()
{
    // 1. Check next to the application executable
    auto appDir = juce::File::getSpecialLocation(
        juce::File::currentApplicationFile).getParentDirectory();

    auto bundled = appDir.getChildFile("ffmpeg.exe");
    if (bundled.existsAsFile())
        return bundled;

    // Also check a "ffmpeg" sub-folder
    bundled = appDir.getChildFile("ffmpeg").getChildFile("ffmpeg.exe");
    if (bundled.existsAsFile())
        return bundled;

    // 2. Search system PATH
    auto pathEnv = juce::SystemStats::getEnvironmentVariable("PATH", "");
    auto dirs = juce::StringArray::fromTokens(pathEnv, ";", "\"");
    for (const auto& d : dirs)
    {
        auto candidate = juce::File(d).getChildFile("ffmpeg.exe");
        if (candidate.existsAsFile())
            return candidate;
    }

    return {};  // not found
}

//==============================================================================
FFmpegProcess::FFmpegProcess() = default;

FFmpegProcess::~FFmpegProcess()
{
    finish();
}

//==============================================================================
#ifdef _WIN32

bool FFmpegProcess::start(const Export::Settings& settings)
{
    if (started_) finish();
    accumulatedStderr_.clear();

    auto ffmpegPath = locateFFmpeg();
    if (!ffmpegPath.existsAsFile())
        return false;

    auto args = settings.buildFFmpegArgs(ffmpegPath);
    juce::String cmdLine = args.joinIntoString(" ");

    // Security attributes — inheritable handles
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // stdin pipe (we write, child reads)
    HANDLE stdinRead = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&stdinRead, &stdinWrite_, &sa, 0))
        return false;
    // Prevent our write end from being inherited
    SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0);

    // stderr pipe (child writes, we read) — use 1 MB buffer to avoid deadlocks
    HANDLE stderrWrite = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&stderrRead_, &stderrWrite, &sa, 1024 * 1024))
    {
        CloseHandle(stdinRead);
        CloseHandle(stdinWrite_);
        stdinWrite_ = INVALID_HANDLE_VALUE;
        return false;
    }
    SetHandleInformation(stderrRead_, HANDLE_FLAG_INHERIT, 0);

    // Startup info — redirect stdin/stdout/stderr
    STARTUPINFOW si{};
    si.cb         = sizeof(si);
    si.hStdInput  = stdinRead;
    // Redirect stdout to NUL to prevent inheriting parent console
    HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE,
                              &sa, OPEN_EXISTING, 0, nullptr);
    si.hStdOutput = hNul;
    si.hStdError  = stderrWrite;
    si.dwFlags   |= STARTF_USESTDHANDLES;

    // Create the process
    auto wideCmdLine = cmdLine.toWideCharPointer();
    std::vector<wchar_t> cmdBuf(wcslen(wideCmdLine) + 1);
    wcscpy_s(cmdBuf.data(), cmdBuf.size(), wideCmdLine);

    BOOL ok = CreateProcessW(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr,
        TRUE,                    // inherit handles
        CREATE_NO_WINDOW,        // hide console
        nullptr, nullptr,
        &si, &pi_);

    // Close the child's ends of the pipes (we don't need them)
    CloseHandle(stdinRead);
    CloseHandle(stderrWrite);
    if (hNul != INVALID_HANDLE_VALUE)
        CloseHandle(hNul);

    if (!ok)
    {
        CloseHandle(stdinWrite_);  stdinWrite_ = INVALID_HANDLE_VALUE;
        CloseHandle(stderrRead_);  stderrRead_ = INVALID_HANDLE_VALUE;
        return false;
    }

    started_ = true;
    return true;
}

bool FFmpegProcess::writeFrame(const void* rgb24Data, size_t numBytes)
{
    if (!started_ || stdinWrite_ == INVALID_HANDLE_VALUE)
        return false;

    const auto* ptr = static_cast<const char*>(rgb24Data);
    size_t remaining = numBytes;

    while (remaining > 0)
    {
        DWORD toWrite = static_cast<DWORD>(std::min<size_t>(remaining, 1u << 24));
        DWORD written = 0;
        if (!WriteFile(stdinWrite_, ptr, toWrite, &written, nullptr))
            return false;
        ptr       += written;
        remaining -= written;
    }
    return true;
}

int FFmpegProcess::finish()
{
    if (!started_)
        return -1;

    // Close stdin pipe — signals EOF to FFmpeg
    if (stdinWrite_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(stdinWrite_);
        stdinWrite_ = INVALID_HANDLE_VALUE;
    }

    // Wait for the process to finish (up to 5 minutes for large encodes)
    WaitForSingleObject(pi_.hProcess, 300000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi_.hProcess, &exitCode);

    // Drain remaining stderr before closing the pipe
    drainStderr();

    if (stderrRead_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(stderrRead_);
        stderrRead_ = INVALID_HANDLE_VALUE;
    }
    CloseHandle(pi_.hProcess);
    CloseHandle(pi_.hThread);
    pi_ = {};
    started_ = false;

    return static_cast<int>(exitCode);
}

bool FFmpegProcess::isRunning() const
{
    if (!started_) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi_.hProcess, &exitCode))
        return exitCode == STILL_ACTIVE;
    return false;
}

juce::String FFmpegProcess::getErrorOutput()
{
    drainStderr();
    return accumulatedStderr_;
}

void FFmpegProcess::drainStderr()
{
    if (stderrRead_ == INVALID_HANDLE_VALUE)
        return;

    char buf[4096];
    DWORD avail = 0;

    while (PeekNamedPipe(stderrRead_, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
    {
        DWORD bytesRead = 0;
        DWORD toRead = std::min<DWORD>(avail, sizeof(buf) - 1);
        if (ReadFile(stderrRead_, buf, toRead, &bytesRead, nullptr) && bytesRead > 0)
        {
            buf[bytesRead] = '\0';
            accumulatedStderr_ += juce::String::fromUTF8(buf, static_cast<int>(bytesRead));
        }
        else break;
    }
}

#else
// Stub implementations for non-Windows (future: Unix pipe support)
bool FFmpegProcess::start(const Export::Settings&) { return false; }
bool FFmpegProcess::writeFrame(const void*, size_t) { return false; }
int  FFmpegProcess::finish() { return -1; }
bool FFmpegProcess::isRunning() const { return false; }
juce::String FFmpegProcess::getErrorOutput() { return "Not supported on this platform."; }
void FFmpegProcess::drainStderr() {}
#endif
