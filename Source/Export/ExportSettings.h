#pragma once

#include <JuceHeader.h>
#include "PostProcessor.h"

namespace Export
{

//==============================================================================
enum class Resolution
{
    HD_1080p,          // 1920x1080
    QHD_1440p,         // 2560x1440
    UHD_4K,            // 3840x2160
    Vertical_1080,     // 1080x1920  (portrait, social-media)
    Custom
};

inline juce::String resolutionName(Resolution r)
{
    switch (r)
    {
        case Resolution::HD_1080p:      return "1080p (1920x1080)";
        case Resolution::QHD_1440p:     return "1440p (2560x1440)";
        case Resolution::UHD_4K:        return "4K (3840x2160)";
        case Resolution::Vertical_1080: return "Vertical (1080x1920)";
        case Resolution::Custom:        return "Custom";
        default: return "Unknown";
    }
}

inline juce::Point<int> resolutionSize(Resolution r)
{
    switch (r)
    {
        case Resolution::HD_1080p:      return { 1920, 1080 };
        case Resolution::QHD_1440p:     return { 2560, 1440 };
        case Resolution::UHD_4K:        return { 3840, 2160 };
        case Resolution::Vertical_1080: return { 1080, 1920 };
        default:                        return { 1920, 1080 };
    }
}

//==============================================================================
enum class FrameRate
{
    FPS_30 = 30,
    FPS_60 = 60
};

//==============================================================================
enum class VideoCodec
{
    H264_MP4,       // libx264 → .mp4
    H265_MP4,       // libx265 → .mp4
    VP9_WebM,       // libvpx-vp9 → .webm
    ProRes_MOV,     // prores_ks → .mov
    PNG_Sequence    // individual PNGs
};

inline juce::String videoCodecName(VideoCodec c)
{
    switch (c)
    {
        case VideoCodec::H264_MP4:    return "H.264 (MP4)";
        case VideoCodec::H265_MP4:    return "H.265/HEVC (MP4)";
        case VideoCodec::VP9_WebM:    return "VP9 (WebM)";
        case VideoCodec::ProRes_MOV:  return "ProRes (MOV)";
        case VideoCodec::PNG_Sequence:return "PNG Sequence";
        default: return "Unknown";
    }
}

inline juce::String videoCodecExtension(VideoCodec c)
{
    switch (c)
    {
        case VideoCodec::H264_MP4:    return ".mp4";
        case VideoCodec::H265_MP4:    return ".mp4";
        case VideoCodec::VP9_WebM:    return ".webm";
        case VideoCodec::ProRes_MOV:  return ".mov";
        case VideoCodec::PNG_Sequence:return "";
        default: return ".mp4";
    }
}

//==============================================================================
enum class QualityPreset
{
    Draft,   // fast encode, lower quality
    Good,    // balanced
    Best     // slow encode, highest quality
};

inline juce::String qualityPresetName(QualityPreset q)
{
    switch (q)
    {
        case QualityPreset::Draft: return "Draft";
        case QualityPreset::Good:  return "Good";
        case QualityPreset::Best:  return "Best";
        default: return "Good";
    }
}

/// FFmpeg preset string for x264/x265 based on quality.
inline juce::String encoderPreset(QualityPreset q)
{
    switch (q)
    {
        case QualityPreset::Draft: return "ultrafast";
        case QualityPreset::Good:  return "medium";
        case QualityPreset::Best:  return "slow";
        default: return "medium";
    }
}

//==============================================================================
enum class AudioCodec
{
    Passthrough,    // copy original codec
    AAC,
    MP3,
    Opus,
    FLAC
};

inline juce::String audioCodecName(AudioCodec c)
{
    switch (c)
    {
        case AudioCodec::Passthrough: return "Passthrough (copy)";
        case AudioCodec::AAC:         return "AAC";
        case AudioCodec::MP3:         return "MP3";
        case AudioCodec::Opus:        return "Opus";
        case AudioCodec::FLAC:        return "FLAC";
        default: return "AAC";
    }
}

inline juce::String ffmpegAudioEncoder(AudioCodec c)
{
    switch (c)
    {
        case AudioCodec::Passthrough: return "copy";
        case AudioCodec::AAC:         return "aac";
        case AudioCodec::MP3:         return "libmp3lame";
        case AudioCodec::Opus:        return "libopus";
        case AudioCodec::FLAC:        return "flac";
        default: return "aac";
    }
}

//==============================================================================
/// All settings needed for a single video export job.
struct Settings
{
    // Video resolution
    Resolution  resolution   = Resolution::HD_1080p;
    int         customWidth  = 1920;
    int         customHeight = 1080;

