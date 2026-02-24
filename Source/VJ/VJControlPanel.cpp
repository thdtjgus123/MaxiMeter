#include "VJControlPanel.h"
#include "../UI/ThemeManager.h"

//==============================================================================
VJControlPanel::VJControlPanel(VJSceneManager&    sceneManager,
                                VJTransitionEngine& transitionEngine,
                                VJBPMSync&          bpmSync)
    : sceneManager_     (sceneManager)
    , transitionEngine_ (transitionEngine)
    , bpmSync_          (bpmSync)
{
    // Latency label
    addAndMakeVisible(latencyLabel_);
    latencyLabel_.setJustificationType(juce::Justification::centredLeft);
    latencyLabel_.setFont(juce::Font(12.f));
    latencyLabel_.setText("Buffer: -- ms   A→V: -- ms", juce::dontSendNotification);

    // ← 2D button (exit VJ mode)
    addAndMakeVisible(exitVJBtn_);
    exitVJBtn_.onClick = [this] { if (onExitVJ) onExitVJ(); };

    // Audio input device dropdown
    addAndMakeVisible(inputDeviceCombo_);
    inputDeviceCombo_.onChange = [this]
    {
        if (onInputDeviceChanged)
            onInputDeviceChanged(inputDeviceCombo_.getText());
    };

    // Add Scene
    addAndMakeVisible(addSceneBtn_);
    addSceneBtn_.onClick = [this] { if (onAddScene) onAddScene(); };

    // Transition type buttons
    for (auto& entry : transTypeBtns_)
    {
        entry.btn = std::make_unique<juce::TextButton>(entry.label);
        addAndMakeVisible(*entry.btn);
        const VJTransitionEngine::Type typeCapture = entry.type;
        entry.btn->onClick = [this, typeCapture]
        {
            transitionEngine_.setType(typeCapture);
            refreshTransitionButtons();
        };
    }

    // Duration slider
    addAndMakeVisible(durSlider_);
    durSlider_.setRange(100.0, 3000.0, 50.0);
    durSlider_.setValue(transitionEngine_.getDuration());
    durSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    durSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
    durSlider_.setTextValueSuffix(" ms");
    durSlider_.onValueChange = [this]
    {
        transitionEngine_.setDuration(static_cast<int>(durSlider_.getValue()));
    };

    addAndMakeVisible(durLabel_);
    durLabel_.setText("Duration", juce::dontSendNotification);
    durLabel_.setFont(juce::Font(11.f));

    // BPM display
    addAndMakeVisible(bpmDisplay_);
    bpmDisplay_.setFont(juce::Font(22.f, juce::Font::bold));
    bpmDisplay_.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(tapBtn_);
    tapBtn_.onClick = [this] { if (onTapBPM) onTapBPM(); };

    addAndMakeVisible(bpmInput_);
    bpmInput_.setInputRestrictions(6, "0123456789.");
    bpmInput_.setText("120");
    bpmInput_.setJustification(juce::Justification::centred);

    addAndMakeVisible(setBtn_);
    setBtn_.onClick = [this]
    {
        const float bpm = bpmInput_.getText().getFloatValue();
        if (bpm > 0.f && onSetBPM) onSetBPM(bpm);
    };
    addAndMakeVisible(clearBtn_);
    clearBtn_.setClickingTogglesState(true);
    clearBtn_.setToggleState(bpmSync_.isAutoSwitchEnabled(), juce::dontSendNotification);
    clearBtn_.onClick = [this]
    {
        bpmSync_.setAutoSwitch(clearBtn_.getToggleState());
    };

    addAndMakeVisible(beatsSlider_);
    beatsSlider_.setRange(1.0, 64.0, 1.0);
    beatsSlider_.setValue(bpmSync_.getBeatsPerSwitch());
    beatsSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    beatsSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 16);
    beatsSlider_.onValueChange = [this]
    {
        bpmSync_.setBeatsPerSwitch(static_cast<int>(beatsSlider_.getValue()));
    };

    addAndMakeVisible(beatsLabel_);
    beatsLabel_.setText("Beats/Switch", juce::dontSendNotification);
    beatsLabel_.setFont(juce::Font(11.f));

    // Hook beat pulse for visual feedback (called on message thread from VJBPMSync timer)
    bpmSync_.onBeat = [this]
    {
        beatPulse_ = true;
        repaint();
    };

    // Update scene list when manager changes
    sceneManager_.onScenesChanged = [this] { buildSceneList(); resized(); repaint(); };

    buildSceneList();
    startTimerHz(8);   // slow timer: BPM display refresh + beat-pulse decay
}

