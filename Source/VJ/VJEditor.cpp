#include "VJEditor.h"
#include "../Canvas/CanvasEditor.h"
#include "../Audio/AudioEngine.h"

//==============================================================================
VJEditor::VJEditor(CanvasEditor& canvasEditor, AudioEngine& audioEngine)
    : canvasEditor_     (canvasEditor)
    , audioEngine_      (audioEngine)
    , sceneManager_     (canvasEditor)
    , controlPanel_     (sceneManager_, transitionEngine_, bpmSync_)
{
    addAndMakeVisible(livePreview_);
    addAndMakeVisible(controlPanel_);

    // --- Scene callbacks from control panel ---------------------------------
    controlPanel_.onAddScene = [this]
    {
        sceneManager_.addSceneFromCurrent();
    };

    controlPanel_.onLoadScene = [this](int index)
    {
        sceneManager_.loadScene(index);
    };

    controlPanel_.onDeleteScene = [this](int index)
    {
        sceneManager_.deleteScene(index);
        bpmSync_.setSceneCount(sceneManager_.getSceneCount());
    };

    controlPanel_.onDuplicateScene = [this](int index)
    {
        sceneManager_.duplicateScene(index);
        bpmSync_.setSceneCount(sceneManager_.getSceneCount());
    };

    controlPanel_.onRenameScene = [this](int index, juce::String name)
    {
        sceneManager_.renameScene(index, name);
    };

    // TAP / SET buttons
    controlPanel_.onTapBPM = [this] { if (onTapBPM) onTapBPM(); };
    controlPanel_.onSetBPM = [this](float bpm) { if (onSetBPM) onSetBPM(bpm); };

    // Audio input device selection
    controlPanel_.onInputDeviceChanged = [this](const juce::String& name)
    {
        if (onInputDeviceChanged) onInputDeviceChanged(name);
    };

    // Mode switch — exit VJ back to 2D
    controlPanel_.onExitVJ = [this] { if (onExitVJ) onExitVJ(); };

    // Scene manager loads a scene → rebuild canvas via owner callback
    sceneManager_.onSceneLoaded = [this](int index) { handleSceneLoaded(index); };

    // BPMSync auto-switch fires a scene change
    bpmSync_.onSceneSwitchRequested = [this](int nextIndex)
    {
        if (nextIndex < sceneManager_.getSceneCount())
            sceneManager_.loadScene(nextIndex);
    };
}

VJEditor::~VJEditor()
{
    stopTimer();
    sceneManager_.onSceneLoaded = nullptr;
    bpmSync_.detach();
}

//==============================================================================
void VJEditor::attachBPMDetector(BPMDetector& detector)
{
    bpmSync_.attach(detector);
    bpmSync_.setSceneCount(juce::jmax(1, sceneManager_.getSceneCount()));
}

//==============================================================================
void VJEditor::setActive(bool active)
{
    active_ = active;

    if (active)
    {
        // Populate audio input device dropdown
        auto devices = audioEngine_.getAvailableInputDevices();
        auto setup = audioEngine_.getDeviceManager().getAudioDeviceSetup();
        controlPanel_.setInputDeviceList(devices, setup.inputDeviceName);

        startTimerHz(30);
    }
    else
    {
        stopTimer();
        bpmSync_.setAutoSwitch(false);
    }
}

//==============================================================================
void VJEditor::setLatencyMs(float bufferMs, float avMs)
{
    controlPanel_.setLatencyMs(bufferMs, avMs);
}

//==============================================================================
void VJEditor::handleSceneLoaded(int index)
{
    if (index < 0 || index >= sceneManager_.getSceneCount()) return;
    const juce::var& state = sceneManager_.getScenes()[index].canvasState;

    // Snapshot CURRENT canvas as the "from" image (no component resizing).
    juce::Image fromImg = (canvasEditor_.getWidth() > 0 && canvasEditor_.getHeight() > 0)
        ? canvasEditor_.createComponentSnapshot(canvasEditor_.getLocalBounds())
        : juce::Image(juce::Image::RGB, 1, 1, true);

    // Freeze the live preview on the "from" frame while we restore the scene
    livePreview_.setFrame(fromImg);

    // Restore the new scene (clears + loads items)
    if (onRestoreScene)
        onRestoreScene(state.toString());

    // Snapshot the NEW canvas (after restore) as the "to" image.
    juce::Image toImg = (canvasEditor_.getWidth() > 0 && canvasEditor_.getHeight() > 0)
        ? canvasEditor_.createComponentSnapshot(canvasEditor_.getLocalBounds())
        : juce::Image(juce::Image::RGB, 1, 1, true);

    // Start the transition animation — paintOverChildren composites it on top of livePreview_
    transitionEngine_.startTransition(transitionEngine_.getType(),
                                       fromImg, toImg,
                                       transitionEngine_.getDuration());
}

//==============================================================================
void VJEditor::timerCallback()
{
    const bool transitioning = transitionEngine_.tick();

    if (!transitioning)
    {
        // Snapshot the already-rendered CanvasEditor (no component resizing!).
        // CanvasEditor is live in VJ mode (positioned by MainComponent::resized),
        // so a snapshot is instant and doesn't disturb its layout.
        if (canvasEditor_.getWidth() > 0 && canvasEditor_.getHeight() > 0)
            livePreview_.setFrame(canvasEditor_.createComponentSnapshot(canvasEditor_.getLocalBounds()));
    }

    repaint();  // makes paintOverChildren fire for transition composite
}

//==============================================================================
void VJEditor::paintOverChildren(juce::Graphics& g)
{
    // Composite the transition animation on top of the LivePreview child.
    if (transitionEngine_.isActive())
    {
        const auto previewBounds = getLocalBounds().withTrimmedRight(kControlPanelWidth);
        transitionEngine_.paint(g, previewBounds);
    }
}

//==============================================================================
void VJEditor::resized()
{
    auto area = getLocalBounds();
    controlPanel_.setBounds(area.removeFromRight(kControlPanelWidth));
    livePreview_.setBounds(area);  // remaining left side = realtime render
}
