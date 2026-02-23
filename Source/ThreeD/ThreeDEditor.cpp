#include "ThreeDEditor.h"
#include "../UI/ThemeManager.h"
#include "../UI/SkinnedTitleBarLookAndFeel.h"

//==============================================================================
ThreeDEditor::ThreeDEditor (AudioEngine& audioEngine, FFTProcessor& fftProcessor)
    : audioEngine_ (audioEngine), fftProcessor_ (fftProcessor)
{
    addAndMakeVisible (webView_);
    ThemeManager::getInstance().addListener (this);
    startTimerHz (30);

    // Apply initial theme
    bool dark = (ThemeManager::getInstance().getCurrentTheme() != AppTheme::Light);
    webView_.applyTheme (dark);
}

ThreeDEditor::~ThreeDEditor()
{
    ThemeManager::getInstance().removeListener (this);
    stopTimer();
}

//==============================================================================
void ThreeDEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0d17));
}

void ThreeDEditor::resized()
{
    webView_.setBounds (getLocalBounds());
}

void ThreeDEditor::timerCallback()
{
    timerTick();
}

void ThreeDEditor::timerTick()
{
    constexpr int N = 256;

    // Waveform
    std::vector<float> wave (N, 0.0f);
    int got = audioEngine_.getLatestMonoSamples (wave.data(), N);
    if (got <= 0) wave.assign (N, 0.0f);

    // Spectrum
    int specN = fftProcessor_.getSpectrumSize();
    const float* specData = fftProcessor_.getSpectrumData();
    std::vector<float> spec (specData, specData + std::min (specN, N));
    spec.resize (N, 0.0f);

    webView_.setAudioData (wave, spec);
}

//==============================================================================
void ThreeDEditor::themeChanged (AppTheme theme)
{
    webView_.applyTheme (theme != AppTheme::Light);
}

//==============================================================================
void ThreeDEditor::showRenderPreview3D()
{
    // Ask the JS side to render and send back a frame.
    // When it arrives, show the preview + export window.
    webView_.onCaptureDone = [] (const juce::Image& img)
    {
        if (!img.isValid()) return;

        // ── Preview image component ──────────────────────────────────────────
        struct PreviewComp : public juce::Component
        {
            juce::Image image;
            juce::TextButton savePng  { "Export PNG"  };
            juce::TextButton saveJpeg { "Export JPEG" };

            explicit PreviewComp (const juce::Image& i) : image (i)
            {
                addAndMakeVisible (savePng);
                addAndMakeVisible (saveJpeg);

                savePng.onClick = [this]
                {
                    auto chooser = std::make_shared<juce::FileChooser> (
                        "Save PNG...", juce::File {}, "*.png");
                    chooser->launchAsync (
                        juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectFiles,
                        [this, chooser] (const juce::FileChooser& fc)
                        {
                            if (fc.getResults().isEmpty()) return;
                            juce::FileOutputStream fos (fc.getResult().withFileExtension (".png"));
                            if (fos.openedOk())
                                juce::PNGImageFormat {}.writeImageToStream (image, fos);
                        });
                };

                saveJpeg.onClick = [this]
                {
                    auto chooser = std::make_shared<juce::FileChooser> (
                        "Save JPEG...", juce::File {}, "*.jpg");
                    chooser->launchAsync (
                        juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectFiles,
                        [this, chooser] (const juce::FileChooser& fc)
                        {
                            if (fc.getResults().isEmpty()) return;
                            juce::FileOutputStream fos (fc.getResult().withFileExtension (".jpg"));
                            if (fos.openedOk())
                                juce::JPEGImageFormat {}.writeImageToStream (image, fos);
                        });
                };
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (juce::Colours::black);
                auto imgArea = getLocalBounds().withTrimmedBottom (40);
                float sx = (float) imgArea.getWidth()  / (float) image.getWidth();
                float sy = (float) imgArea.getHeight() / (float) image.getHeight();
                float sc = std::min (sx, sy);
                float w  = image.getWidth()  * sc;
                float h  = image.getHeight() * sc;
                float x  = imgArea.getX() + (imgArea.getWidth()  - w) * 0.5f;
                float y  = imgArea.getY() + (imgArea.getHeight() - h) * 0.5f;
                g.drawImage (image, { x, y, w, h });
            }

            void resized() override
            {
                auto row = getLocalBounds().removeFromBottom (36).reduced (8, 4);
                savePng .setBounds (row.removeFromLeft (110));
                row.removeFromLeft (8);
                saveJpeg.setBounds (row.removeFromLeft (110));
            }
        };

        // ── Self-deleting document window ────────────────────────────────────
        struct PreviewWindow : public juce::DocumentWindow
        {
            static SkinnedTitleBarLookAndFeel& lnf()
            {
                static SkinnedTitleBarLookAndFeel s;
                s.updateColours();
                return s;
            }

            PreviewWindow (const juce::Image& i)
                : juce::DocumentWindow ("3D Render Preview  (" +
                                        juce::String (i.getWidth()) + " x " +
                                        juce::String (i.getHeight()) + ")",
                                        juce::Colours::black,
                                        juce::DocumentWindow::closeButton)
            {
                setUsingNativeTitleBar (false);
                setLookAndFeel (&lnf());
                setContentOwned (new PreviewComp (i), false);
                float ratio = (float) i.getWidth() / (float) std::max (1, i.getHeight());
                int   w     = 960;
                int   h     = juce::roundToInt (w / ratio) + getTitleBarHeight() + 40;
                setSize (w, h);
                setResizable (true, false);
                centreWithSize (w, h);
                setVisible (true);
            }

            void closeButtonPressed() override { setLookAndFeel (nullptr); delete this; }
        };

        new PreviewWindow (img);   // self-deletes on close
    };

    webView_.requestCapture();
}

//==============================================================================
void ThreeDEditor::startVideoExport (const Export::Settings& settings)
{
    auto renderer = std::make_unique<ThreeDOfflineRenderer> (settings, webView_);

    // The progress window takes ownership of the renderer and self-deletes on close
    auto* win = new ThreeDExportProgressWindow (std::move (renderer));
    juce::ignoreUnused (win);
}