VJControlPanel::~VJControlPanel()
{
    bpmSync_.onBeat = nullptr;
    sceneManager_.onScenesChanged = nullptr;
    stopTimer();
}

//==============================================================================
void VJControlPanel::setLatencyMs(float bufferLatencyMs, float avLatencyMs)
{
    bufLatMs_ = bufferLatencyMs;
    avLatMs_  = avLatencyMs;

    latencyLabel_.setText("Buffer: "  + juce::String(bufLatMs_,  1) + " ms   "
                          "A→V: "    + juce::String(avLatMs_,   1) + " ms",
                          juce::dontSendNotification);
}

//==============================================================================
void VJControlPanel::setInputDeviceList(const juce::StringArray& devices, const juce::String& current)
{
    inputDeviceCombo_.clear(juce::dontSendNotification);
    for (int i = 0; i < devices.size(); ++i)
        inputDeviceCombo_.addItem(devices[i], i + 1);

    // Select the current device
    int idx = devices.indexOf(current);
    if (idx >= 0)
        inputDeviceCombo_.setSelectedItemIndex(idx, juce::dontSendNotification);
    else if (devices.size() > 0)
        inputDeviceCombo_.setSelectedItemIndex(0, juce::dontSendNotification);
}

//==============================================================================
void VJControlPanel::timerCallback()
{
    beatPulse_ = false;
    refreshBPMDisplay();
}

//==============================================================================
void VJControlPanel::buildSceneList()
{
    sceneRows_.clear();

    for (int i = 0; i < sceneManager_.getSceneCount(); ++i)
    {
        auto* row = sceneRows_.add(new SceneRow());

        // Thumbnail as load button
        auto thumb = sceneManager_.getScenes()[i].thumbnail;
        if (thumb.isValid())
            row->loadBtn.setImages(false, true, true,
                thumb, 1.0f, {}, thumb, 0.8f, {}, thumb, 1.0f, {});
        addAndMakeVisible(row->loadBtn);

        const int capturedIndex = i;
        row->loadBtn.onClick = [this, capturedIndex]
        {
            if (onLoadScene) onLoadScene(capturedIndex);
        };

        addAndMakeVisible(row->deleteBtn);
        row->deleteBtn.onClick = [this, capturedIndex]
        {
            if (onDeleteScene) onDeleteScene(capturedIndex);
        };

        addAndMakeVisible(row->dupBtn);
        row->dupBtn.onClick = [this, capturedIndex]
        {
            if (onDuplicateScene) onDuplicateScene(capturedIndex);
        };
    }
}

void VJControlPanel::refreshSceneList()
{
    buildSceneList();
    resized();
    repaint();
}

//==============================================================================
void VJControlPanel::refreshTransitionButtons()
{
    for (auto& entry : transTypeBtns_)
        entry.btn->setToggleState(entry.type == transitionEngine_.getType(),
                                  juce::dontSendNotification);
}

void VJControlPanel::refreshBPMDisplay()
{
    const float bpm = bpmSync_.getBPM();
    if (bpm > 0.f)
        bpmDisplay_.setText(juce::String(bpm, 1) + " BPM", juce::dontSendNotification);
    else
        bpmDisplay_.setText("-- BPM", juce::dontSendNotification);
}

//==============================================================================
int VJControlPanel::paintSectionHeader(juce::Graphics& g, const juce::String& label, int y) const
{
    const auto& pal = ThemeManager::getInstance().getPalette();
    g.setColour(pal.border);
    g.fillRect(8, y, getWidth() - 16, 1);
    g.setColour(pal.dimText);
    g.setFont(juce::Font(11.f, juce::Font::bold));
    g.drawText(label, 8, y + 4, getWidth() - 16, 14, juce::Justification::left);
    return y + 20;
}

