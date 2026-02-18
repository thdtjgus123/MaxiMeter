#include "ExportProgressWindow.h"

//==============================================================================
// ContentComp
//==============================================================================
ExportProgressWindow::ContentComp::ContentComp(ExportProgressWindow& owner)
    : progressBar_(progressValue_), owner_(owner)
{
    setSize(420, 400);

    addAndMakeVisible(progressBar_);
    addAndMakeVisible(frameLabel_);
    addAndMakeVisible(etaLabel_);
    addAndMakeVisible(statusLabel_);
    addAndMakeVisible(pauseButton_);
    addAndMakeVisible(cancelButton_);

    frameLabel_.setText("Preparing...", juce::dontSendNotification);
    etaLabel_.setText("", juce::dontSendNotification);
    statusLabel_.setText("", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(12.0f));

    // Log editor — initially hidden
    addChildComponent(logEditor_);
    logEditor_.setMultiLine(true, true);
    logEditor_.setReadOnly(true);
    logEditor_.setScrollbarsShown(true);
    logEditor_.setCaretVisible(false);
    logEditor_.setColour(juce::TextEditor::backgroundColourId,
                         ThemeManager::getInstance().getPalette().windowBg);
    logEditor_.setColour(juce::TextEditor::textColourId,
                         ThemeManager::getInstance().getPalette().bodyText);
    logEditor_.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, 0));

    addChildComponent(copyLogButton_);
    copyLogButton_.onClick = [this]()
    {
        juce::SystemClipboard::copyTextToClipboard(logEditor_.getText());
        copyLogButton_.setButtonText("Copied!");
        juce::Timer::callAfterDelay(1500, [this]() {
            if (copyLogButton_.isShowing())
                copyLogButton_.setButtonText("Copy Log");
        });
    };

    pauseButton_.onClick = [this]()
    {
        if (owner_.renderer_)
        {
            if (owner_.renderer_->isPaused())
            {
                owner_.renderer_->resume();
                pauseButton_.setButtonText("Pause");
            }
            else
            {
                owner_.renderer_->pause();
                pauseButton_.setButtonText("Resume");
            }
        }
    };

    cancelButton_.onClick = [this]()
    {
        if (owner_.renderer_)
            owner_.renderer_->cancel();
    };
}

void ExportProgressWindow::ContentComp::paint(juce::Graphics& g)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll(pal.panelBg);

    // Draw preview image at top
    if (previewImage_.isValid())
    {
        auto area = getLocalBounds().reduced(16);
        auto previewArea = area.removeFromTop(200);

        // Fit preview into available area preserving aspect ratio
        float imgAspect = static_cast<float>(previewImage_.getWidth())
                        / static_cast<float>(previewImage_.getHeight());
        float areaAspect = static_cast<float>(previewArea.getWidth())
                         / static_cast<float>(previewArea.getHeight());

        juce::Rectangle<float> dest;
        if (imgAspect > areaAspect)
        {
            float w = static_cast<float>(previewArea.getWidth());
            float h = w / imgAspect;
            dest = { previewArea.getX() + 0.0f,
                     previewArea.getY() + (previewArea.getHeight() - h) * 0.5f,
                     w, h };
        }
        else
        {
            float h = static_cast<float>(previewArea.getHeight());
            float w = h * imgAspect;
            dest = { previewArea.getX() + (previewArea.getWidth() - w) * 0.5f,
                     previewArea.getY() + 0.0f,
                     w, h };
        }

        g.setColour(pal.windowBg);
        g.fillRect(previewArea);
        g.drawImage(previewImage_, dest);
    }
    else
    {
        auto area = getLocalBounds().reduced(16);
        auto previewArea = area.removeFromTop(200);
        g.setColour(pal.windowBg);
        g.fillRect(previewArea);
        g.setColour(pal.dimText);
        g.setFont(13.0f);
        g.drawText("Preview", previewArea, juce::Justification::centred);
    }
}

void ExportProgressWindow::ContentComp::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Preview area at top
    area.removeFromTop(210);

    frameLabel_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    progressBar_.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    etaLabel_.setBounds(area.removeFromTop(20));
    area.removeFromTop(4);
    statusLabel_.setBounds(area.removeFromTop(20));

    // Error log area (below status, above buttons)
    if (logEditor_.isVisible())
    {
        area.removeFromTop(6);
        auto logArea = area.removeFromTop(140);
        logEditor_.setBounds(logArea);
        area.removeFromTop(4);
        copyLogButton_.setBounds(area.removeFromTop(28).removeFromLeft(100));
    }

    auto buttonRow = area.removeFromBottom(32);
    pauseButton_.setBounds(buttonRow.removeFromLeft(100));
    buttonRow.removeFromLeft(10);
    cancelButton_.setBounds(buttonRow.removeFromLeft(100));
}

