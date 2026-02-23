#pragma once

#include <JuceHeader.h>
#include "SkinnedTitleBarLookAndFeel.h"
#include "ThemeManager.h"

//==============================================================================
/// Standalone window that shows the Shader Documentation page (assets/threed/shaderdocs.html)
/// via a WebBrowserComponent / WebView2 resource provider.
class ShaderDocsWindow : public juce::DocumentWindow
{
public:
    ShaderDocsWindow()
        : juce::DocumentWindow("Shader Documentation",
                               ThemeManager::getInstance().getPalette().windowBg,
                               juce::DocumentWindow::closeButton)
    {
        setLookAndFeel(&titleBarLnf_);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(32);

        browser_ = makeBrowser();
        browser_->setSize(860, 640);
        setContentNonOwned(browser_.get(), true);
        centreWithSize(860, 640);
        setResizable(true, false);
        setVisible(true);
    }

    ~ShaderDocsWindow() override
    {
        setLookAndFeel(nullptr);
        // browser_ must outlive the window content
        clearContentComponent();
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    SkinnedTitleBarLookAndFeel titleBarLnf_;

    //──────────────────────────────────────────────────────────────────────────
    struct DocsBrowser : public juce::WebBrowserComponent
    {
        explicit DocsBrowser(const Options& opts) : juce::WebBrowserComponent(opts) {}
    };

    std::unique_ptr<DocsBrowser> browser_;

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

    std::unique_ptr<DocsBrowser> makeBrowser()
    {
        const auto assetsDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                   .getSiblingFile("assets")
                                   .getChildFile("threed");

        auto provider = [assetsDir](const juce::String& requestPath)
            -> std::optional<juce::WebBrowserComponent::Resource>
        {
            juce::String path = requestPath;
            if (path == "/" || path.isEmpty())
                path = "shaderdocs.html";
            else
                path = path.trimCharactersAtStart("/");

            auto file = assetsDir.getChildFile(path);
            if (!file.existsAsFile())
                return std::nullopt;

            juce::MemoryBlock mb;
            if (!file.loadFileAsData(mb))
                return std::nullopt;

            std::vector<std::byte> bytes(mb.getSize());
            std::memcpy(bytes.data(), mb.getData(), mb.getSize());

            auto mime = mimeForExt(file.getFileExtension().toLowerCase());
            return juce::WebBrowserComponent::Resource{ std::move(bytes), mime.toStdString() };
        };

        auto opts = juce::WebBrowserComponent::Options{}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(
                juce::WebBrowserComponent::Options::WinWebView2{}
                    .withStatusBarDisabled()
                    .withBuiltInErrorPageDisabled())
            .withResourceProvider(provider);

        auto b = std::make_unique<DocsBrowser>(opts);
        b->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        return b;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaderDocsWindow)
};
