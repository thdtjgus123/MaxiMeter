#pragma once

#include <JuceHeader.h>
#include "VJSceneManager.h"
#include "VJTransitionEngine.h"
#include "VJBPMSync.h"

//==============================================================================
/// Right-side VJ control panel (320 px wide).
/// Sections: Latency · Scenes · Transition · BPM
class VJControlPanel : public juce::Component,
                       private juce::Timer
{
public:
    explicit VJControlPanel(VJSceneManager&    sceneManager,
                            VJTransitionEngine& transitionEngine,
                            VJBPMSync&          bpmSync);
    ~VJControlPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==========================================================================
    // Latency display (set from AudioEngine each repaint cycle)
    void setLatencyMs(float bufferLatencyMs, float avLatencyMs);

    //==========================================================================
    // Callbacks wired by VJEditor
    std::function<void()>                     onAddScene;
    std::function<void(int index)>            onLoadScene;
    std::function<void(int index)>            onDeleteScene;
    std::function<void(int index)>            onDuplicateScene;
    std::function<void(int index, juce::String name)> onRenameScene;

    // BPM
    std::function<void()>        onTapBPM;
    std::function<void(float)>   onSetBPM;

    // Audio input device selection
    std::function<void(const juce::String&)> onInputDeviceChanged;

    // Mode switch
    std::function<void()> onExitVJ;

    /// Populate the audio-input dropdown from a list of device names.
    void setInputDeviceList(const juce::StringArray& devices, const juce::String& current);

private:
    void timerCallback() override;
    void buildSceneList();
    void refreshSceneList();
    void refreshTransitionButtons();
    void refreshBPMDisplay();

    // Section helpers
    int paintSectionHeader(juce::Graphics& g, const juce::String& label, int y) const;

    //==========================================================================
    VJSceneManager&     sceneManager_;
    VJTransitionEngine& transitionEngine_;
    VJBPMSync&          bpmSync_;

    // Mode switch
    juce::TextButton exitVJBtn_  { juce::CharPointer_UTF8("\xe2\x86\x90 2D") };

    // Audio input
    juce::ComboBox inputDeviceCombo_;

    // Latency
    juce::Label  latencyLabel_;
    float        bufLatMs_ = 0.f;
    float        avLatMs_  = 0.f;

    // Scenes
    struct SceneRow
    {
        juce::ImageButton  loadBtn;
        juce::TextButton   deleteBtn  { "x" };
        juce::TextButton   dupBtn     { "+" };
    };
    juce::OwnedArray<SceneRow> sceneRows_;
    juce::TextButton addSceneBtn_     { "+ Add Scene" };

    // Transition
    struct TransTypeBtn
    {
        VJTransitionEngine::Type type;
        juce::String             label;
        std::unique_ptr<juce::TextButton> btn;
    };
    std::array<TransTypeBtn, 7> transTypeBtns_ =
    {{
        { VJTransitionEngine::Type::Cut,        "Cut",     nullptr },
        { VJTransitionEngine::Type::Crossfade,  "Fade",    nullptr },
        { VJTransitionEngine::Type::WipeLeft,   "← Wipe",  nullptr },
        { VJTransitionEngine::Type::WipeRight,  "→ Wipe",  nullptr },
        { VJTransitionEngine::Type::WipeUp,     "↑ Wipe",  nullptr },
        { VJTransitionEngine::Type::WipeDown,   "↓ Wipe",  nullptr },
        { VJTransitionEngine::Type::ZoomBlend,  "Zoom",    nullptr },
    }};

    juce::Slider durSlider_;
    juce::Label  durLabel_;

    // BPM
    juce::Label  bpmDisplay_;
    juce::TextButton tapBtn_     { "TAP" };
    juce::TextEditor bpmInput_;
    juce::TextButton setBtn_     { "SET" };
    juce::TextButton clearBtn_   { "AUTO" };
    juce::Slider beatsSlider_;
    juce::Label  beatsLabel_;
    bool         beatPulse_      = false;

    static constexpr int kPanelWidth = 320;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VJControlPanel)
};
