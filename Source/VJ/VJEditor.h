#pragma once
#include <JuceHeader.h>
#include "VJTransitionEngine.h"
#include "VJSceneManager.h"
#include "VJControlPanel.h"
#include "VJBPMSync.h"
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

    VJEditor(CanvasEditor& canvasEditor, AudioEngine& audioEngine);
    ~VJEditor() override;

    void attachBPMDetector(BPMDetector& detector);
    void setActive(bool active);
    bool isActive() const noexcept { return active_; }
    void setLatencyMs(float bufferMs, float avMs);

    std::function<void(const juce::String&)> onRestoreScene;
    std::function<void()>                    onTapBPM;
    std::function<void(float)>               onSetBPM;
    std::function<void(const juce::String&)> onInputDeviceChanged;
    std::function<void()>                    onExitVJ;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VJEditor)
};
