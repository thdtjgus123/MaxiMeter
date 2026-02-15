#pragma once

#include <JuceHeader.h>
#include "OfflineRenderer.h"

//==============================================================================
/// Floating window that shows export progress — progress bar, frame counter,
/// estimated time remaining, pause/resume and cancel buttons.
class ExportProgressWindow : public juce::DocumentWindow,
                             public OfflineRenderer::Listener,
                             public juce::Timer
{
public:
    ExportProgressWindow(std::unique_ptr<OfflineRenderer> renderer);
    ~ExportProgressWindow() override;

    void closeButtonPressed() override;

    // Timer — polls for preview frames
    void timerCallback() override;

    // OfflineRenderer::Listener
    void renderingProgress(float progress, int currentFrame, int totalFrames,
                           double estimatedSecondsRemaining) override;
    void renderingFinished(bool success, const juce::String& message) override;

    /// Called when the window is about to close / self-delete.
    std::function<void()> onClose;

private:
    //-- Inner content component  ---------------------------------------------
    class ContentComp : public juce::Component
    {
    public:
        ContentComp(ExportProgressWindow& owner);
        void paint(juce::Graphics& g) override;
        void resized() override;

        void showErrorLog(const juce::String& logText);

        juce::ProgressBar  progressBar_;
        juce::Label        frameLabel_;
        juce::Label        etaLabel_;
        juce::Label        statusLabel_;
        juce::TextButton   pauseButton_  { "Pause" };
        juce::TextButton   cancelButton_ { "Cancel" };
        juce::TextEditor   logEditor_;
        juce::TextButton   copyLogButton_  { "Copy Log" };

        juce::Image        previewImage_;   ///< Latest preview frame from renderer

        double progressValue_ = 0.0;

    private:
        ExportProgressWindow& owner_;
    };

    std::unique_ptr<OfflineRenderer>  renderer_;
    ContentComp                       content_;
    bool                              finished_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportProgressWindow)
};
