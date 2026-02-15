#pragma once

#include <JuceHeader.h>
#include "ExportSettings.h"

#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

//==============================================================================
/// Manages an FFmpeg child process with stdin pipe for raw video frame writing.
/// Uses Win32 CreateProcess + pipes on Windows for stdin support.
class FFmpegProcess
{
public:
    FFmpegProcess();
    ~FFmpegProcess();

    /// Locate ffmpeg.exe — first checks app resource dir, then system PATH.
    static juce::File locateFFmpeg();

    /// Start FFmpeg with the given export settings.
    bool start(const Export::Settings& settings);

    /// Write raw RGB24 frame data to the FFmpeg stdin pipe.
    bool writeFrame(const void* rgb24Data, size_t numBytes);

    /// Close stdin pipe and wait for FFmpeg to finish.  Returns exit code.
    int finish();

    /// Is the child process still running?
    bool isRunning() const;

    /// Collect any stderr output (best-effort, non-blocking).
    juce::String getErrorOutput();

    /// Drain stderr pipe into internal buffer (call periodically to prevent deadlocks).
    void drainStderr();

private:
#ifdef _WIN32
    HANDLE  stdinWrite_    = INVALID_HANDLE_VALUE;
    HANDLE  stderrRead_    = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION pi_ = {};
    bool    started_       = false;
#endif
    juce::String accumulatedStderr_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFmpegProcess)
};
