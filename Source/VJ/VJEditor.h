#pragma once
#include <JuceHeader.h>
#include "VJTransitionEngine.h"
#include "VJSceneManager.h"
#include "VJControlPanel.h"
#include "VJBPMSync.h"
#include "VJShaderEditorDialog.h"
#include "VJBuiltinTransitions.h"
#include "../Audio/BPMDetector.h"

class CanvasEditor;
class AudioEngine;

class VJEditor : public juce::Component,
                 private juce::Timer
{
public:
    static constexpr int kControlPanelWidth = 280;

    class LivePreview : public juce::Component
    {
    public:
        LivePreview() = default;
        void setFrame(juce::Image img) { frame_ = std::move(img); repaint(); }
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black);
            if (frame_.isValid())
                g.drawImage(frame_, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }
        void mouseDown(const juce::MouseEvent&) override {}
        void mouseDrag(const juce::MouseEvent&) override {}
        void mouseUp(const juce::MouseEvent&) override {}
        void mouseMove(const juce::MouseEvent&) override {}
        void mouseEnter(const juce::MouseEvent&) override {}
        void mouseExit(const juce::MouseEvent&) override {}
        void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
    private:
        juce::Image frame_;
        JUCE_DECLARE_NON_COPYABLE(LivePreview)
    };

    //==========================================================================
    /// Floating window for the detached control panel.
    class PanelWindow : public juce::DocumentWindow
    {
    public:
        PanelWindow(VJControlPanel& panel, std::function<void()> onClose)
            : juce::DocumentWindow("VJ Controls",
                                   juce::Colour(0xFF1e1e1e),
                                   juce::DocumentWindow::closeButton),
              onClose_(onClose)
        {
            setUsingNativeTitleBar(false);
            setTitleBarHeight(28);
            setContentNonOwned(&panel, false);
            setResizable(true, false);
            centreWithSize(320, 600);
            setVisible(true);
            setAlwaysOnTop(true);
        }
        void closeButtonPressed() override { if (onClose_) onClose_(); }
    private:
        std::function<void()> onClose_;
    };

    //==========================================================================
    VJEditor(CanvasEditor& canvasEditor, AudioEngine& audioEngine);
    ~VJEditor() override;

    void attachBPMDetector(BPMDetector& detector);
    void setActive(bool active);
    bool isActive() const noexcept { return active_; }
    void setLatencyMs(float bufferMs, float avMs);

    /// Detach the control panel to a floating window.
    void detachPanel();
    /// Reattach the floating control panel back inline.
    void reattachPanel();
    bool isPanelDetached() const noexcept { return panelDetached_; }

    /// Open / close the shader editor dialog.
    void openShaderEditor();
    void closeShaderEditor();

    std::function<void(const juce::String&)> onRestoreScene;
    std::function<void()>                    onTapBPM;
    std::function<void(float)>               onSetBPM;
    std::function<void(const juce::String&)> onInputDeviceChanged;
    std::function<void()>                    onExitVJ;

    // Fullscreen — forwarded to MainComponent
    std::function<void()>    onToggleFullscreen;
    std::function<void(int)> onSelectDisplay;

    VJSceneManager&     getSceneManager()     { return sceneManager_; }
    VJTransitionEngine& getTransitionEngine() { return transitionEngine_; }
    VJBPMSync&          getBPMSync()          { return bpmSync_; }
    VJControlPanel&     getControlPanel()     { return controlPanel_; }
    LivePreview&        getLivePreview()       { return livePreview_; }

private:
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void handleSceneLoaded(int sceneIndex);

    CanvasEditor&       canvasEditor_;
    AudioEngine&        audioEngine_;
    VJTransitionEngine  transitionEngine_;
    VJSceneManager      sceneManager_;
    VJBPMSync           bpmSync_;
    VJControlPanel      controlPanel_;
    LivePreview         livePreview_;
    bool                active_ = false;

    // Detachable panel
    bool panelDetached_ = false;
    std::unique_ptr<PanelWindow> panelWindow_;

    // Shader editor dialog
    std::unique_ptr<VJShaderEditorDialog> shaderEditorDialog_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VJEditor)
};