void ExportProgressWindow::ContentComp::showErrorLog(const juce::String& logText)
{
    logEditor_.setText(logText, false);
    logEditor_.setVisible(true);
    copyLogButton_.setVisible(true);
    setSize(460, 600);   // expand to fit log area
    if (auto* pw = findParentComponentOfClass<ExportProgressWindow>())
        pw->centreWithSize(460, 600 + pw->getTitleBarHeight());
}

//==============================================================================
// ExportProgressWindow
//==============================================================================
ExportProgressWindow::ExportProgressWindow(std::unique_ptr<OfflineRenderer> renderer)
    : juce::DocumentWindow("Exporting Video...",
                           ThemeManager::getInstance().getPalette().windowBg,
                           juce::DocumentWindow::closeButton),
      renderer_(std::move(renderer)),
      content_(*this)
{
    setContentNonOwned(&content_, true);
    setResizable(false, false);
    centreWithSize(content_.getWidth(), content_.getHeight() + getTitleBarHeight());
    setLookAndFeel(&titleBarLnf_);
    setUsingNativeTitleBar(false);
    setTitleBarHeight(32);
    setVisible(true);

    renderer_->addListener(this);
    renderer_->startThread();

    // Poll for preview frames at ~12 fps
    startTimer(80);
}

ExportProgressWindow::~ExportProgressWindow()
{
    stopTimer();
    setLookAndFeel(nullptr);

    if (renderer_)
    {
        renderer_->removeListener(this);
        renderer_->cancel();
        renderer_->stopThread(10000);
    }
}

void ExportProgressWindow::closeButtonPressed()
{
    stopTimer();

    if (renderer_)
    {
        renderer_->removeListener(this);
        renderer_->cancel();
        renderer_->stopThread(5000);
    }
    setVisible(false);

    auto closeCb = onClose;
    // Self-delete safely on next message loop iteration
    juce::MessageManager::callAsync([this, closeCb]() {
        if (closeCb) closeCb();
        delete this;
    });
}

void ExportProgressWindow::timerCallback()
{
    if (renderer_)
    {
        auto preview = renderer_->getLatestPreview();
        if (preview.isValid())
        {
            content_.previewImage_ = preview;
            content_.repaint();
        }
    }
}

//==============================================================================
void ExportProgressWindow::renderingProgress(float progress, int currentFrame,
                                              int totalFrames, double etaSec)
{
    // Already on the message thread (OfflineRenderer calls via MessageManager::callAsync)
    content_.progressValue_ = static_cast<double>(progress);
    content_.frameLabel_.setText(
        "Frame " + juce::String(currentFrame) + " / " + juce::String(totalFrames),
        juce::dontSendNotification);

    if (etaSec > 0)
    {
        int mins = static_cast<int>(etaSec) / 60;
        int secs = static_cast<int>(etaSec) % 60;
        content_.etaLabel_.setText(
            "ETA: " + juce::String(mins) + "m " + juce::String(secs) + "s",
            juce::dontSendNotification);
    }

    content_.repaint();
}

void ExportProgressWindow::renderingFinished(bool success, const juce::String& message)
{
    finished_ = true;
    content_.progressValue_ = success ? 1.0 : content_.progressValue_;
    content_.frameLabel_.setText(success ? "Complete!" : "Failed", juce::dontSendNotification);
    content_.etaLabel_.setText("", juce::dontSendNotification);
    content_.statusLabel_.setText(
        success ? message : "Export failed — see error log below.",
        juce::dontSendNotification);
    content_.statusLabel_.setColour(juce::Label::textColourId,
        success ? juce::Colours::limegreen : juce::Colours::orangered);

    content_.pauseButton_.setEnabled(false);
    content_.cancelButton_.setButtonText("Close");
    content_.cancelButton_.onClick = [this]() { closeButtonPressed(); };

    if (!success)
    {
        // Show full error log in a copyable text editor
        content_.showErrorLog(message);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Export Complete",
            message);
    }
}
