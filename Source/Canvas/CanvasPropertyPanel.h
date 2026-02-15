#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Right-side panel showing editable properties for selected canvas item(s).
/// Wraps content in a juce::Viewport for scrolling.
class CanvasPropertyPanel : public juce::Component,
                            public CanvasModelListener
{
public:
    explicit CanvasPropertyPanel(CanvasModel& model);
    ~CanvasPropertyPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // CanvasModelListener
    void selectionChanged() override { refresh(); }
    void itemsChanged() override    { refresh(); }

    void refresh();
    void applyThemeColours();

private:
    CanvasModel& model;

    // ── Scrollable viewport ──
    juce::Viewport viewport;

    /// Inner component that holds all the controls.
    class ContentComp : public juce::Component {
    public: void paint(juce::Graphics&) override {} };
    ContentComp content;

    // Property editors (children of content)
    juce::Label titleLabel;
    juce::Label nameLabel;       juce::TextEditor nameEditor;
    juce::Label xLabel;          juce::TextEditor xEditor;
    juce::Label yLabel;          juce::TextEditor yEditor;
    juce::Label wLabel;          juce::TextEditor wEditor;
    juce::Label hLabel;          juce::TextEditor hEditor;
    juce::Label rotLabel;        juce::ComboBox   rotCombo;
    juce::ToggleButton lockToggle     { "Locked" };
    juce::ToggleButton visibleToggle  { "Visible" };
    juce::ToggleButton aspectToggle   { "Lock Aspect" };
    juce::Label        opacityLabel;
    juce::Slider       opacitySlider;

    // Background color
    juce::Label        bgColourLabel;
    juce::TextButton   bgColourButton  { "" };

    // Grid color
    juce::Label        gridColourLabel;
    juce::TextButton   gridColourButton { "" };

    // Per-item background
    juce::Label        itemBgColourLabel;
    juce::TextButton   itemBgColourButton { "" };

    // ── Meter colour overrides ──
    juce::Label        meterBgLabel;
    juce::TextButton   meterBgButton { "" };
    juce::Label        meterFgLabel;
    juce::TextButton   meterFgButton { "" };

    // ── Blend mode ──
    juce::Label        blendLabel;
    juce::ComboBox     blendCombo;

    void applyEdits();
    void setupRow(juce::Label& lbl, const juce::String& text, juce::Component& editor);
    void launchColourPicker(juce::TextButton& btn, juce::Colour initial,
                            std::function<void(juce::Colour)> onChange);
    void layoutContent();   ///< Lay out all controls inside content

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanvasPropertyPanel)
};
