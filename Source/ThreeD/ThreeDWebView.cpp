#include "ThreeDWebView.h"

//==============================================================================
// MIME type helper
static juce::String mimeForExtension (const juce::String& ext)
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
ThreeDWebView::ThreeDWebView()
{
    // Locate assets directory next to the application executable
    assetsDir_ = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                     .getSiblingFile ("assets")
                     .getChildFile  ("threed");

    setupBrowser();
    addAndMakeVisible (*browser_);
}

ThreeDWebView::~ThreeDWebView() {}

//==============================================================================
void ThreeDWebView::setupBrowser()
{
    // Resource provider: serves every file under assetsDir_
    auto provider = [this] (const juce::String& path)
        -> std::optional<juce::WebBrowserComponent::Resource>
    {
        return serveResource (path);
    };

    auto opts = juce::WebBrowserComponent::Options{}
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withStatusBarDisabled()
                .withBuiltInErrorPageDisabled())
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withNativeIntegrationEnabled()
        .withResourceProvider (provider)
        .withEventListener (juce::Identifier { "frameCaptured" },
            [this] (const juce::var& data)
            {
                // Runs on the message thread.
                juce::String dataUrl = data["dataUrl"].toString();
                // Strip the "data:image/png;base64," prefix
                auto base64 = dataUrl.fromFirstOccurrenceOf (",", false, false);
                if (base64.isEmpty()) return;

                juce::MemoryOutputStream mos;
                if (juce::Base64::convertFromBase64 (mos, base64))
                {
                    juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
                    auto img = juce::ImageFileFormat::loadFrom (mis);
                    if (img.isValid() && onCaptureDone)
                        onCaptureDone (img);
                }
            })
        .withEventListener (juce::Identifier { "frameReady" },
            [this] (const juce::var& data)
            {
                // Invoked on the message thread after an exportFrame call.
                if (onFrameReady)
                {
                    juce::String dataUrl = data["dataUrl"].toString();
                    auto base64 = dataUrl.fromFirstOccurrenceOf (",", false, false);
                    int frameIndex = static_cast<int> (data["frameIndex"]);
                    onFrameReady (base64, frameIndex);
                }
            })
        .withEventListener (juce::Identifier { "openExternalEditor" },
            [] (const juce::var& data)
            {
                // Save to CustomComponents/3D/shaders/ next to the executable
                auto exeDir   = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
                auto shadersDir = exeDir.getChildFile ("CustomComponents")
                                        .getChildFile ("3D")
                                        .getChildFile ("shaders");
                shadersDir.createDirectory();

                // Build a safe filename from the layer name (replace non-alnum with _)
                juce::String rawName = data["name"].toString().trim();
                if (rawName.isEmpty()) rawName = "shader";
                juce::String safeName;
                for (auto ch : rawName)
                    safeName += (juce::CharacterFunctions::isLetterOrDigit (ch) ? ch : juce::juce_wchar ('_'));
                auto outFile = shadersDir.getChildFile (safeName + ".glsl");

                juce::String content;
                content << "// === VERTEX ===" << juce::newLine
                        << data["vert"].toString() << juce::newLine
                        << juce::newLine
                        << "// === FRAGMENT ===" << juce::newLine
                        << data["frag"].toString() << juce::newLine;

                outFile.replaceWithText (content);

                // Try VSCode first (code.cmd is in PATH on most installs)
                juce::ChildProcess proc;
                if (proc.start ("cmd.exe /c code \"" + outFile.getFullPathName() + "\""))
                    return;

                // Fallback: open in system default .glsl handler
                outFile.startAsProcess();
            })
        .withEventListener (juce::Identifier { "requestShaderFiles" },
            [this] (const juce::var& /*data*/)
            {
                // Scan CustomComponents/3D/shaders/*.glsl and send all files to JS
                auto exeDir     = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
                auto shadersDir = exeDir.getChildFile ("CustomComponents")
                                        .getChildFile ("3D")
                                        .getChildFile ("shaders");

                juce::Array<juce::var> files;
                if (shadersDir.isDirectory())
                {
                    for (auto& f : shadersDir.findChildFiles (juce::File::findFiles, false, "*.glsl"))
                    {
                        auto* obj = new juce::DynamicObject();
                        obj->setProperty ("name",    f.getFileNameWithoutExtension());
                        obj->setProperty ("content", f.loadFileAsString());
                        files.add (juce::var (obj));
                    }
                }

                auto* payload = new juce::DynamicObject();
                payload->setProperty ("files", juce::var (files));
                if (browser_)
                    browser_->emitEventIfBrowserIsVisible ("shaderFiles", juce::var (payload));
            });

    browser_ = std::make_unique<Browser> (opts);

    // Navigate to the resource-provider root → serves index.html
    browser_->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

