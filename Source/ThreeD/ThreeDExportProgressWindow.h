#pragma once

#include <JuceHeader.h>
#include "ThreeDOfflineRenderer.h"
#include "../UI/SkinnedTitleBarLookAndFeel.h"
#include "../UI/ThemeManager.h"

//==============================================================================
/// Floating window that shows 3D video export progress.
/// Owns the ThreeDOfflineRenderer and self-deletes when closed.
class ThreeDExportProgressWindow : public juce::DocumentWindow,
                                   public ThreeDOfflineRenderer::Listener,
                                   public juce::Timer
{
public:
    ThreeDExportProgressWindow (std::unique_ptr<ThreeDOfflineRenderer> renderer);
    ~ThreeDExportProgressWindow() override;

    void closeButtonPressed() override;
    void timerCallback()      override;

    // ThreeDOfflineRenderer::Listener
    void threed_renderProgress (float progress, int currentFrame,
                                int totalFrames, double etaSeconds) override;
    void threed_renderFinished (bool success, const juce::String& message) override;

    /// Called when the window is about to self-delete.
    std::function<void()> onClose;

private:
    //==========================================================================
    class ContentComp : public juce::Component
    {
    public:
        explicit ContentComp (ThreeDExportProgressWindow& owner);
        void paint   (juce::Graphics& g) override;
        void resized () override;

        void setProgress  (float p);
        void setFrameText (const juce::String& t);
        void setEtaText   (const juce::String& t);
        void showLog      (const juce::String& log);
        void setPreview   (const juce::Image& img);

    private:
        ThreeDExportProgressWindow& owner_;

        juce::Image      previewImage_;
        double           progressValue_ = 0.0;
        juce::ProgressBar progressBar_  { progressValue_ };
        juce::Label      frameLabel_;
        juce::Label      etaLabel_;
        juce::TextButton cancelButton_ { "Cancel" };
        juce::TextEditor logEditor_;
        juce::TextButton copyLogButton_ { "Copy Log" };
        bool             logVisible_ = false;
    };

    //==========================================================================
    static SkinnedTitleBarLookAndFeel& lnf();

    std::unique_ptr<ThreeDOfflineRenderer> renderer_;
    bool finished_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeDExportProgressWindow)
};