//==============================================================================
void VJControlPanel::paint(juce::Graphics& g)
{
    const auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll(pal.panelBg);

    if (beatPulse_)
    {
        g.setColour(pal.accent.withAlpha(0.12f));
        g.fillAll();
    }

    // Draw section header dividers
    int y = 4;
    paintSectionHeader(g, "AUDIO INPUT", y); y += 20 + 26;

    paintSectionHeader(g, "LATENCY", y);   y += 20 + 22;

    const int sceneBlockH = sceneRows_.size() * 56 + 28;
    paintSectionHeader(g, "SCENES", y);    y += 20 + sceneBlockH;

    const int transBlockH = 22 * 2 + 4 + 22;  // two rows of buttons + dur slider
    paintSectionHeader(g, "TRANSITION", y); y += 20 + transBlockH;

    paintSectionHeader(g, "BPM", y);
}

//==============================================================================
void VJControlPanel::resized()
{
    const int W  = getWidth();
    const int pad = 8;
    int y = 4;

    // ← 2D button at top
    exitVJBtn_.setBounds(pad, y, 60, 22);
    y += 26;

    // ---- AUDIO INPUT --------------------------------------------------------
    y += 20;  // section header
    inputDeviceCombo_.setBounds(pad, y, W - pad * 2, 22);
    y += 26;

    // ---- LATENCY -----------------------------------------------------------
    y += 20;  // section header
    latencyLabel_.setBounds(pad, y, W - pad * 2, 18);
    y += 22;

    // ---- SCENES ------------------------------------------------------------
    y += 20;  // section header
    constexpr int rowH  = 56;
    constexpr int thumbW = 80;
    constexpr int btnW  = 28;

    for (int i = 0; i < sceneRows_.size(); ++i)
    {
        auto* row = sceneRows_[i];
        const bool isActive = (i == sceneManager_.getActiveIndex());
        row->loadBtn.setBounds  (pad,             y, thumbW,   rowH - 4);
        row->dupBtn .setBounds  (W - pad - btnW * 2 - 2, y, btnW,  rowH / 2 - 2);
        row->deleteBtn.setBounds(W - pad - btnW,  y, btnW,  rowH / 2 - 2);
        y += rowH;
        juce::ignoreUnused(isActive);
    }

    addSceneBtn_.setBounds(pad, y, W - pad * 2, 22);
    y += 28;

    // ---- TRANSITION --------------------------------------------------------
    y += 20;  // section header
    const int transBtnW = (W - pad * 2 - 6) / 4;
    const int transBtnH = 22;
    int tx = pad;
    for (int i = 0; i < static_cast<int>(transTypeBtns_.size()); ++i)
    {
        transTypeBtns_[i].btn->setBounds(tx, y, transBtnW, transBtnH);
        tx += transBtnW + 2;
        if (tx + transBtnW > W - pad)
        {
            tx = pad;
            y += transBtnH + 2;
        }
    }
    y += transBtnH + 4;

    durLabel_.setBounds(pad, y, 70, 16);
    durSlider_.setBounds(pad + 72, y, W - pad - 72 - pad, 18);
    y += 22;

    // ---- BPM ---------------------------------------------------------------
    y += 20;  // section header
    bpmDisplay_.setBounds(pad, y, W - pad * 2, 28);
    y += 32;

    const int btnW3 = (W - pad * 2 - 8) / 4;
    tapBtn_  .setBounds(pad,                  y, btnW3,     22);
    bpmInput_.setBounds(pad + btnW3 + 2,      y, btnW3 * 2, 22);
    setBtn_  .setBounds(pad + btnW3 * 3 + 6,  y, btnW3,     22);
    y += 26;

    beatsLabel_.setBounds(pad, y, 90, 16);
    beatsSlider_.setBounds(pad + 92, y, W - pad - 92 - pad, 18);
    y += 22;

    clearBtn_.setBounds(pad, y, W - pad * 2, 22);
    juce::ignoreUnused(y);
}