    // Frame rate
    FrameRate   frameRate    = FrameRate::FPS_30;

    // Video codec
    VideoCodec  videoCodec   = VideoCodec::H264_MP4;

    // Quality
    QualityPreset qualityPreset = QualityPreset::Good;
    int         bitrateMbps  = 20;      // 1 – 100

    // Audio
    AudioCodec  audioCodec   = AudioCodec::AAC;
    int         audioBitrateKbps = 192;  // kbps

    // Files
    juce::File  audioFile;               // source audio
    juce::File  outputFile;              // destination video

    // Post-processing effects
    PostProcessSettings postProcess;

    //-- Helpers ---------------------------------------------------------------
    int getWidth() const
    {
        int w;
        if (resolution == Resolution::Custom)
            w = (customWidth > 0) ? customWidth : 1920;
        else
            w = resolutionSize(resolution).x;
        return (w % 2 != 0) ? w + 1 : w;  // yuv420p requires even dimensions
    }

    int getHeight() const
    {
        int h;
        if (resolution == Resolution::Custom)
            h = (customHeight > 0) ? customHeight : 1080;
        else
            h = resolutionSize(resolution).y;
        return (h % 2 != 0) ? h + 1 : h;  // yuv420p requires even dimensions
    }

    int getFPS() const { return static_cast<int>(frameRate); }

    /// Wrap a path in double-quotes for command-line safety.
    static juce::String quoted(const juce::String& path)
    {
        return "\"" + path + "\"";
    }

    /// Build the full FFmpeg argument list (raw-video stdin → muxed output).
    juce::StringArray buildFFmpegArgs(const juce::File& ffmpegPath) const
    {
        juce::StringArray args;
        args.add(quoted(ffmpegPath.getFullPathName()));
        args.add("-y");                        // overwrite

        // Input 0: raw video from stdin pipe
        args.add("-f");        args.add("rawvideo");
        args.add("-pix_fmt"); args.add("rgb24");
        args.add("-s");       args.add(juce::String(getWidth()) + "x" + juce::String(getHeight()));
        args.add("-r");       args.add(juce::String(getFPS()));
        args.add("-i");       args.add("pipe:0");

        // Input 1: audio file
        args.add("-i");       args.add(quoted(audioFile.getFullPathName()));

        // Video encoder
        switch (videoCodec)
        {
            case VideoCodec::H264_MP4:
                args.add("-c:v");     args.add("libx264");
                args.add("-preset");  args.add(encoderPreset(qualityPreset));
                args.add("-b:v");     args.add(juce::String(bitrateMbps) + "M");
                args.add("-pix_fmt"); args.add("yuv420p");
                break;

            case VideoCodec::H265_MP4:
                args.add("-c:v");     args.add("libx265");
                args.add("-preset");  args.add(encoderPreset(qualityPreset));
                args.add("-b:v");     args.add(juce::String(bitrateMbps) + "M");
                args.add("-pix_fmt"); args.add("yuv420p");
                args.add("-tag:v");   args.add("hvc1");
                break;

            case VideoCodec::VP9_WebM:
                args.add("-c:v");     args.add("libvpx-vp9");
                args.add("-b:v");     args.add(juce::String(bitrateMbps) + "M");
                args.add("-pix_fmt"); args.add("yuv420p");
                args.add("-deadline"); args.add("good");
                args.add("-cpu-used"); args.add("2");
                break;

            case VideoCodec::ProRes_MOV:
                args.add("-c:v");     args.add("prores_ks");
                args.add("-profile:v"); args.add("3");   // HQ
                args.add("-pix_fmt"); args.add("yuv422p10le");
                break;

            default: break;
        }

        // Audio encoder
        if (audioCodec == AudioCodec::Passthrough)
        {
            args.add("-c:a"); args.add("copy");
        }
        else
        {
            args.add("-c:a"); args.add(ffmpegAudioEncoder(audioCodec));
            args.add("-b:a"); args.add(juce::String(audioBitrateKbps) + "k");
        }

        // Shortest — stop when shorter stream ends
        args.add("-shortest");

        // Output
        args.add(quoted(outputFile.getFullPathName()));

        return args;
    }

    /// Build command line for a single PNG frame save (no FFmpeg needed).
    juce::File pngFramePath(int frameIndex) const
    {
        return outputFile.getChildFile(
            juce::String::formatted("frame_%06d.png", frameIndex));
    }
};

} // namespace Export
