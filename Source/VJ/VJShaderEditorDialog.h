#pragma once

#include <JuceHeader.h>
#include "../UI/ThemeManager.h"
#include "../UI/SkinnedTitleBarLookAndFeel.h"

//==============================================================================
/// WebView2-based GLSL transition editor (Monaco Editor).
/// Hosted inside a DocumentWindow via VJShaderEditorDialog.
class VJShaderEditorWebView : public juce::Component
{
public:
    VJShaderEditorWebView();
    ~VJShaderEditorWebView() override;

    void resized() override;

    /// Set editor content programmatically.
    void setSource(const juce::String& glsl);

    /// Get the current editor content via callback.
    /// (Direct read not possible — JS communicates via events.)

    /// Populate the preset dropdown.
    void setPresets(const juce::StringArray& names);

    /// Show a compilation error below the editor.
    void setError(const juce::String& error);

    //==========================================================================
    // Callbacks — set by the dialog owner
    std::function<void(const juce::String& source)>  onApply;
    std::function<void()>                             onOpenFile;
    std::function<void(const juce::String& source)>  onSaveFile;
    std::function<void(int presetIndex)>              onLoadPreset;

private:
    struct Browser : public juce::WebBrowserComponent
    {
        explicit Browser(const Options& opts) : juce::WebBrowserComponent(opts) {}
        void pageFinishedLoading(const juce::String&) override { pageReady_ = true; }
        bool pageReady_ = false;
    };

    std::unique_ptr<Browser> browser_;
    juce::File assetsDir_;

    void setupBrowser();
    std::optional<juce::WebBrowserComponent::Resource>
        serveResource(const juce::String& requestPath);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VJShaderEditorWebView)
};

//==============================================================================
/// Self-contained dialog window that wraps VJShaderEditorWebView.
class VJShaderEditorDialog : public juce::DocumentWindow
{
public:
    VJShaderEditorDialog();
    ~VJShaderEditorDialog() override;

    void closeButtonPressed() override;

    VJShaderEditorWebView& getEditor() { return editor_; }

private:
    SkinnedTitleBarLookAndFeel lnf_;
    VJShaderEditorWebView     editor_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VJShaderEditorDialog)
};
