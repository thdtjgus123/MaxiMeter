#include "VJShaderEditorDialog.h"

//==============================================================================
// MIME helper (shared with ThreeDWebView — duplicated to keep this unit standalone)
static juce::String mimeForExt(const juce::String& ext)
{
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".js"   || ext == ".mjs") return "application/javascript";
    if (ext == ".css")  return "text/css";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".woff2")return "font/woff2";
    if (ext == ".woff") return "font/woff";
    if (ext == ".ttf")  return "font/ttf";
    return "application/octet-stream";
}

//==============================================================================
// VJShaderEditorWebView
//==============================================================================
VJShaderEditorWebView::VJShaderEditorWebView()
{
    // The HTML page references monaco-editor/ which lives in Assets/threed/
    // but the page itself is in Assets/vj_transition_editor/.
    // We serve both directories by checking them in order.
    assetsDir_ = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                     .getSiblingFile("assets");
    setupBrowser();
    addAndMakeVisible(*browser_);
}

VJShaderEditorWebView::~VJShaderEditorWebView() {}

void VJShaderEditorWebView::resized()
{
    if (browser_)
        browser_->setBounds(getLocalBounds());
}

//==============================================================================
void VJShaderEditorWebView::setupBrowser()
{
    auto provider = [this](const juce::String& path)
        -> std::optional<juce::WebBrowserComponent::Resource>
    {
        return serveResource(path);
    };

    auto opts = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withStatusBarDisabled()
                .withBuiltInErrorPageDisabled())
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider(provider)
        // ── JS → C++ events ─────────────────────────────────────────
        .withEventListener(juce::Identifier{"applySource"},
            [this](const juce::var& data) {
                if (onApply)
                    onApply(data["source"].toString());
            })
        .withEventListener(juce::Identifier{"openFile"},
            [this](const juce::var&) {
                if (onOpenFile)
                    onOpenFile();
            })
        .withEventListener(juce::Identifier{"saveFile"},
            [this](const juce::var& data) {
                if (onSaveFile)
                    onSaveFile(data["source"].toString());
            })
        .withEventListener(juce::Identifier{"loadPreset"},
            [this](const juce::var& data) {
                if (onLoadPreset)
                    onLoadPreset(static_cast<int>(data["index"]));
            });

    browser_ = std::make_unique<Browser>(opts);
    browser_->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
}

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource>
VJShaderEditorWebView::serveResource(const juce::String& requestPath)
{
    juce::String path = requestPath;
    if (path == "/" || path.isEmpty())
        path = "index.html";
    else
        path = path.trimCharactersAtStart("/");

    // Try vj_transition_editor/ first, then threed/ (for monaco-editor assets)
    auto file = assetsDir_.getChildFile("vj_transition_editor").getChildFile(path);
    if (!file.existsAsFile())
        file = assetsDir_.getChildFile("threed").getChildFile(path);
    if (!file.existsAsFile())
        return std::nullopt;

    juce::MemoryBlock mb;
    if (!file.loadFileAsData(mb))
        return std::nullopt;

    std::vector<std::byte> bytes(mb.getSize());
    std::memcpy(bytes.data(), mb.getData(), mb.getSize());

    auto mime = mimeForExt(file.getFileExtension().toLowerCase());
    return juce::WebBrowserComponent::Resource{ std::move(bytes), mime.toStdString() };
}

//==============================================================================
void VJShaderEditorWebView::setSource(const juce::String& glsl)
{
    if (!browser_) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty("source", glsl);
    browser_->emitEventIfBrowserIsVisible("setSource", juce::var(obj));
}

void VJShaderEditorWebView::setPresets(const juce::StringArray& names)
{
    if (!browser_) return;
    juce::Array<juce::var> arr;
    for (auto& n : names)
        arr.add(n);
    auto* obj = new juce::DynamicObject();
    obj->setProperty("presets", juce::var(arr));
    browser_->emitEventIfBrowserIsVisible("setPresets", juce::var(obj));
}

void VJShaderEditorWebView::setError(const juce::String& error)
{
    if (!browser_) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty("error", error);
    browser_->emitEventIfBrowserIsVisible("setError", juce::var(obj));
}

//==============================================================================
// VJShaderEditorDialog
//==============================================================================
VJShaderEditorDialog::VJShaderEditorDialog()
    : juce::DocumentWindow("VJ Transition Shader Editor",
                           juce::Colour(0xFF1e1e1e),
                           juce::DocumentWindow::closeButton)
{
    setLookAndFeel(&lnf_);
    setUsingNativeTitleBar(false);
    setTitleBarHeight(32);
    setContentNonOwned(&editor_, false);
    setResizable(true, false);
    centreWithSize(720, 560);
    setVisible(true);
}

VJShaderEditorDialog::~VJShaderEditorDialog()
{
    setLookAndFeel(nullptr);
}

void VJShaderEditorDialog::closeButtonPressed()
{
    setVisible(false);
}