//==============================================================================
std::optional<juce::WebBrowserComponent::Resource>
ThreeDWebView::serveResource (const juce::String& requestPath)
{
    juce::String path = requestPath;
    if (path == "/" || path.isEmpty())
        path = "index.html";
    else
        path = path.trimCharactersAtStart ("/");

    auto file = assetsDir_.getChildFile (path);
    if (!file.existsAsFile())
        return std::nullopt;

    // Load file bytes
    juce::MemoryBlock mb;
    if (!file.loadFileAsData (mb))
        return std::nullopt;

    std::vector<std::byte> bytes (mb.getSize());
    std::memcpy (bytes.data(), mb.getData(), mb.getSize());

    auto mime = mimeForExtension (file.getFileExtension().toLowerCase());
    return juce::WebBrowserComponent::Resource { std::move (bytes), mime.toStdString() };
}

//==============================================================================
void ThreeDWebView::setAudioData (const std::vector<float>& waveformMono,
                                   const std::vector<float>& spectrumMag)
{
    if (!browser_) return;

    constexpr int N = 64;   // 64 waveform + 64 spectrum = 128 samples total

    juce::Array<juce::var> wArr, sArr;
    wArr.ensureStorageAllocated (N);
    sArr.ensureStorageAllocated (N);

    for (int i = 0; i < N; i++)
    {
        float wi = (!waveformMono.empty())
            ? waveformMono[i * (int)waveformMono.size() / N]
            : 0.0f;
        float si = (!spectrumMag.empty())
            ? spectrumMag [i * (int)spectrumMag.size()  / N]
            : 0.0f;
        wArr.add (wi);
        sArr.add (si);
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("waveform", juce::var (wArr));
    obj->setProperty ("spectrum", juce::var (sArr));

    browser_->emitEventIfBrowserIsVisible ("audioData", juce::var (obj));
}

void ThreeDWebView::applyTheme (bool isDark)
{
    if (!browser_) return;

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("dark", juce::var (isDark));
    browser_->emitEventIfBrowserIsVisible ("themeChange", juce::var (obj));
}

void ThreeDWebView::resetCamera()
{
    if (!browser_) return;
    browser_->emitEventIfBrowserIsVisible ("resetCamera",
                                            juce::var (new juce::DynamicObject()));
}

void ThreeDWebView::requestCapture()
{
    if (!browser_) return;
    browser_->emitEventIfBrowserIsVisible ("requestCapture",
                                            juce::var (new juce::DynamicObject()));
}

void ThreeDWebView::exportFrame (int frameIndex, int totalFrames, double timeSeconds,
                                  const std::vector<float>& waveform,
                                  const std::vector<float>& spectrum)
{
    if (!browser_) return;

    constexpr int N = 64;
    juce::Array<juce::var> wArr, sArr;
    wArr.ensureStorageAllocated (N);
    sArr.ensureStorageAllocated (N);
    for (int i = 0; i < N; i++)
    {
        float wi = (!waveform.empty())  ? waveform [i * (int)waveform.size()  / N] : 0.0f;
        float si = (!spectrum.empty())  ? spectrum [i * (int)spectrum.size()  / N] : 0.0f;
        wArr.add (wi);
        sArr.add (si);
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("frameIndex",  frameIndex);
    obj->setProperty ("totalFrames", totalFrames);
    obj->setProperty ("timeSeconds", timeSeconds);
    obj->setProperty ("waveform",    juce::var (wArr));
    obj->setProperty ("spectrum",    juce::var (sArr));
    browser_->emitEventIfBrowserIsVisible ("exportFrame", juce::var (obj));
}

void ThreeDWebView::exportStop()
{
    if (!browser_) return;
    browser_->emitEventIfBrowserIsVisible ("exportStop",
                                            juce::var (new juce::DynamicObject()));
}

//==============================================================================
void ThreeDWebView::resized()
{
    if (browser_)
        browser_->setBounds (getLocalBounds());
}

//==============================================================================
// Browser inner class
void ThreeDWebView::Browser::pageFinishedLoading (const juce::String& /*url*/)
{
    pageLoaded_ = true;
    // The JS side handles all initialisation via event listeners
}
