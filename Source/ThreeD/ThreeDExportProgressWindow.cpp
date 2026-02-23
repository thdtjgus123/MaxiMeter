#include "ThreeDExportProgressWindow.h"

//==============================================================================
// ContentComp
//==============================================================================
ThreeDExportProgressWindow::ContentComp::ContentComp (ThreeDExportProgressWindow& owner)
    : progressBar_ (progressValue_), owner_ (owner)
{
    setSize (420, 360);

    addAndMakeVisible (progressBar_);
    addAndMakeVisible (frameLabel_);
    addAndMakeVisible (etaLabel_);
    addAndMakeVisible (cancelButton_);

    frameLabel_.setText ("Preparing…", juce::dontSendNotification);

    addChildComponent (logEditor_);
    logEditor_.setMultiLine (true, true);
    logEditor_.setReadOnly (true);
    logEditor_.setScrollbarsShown (true);
    logEditor_.setCaretVisible (false);
    logEditor_.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.0f, 0));

    addChildComponent (copyLogButton_);
    copyLogButton_.onClick = [this]()
    {
        juce::SystemClipboard::copyTextToClipboard (logEditor_.getText());
        copyLogButton_.setButtonText ("Copied!");
        juce::Timer::callAfterDelay (1500, [this]()
        {
            if (copyLogButton_.isShowing())
                copyLogButton_.setButtonText ("Copy Log");
        });
    };

    cancelButton_.onClick = [this]()
    {
        if (owner_.renderer_)
            owner_.renderer_->signalThreadShouldExit();
    };
}

void ThreeDExportProgressWindow::ContentComp::paint (juce::Graphics& g)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll (pal.panelBg);

    if (previewImage_.isValid())
    {
        auto area        = getLocalBounds().reduced (16);
        auto previewArea = area.removeFromTop (200);

        float ia = (float) previewImage_.getWidth() / (float) previewImage_.getHeight();
        float aa = (float) previewArea.getWidth()   / (float) previewArea.getHeight();
        juce::Rectangle<float> dest;
        if (ia > aa)
        {
            float w = (float) previewArea.getWidth();
            float h = w / ia;
            dest = { (float) previewArea.getX(),
                     previewArea.getY() + (previewArea.getHeight() - h) * 0.5f, w, h };
        }
        else
        {
            float h = (float) previewArea.getHeight();
            float w = h * ia;
            dest = { previewArea.getX() + (previewArea.getWidth() - w) * 0.5f,
                     (float) previewArea.getY(), w, h };
        }
        g.drawImage (previewImage_, dest);
    }
}

void ThreeDExportProgressWindow::ContentComp::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (200 + 8);   // preview region

    frameLabel_.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    etaLabel_.setBounds (area.removeFromTop (18));
    area.removeFromTop (8);
    progressBar_.setBounds (area.removeFromTop (20));
    area.removeFromTop (10);
    cancelButton_.setBounds (area.removeFromTop (28).removeFromLeft (90));

    if (logVisible_)
    {
        area.removeFromTop (10);
        logEditor_.setBounds   (area.removeFromTop (120));
        copyLogButton_.setBounds (area.removeFromTop (24).removeFromLeft (80));
    }
}

void ThreeDExportProgressWindow::ContentComp::setProgress (float p)
{
    progressValue_ = (double) p;
}

void ThreeDExportProgressWindow::ContentComp::setFrameText (const juce::String& t)
{
    frameLabel_.setText (t, juce::dontSendNotification);
}

void ThreeDExportProgressWindow::ContentComp::setEtaText (const juce::String& t)
{
    etaLabel_.setText (t, juce::dontSendNotification);
}

void ThreeDExportProgressWindow::ContentComp::showLog (const juce::String& log)
{
    logEditor_.setText (log);
    logEditor_.setVisible (true);
    copyLogButton_.setVisible (true);
    logVisible_ = true;
    setSize (getWidth(), 540);
    resized();
}

void ThreeDExportProgressWindow::ContentComp::setPreview (const juce::Image& img)
{
    previewImage_ = img;
    repaint();
}

//==============================================================================
// ThreeDExportProgressWindow
//==============================================================================
SkinnedTitleBarLookAndFeel& ThreeDExportProgressWindow::lnf()
{
    static SkinnedTitleBarLookAndFeel s;
    s.updateColours();
    return s;
}

ThreeDExportProgressWindow::ThreeDExportProgressWindow (
    std::unique_ptr<ThreeDOfflineRenderer> renderer)
    : juce::DocumentWindow ("3D Video Export",
                            juce::Colours::black,
                            juce::DocumentWindow::closeButton),
      renderer_ (std::move (renderer))
{
    setUsingNativeTitleBar (false);
    setLookAndFeel (&lnf());

    auto* content = new ContentComp (*this);
    setContentOwned (content, true);
    setSize (452, content->getHeight() + getTitleBarHeight());
    setResizable (true, false);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);

    renderer_->addListener (this);
    renderer_->startThread (juce::Thread::Priority::normal);
    startTimer (120);   // poll for preview updates
}

ThreeDExportProgressWindow::~ThreeDExportProgressWindow()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void ThreeDExportProgressWindow::closeButtonPressed()
{
    if (renderer_ && renderer_->isThreadRunning())
    {
        renderer_->signalThreadShouldExit();
        return;   // wait for the thread to finish; it will call threed_renderFinished
    }

    if (onClose) onClose();
    setLookAndFeel (nullptr);
    delete this;
}

void ThreeDExportProgressWindow::timerCallback()
{
    if (!renderer_) return;

    auto img = renderer_->getLatestPreview();
    if (img.isValid())
        if (auto* c = dynamic_cast<ContentComp*> (getContentComponent()))
            c->setPreview (img);
}

//==============================================================================
void ThreeDExportProgressWindow::threed_renderProgress (float progress,
                                                        int   currentFrame,
                                                        int   totalFrames,
                                                        double etaSeconds)
{
    auto* c = dynamic_cast<ContentComp*> (getContentComponent());
    if (!c) return;

    c->setProgress  (progress);
    c->setFrameText ("Frame " + juce::String (currentFrame) +
                     " / " + juce::String (totalFrames));

    if (etaSeconds > 0.0)
    {
        int etaMin = (int) etaSeconds / 60;
        int etaSec = (int) etaSeconds % 60;
        juce::String eta;
        if (etaMin > 0) eta = juce::String (etaMin) + "m ";
        eta += juce::String (etaSec) + "s remaining";
        c->setEtaText (eta);
    }
}

void ThreeDExportProgressWindow::threed_renderFinished (bool success,
                                                        const juce::String& message)
{
    finished_ = true;
    stopTimer();

    if (success)
    {
        threed_renderProgress (1.0f, 0, 0, 0.0);   // fill bar to 100 %

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Export Complete", message,
            "OK", this,
            juce::ModalCallbackFunction::create ([this] (int)
            {
                if (onClose) onClose();
                setLookAndFeel (nullptr);
                delete this;
            }));
    }
    else
    {
        auto* c = dynamic_cast<ContentComp*> (getContentComponent());
        if (c) c->showLog (message);

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Export Failed", message,
            "OK", this,
            juce::ModalCallbackFunction::create ([this] (int)
            {
                if (onClose) onClose();
                setLookAndFeel (nullptr);
                delete this;
            }));
    }
}
