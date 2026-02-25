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

    // Fullscreen / display / popout / shader editor
    controlPanel_.onToggleFullscreen = [this] { if (onToggleFullscreen) onToggleFullscreen(); };
    controlPanel_.onSelectDisplay    = [this](int idx) { if (onSelectDisplay) onSelectDisplay(idx); };
    controlPanel_.onDetachPanel      = [this] { if (panelDetached_) reattachPanel(); else detachPanel(); };
    controlPanel_.onOpenShaderEditor = [this] { openShaderEditor(); };

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
    shaderEditorDialog_.reset();
    panelWindow_.reset();
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
        reattachPanel();
        closeShaderEditor();
    }
}

//==============================================================================
void VJEditor::detachPanel()
{
    if (panelDetached_) return;
    panelDetached_ = true;

    removeChildComponent(&controlPanel_);
    panelWindow_ = std::make_unique<PanelWindow>(controlPanel_, [this] { reattachPanel(); });
    controlPanel_.setBounds(0, 0, kControlPanelWidth, 600);
    resized();
}

void VJEditor::reattachPanel()
{
    if (!panelDetached_) return;
    panelDetached_ = false;

    panelWindow_.reset();
    addAndMakeVisible(controlPanel_);
    resized();
}

//==============================================================================
void VJEditor::openShaderEditor()
{
    if (!shaderEditorDialog_)
    {
        shaderEditorDialog_ = std::make_unique<VJShaderEditorDialog>();

        auto& editor = shaderEditorDialog_->getEditor();

        // Populate presets
        auto builtins = VJBuiltinTransitions::getAll();
        juce::StringArray names;
        for (auto& p : builtins) names.add(p.name);
        editor.setPresets(names);

        // If a GLSL transition is already loaded, show it
        auto& src = transitionEngine_.getGLSLSource();
        if (src.isNotEmpty())
            editor.setSource(src);
        else if (!builtins.empty())
            editor.setSource(builtins[0].source);

        // Apply callback — compile & set the GLSL source
        editor.onApply = [this](const juce::String& source)
        {
            transitionEngine_.setCustomGLSL(source);
            auto* glsl = transitionEngine_.getGLSLTransition();
            if (glsl && !glsl->isValid())
            {
                if (shaderEditorDialog_)
                    shaderEditorDialog_->getEditor().setError(glsl->getLastError());
            }
            else
            {
                if (shaderEditorDialog_)
                    shaderEditorDialog_->getEditor().setError("");
                // Auto-select CustomGLSL type
                transitionEngine_.setType(VJTransitionEngine::Type::CustomGLSL);
            }
        };

        // Load preset callback
        editor.onLoadPreset = [this, builtins](int index)
        {
            if (index >= 0 && index < static_cast<int>(builtins.size()))
            {
                if (shaderEditorDialog_)
                    shaderEditorDialog_->getEditor().setSource(builtins[index].source);
            }
        };

        // Open .glsl file
        editor.onOpenFile = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Open GLSL Transition", juce::File{}, "*.glsl");
            juce::Component::SafePointer<VJEditor> safeThis(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                [safeThis, chooser](const juce::FileChooser& fc)
                {
                    if (safeThis == nullptr) return;
                    auto result = fc.getResult();
                    if (result.existsAsFile() && safeThis->shaderEditorDialog_)
                        safeThis->shaderEditorDialog_->getEditor().setSource(result.loadFileAsString());
                });
        };

        // Save .glsl file
        editor.onSaveFile = [this](const juce::String& source)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Save GLSL Transition", juce::File{}, "*.glsl");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                [source, chooser](const juce::FileChooser& fc)
                {
                    auto result = fc.getResult();
                    if (result != juce::File{})
                        result.replaceWithText(source);
                });
        };
    }

    shaderEditorDialog_->setVisible(true);
    shaderEditorDialog_->toFront(true);
}

void VJEditor::closeShaderEditor()
{
    shaderEditorDialog_.reset();
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

    // Snapshot only the CanvasView child — not the entire CanvasEditor which
    // includes the toolbox, property panel, alignment toolbar, etc.
    auto& cv = canvasEditor_.getCanvasView();

    juce::Image fromImg = (cv.getWidth() > 0 && cv.getHeight() > 0)
        ? cv.createComponentSnapshot(cv.getLocalBounds())
        : juce::Image(juce::Image::RGB, 1, 1, true);

    // Freeze the live preview on the "from" frame while we restore the scene
    livePreview_.setFrame(fromImg);

    // Restore the new scene (clears + loads items)
    if (onRestoreScene)
        onRestoreScene(state.toString());

    // Snapshot the NEW canvas (after restore) as the "to" image.
    juce::Image toImg = (cv.getWidth() > 0 && cv.getHeight() > 0)
        ? cv.createComponentSnapshot(cv.getLocalBounds())
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
        // Snapshot only the CanvasView child (the actual meter viewport) — not
        // the entire CanvasEditor with its toolbox / property panel / toolbar.
        auto& cv = canvasEditor_.getCanvasView();
        if (cv.getWidth() > 0 && cv.getHeight() > 0)
            livePreview_.setFrame(cv.createComponentSnapshot(cv.getLocalBounds()));
    }

    // During a transition, repaint the full VJEditor (paintOverChildren
    // composites the transition overlay).  Otherwise, only repaint the
    // live-preview area to avoid flickering the control panel at 30 Hz.
    if (transitioning)
        repaint();
    else
        livePreview_.repaint();
}

//==============================================================================
void VJEditor::paintOverChildren(juce::Graphics& g)
{
    // Composite the transition animation on top of the LivePreview child.
    if (transitionEngine_.isActive())
    {
        auto previewBounds = panelDetached_
            ? getLocalBounds()
            : getLocalBounds().withTrimmedRight(kControlPanelWidth);
        transitionEngine_.paint(g, previewBounds);
    }
}

//==============================================================================
void VJEditor::resized()
{
    auto area = getLocalBounds();
    if (!panelDetached_)
    {
        controlPanel_.setBounds(area.removeFromRight(kControlPanelWidth));
    }
    livePreview_.setBounds(area);  // remaining left side (or full width if panel detached)
}
