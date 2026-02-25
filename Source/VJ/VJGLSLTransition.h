#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Renders a GLTransitions-compatible fragment shader on the CPU.
///
/// The user writes a single GLSL-style function:
///   vec4 transition(vec2 uv) { ... }
/// with access to:
///   uniform sampler2D from, to;
///   uniform float     progress;   // 0 → 1
///   uniform float     ratio;      // width / height
///   vec4 getFromColor(vec2 uv);
///   vec4 getToColor(vec2 uv);
///
/// Because we can't compile real GLSL without a GL context on-screen,
/// the shader source is stored as-is and rendered by the C++ paint path
/// as a GPU shader at display time.  A "preview" mode interprets simple
/// mix/step/smoothstep patterns to give a rough CPU approximation.
///
/// When an OpenGL context is available (VJEditor supplies one), the
/// shader is compiled via juce::OpenGLShaderProgram and rendered to an
/// off-screen FBO for pixel-perfect output.
class VJGLSLTransition
{
public:
    VJGLSLTransition() = default;

    /// Set the user's transition GLSL source (the body of the transition fn).
    /// Returns true if the source was accepted (basic validation).
    bool setSource(const juce::String& glslSource);

    /// Get the current source code.
    const juce::String& getSource() const noexcept { return source_; }

    /// Get the last compilation / validation error, or empty if OK.
    const juce::String& getLastError() const noexcept { return lastError_; }

    /// Has the source been set and at least minimally valid?
    bool isValid() const noexcept { return valid_; }

    //==========================================================================
    /// Render the transition frame into the destination image.
    /// This uses a simple CPU fallback (bilinear sample + per-pixel eval)
    /// for the subset of GLTransitions patterns we can interpret.
    ///
    /// For complex shaders the result is an approximation; the real GPU path
    /// is used when the OpenGL context is available.
    void renderCPU(juce::Image& dest,
                   const juce::Image& fromImg,
                   const juce::Image& toImg,
                   float progress) const;

    //==========================================================================
    /// Name / label for display in the UI.
    void setName(const juce::String& n) { name_ = n; }
    const juce::String& getName() const noexcept { return name_; }

    /// File path if loaded from disk.
    void setFilePath(const juce::File& f) { filePath_ = f; }
    const juce::File& getFilePath() const noexcept { return filePath_; }

private:
    juce::String source_;
    juce::String lastError_;
    juce::String name_   { "Custom" };
    juce::File   filePath_;
    bool         valid_  = false;

    /// Minimal source validation (checks for 'transition' function).
    bool validate(const juce::String& src);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VJGLSLTransition)
};
