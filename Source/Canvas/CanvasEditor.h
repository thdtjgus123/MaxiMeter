#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"
#include "CanvasView.h"
#include "CanvasToolbox.h"
#include "CanvasPropertyPanel.h"
#include "MeterSettingsPanel.h"
#include "LayerPanel.h"
#include "MiniMap.h"
#include "AlignmentToolbar.h"
#include "MeterFactory.h"
#include "../Export/ExportSettings.h"

//==============================================================================
/// Top-level canvas editor component.  Owns the model, view, toolbox, property
/// panel, meter settings panel, layer panel, minimap, alignment toolbar, and meter factory.
/// Replaces the old tabbed-meter layout in MainComponent.
/// Stage 7: Implements DragAndDropTarget to receive meters dragged from toolbox.
class CanvasEditor : public juce::Component,
                     public juce::DragAndDropTarget,
                     public CanvasView::Listener
{
public:
    CanvasEditor(AudioEngine& audioEngine,
                 FFTProcessor& fftProcessor,
                 LevelAnalyzer& levelAnalyzer,
                 LoudnessAnalyzer& loudnessAnalyzer,
                 StereoFieldAnalyzer& stereoAnalyzer);
    ~CanvasEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /// Called 60 fps from the main timer — feeds audio data into every meter.
    void timerTick();

    /// Add a meter of the given type at the given canvas position.
    CanvasItem* addMeter(MeterType type, juce::Point<float> canvasPos = {});

    /// Add a custom Python plugin component at the given canvas position.
    /// Handles PythonPluginBridge startup, manifest lookup, and instance creation.
    CanvasItem* addCustomPluginAt(const juce::String& pluginName, juce::Point<float> canvasPos);

    /// Apply skin to all skinnable items.
    void applySkinToAll(const Skin::SkinModel* skin);

    /// Reset meters on new file load.
    void onFileLoaded(double sampleRate);

    /// Access model (for external wiring if needed).
    CanvasModel& getModel() { return model; }

    /// Access canvas view (for external wiring if needed).
    CanvasView& getCanvasView() { return canvasView; }

    /// Access meter settings panel (for external callback wiring).
    MeterSettingsPanel& getMeterSettings() { return meterSettings; }

    /// Re-apply theme colours to all sub-panels (alignment, properties, layers, settings).
    void applyThemeToAllPanels();

    /// Access meter factory (for external callback wiring).
    MeterFactory& getMeterFactory() { return meterFactory; }

    /// Paste clipboard items at canvas position, creating visual components.
    void pasteItems(juce::Point<float> canvasPos);

    /// Duplicate selected items, creating visual components for the copies.
    void duplicateItems();

    /// Ensure all items have visual components (creates missing ones).
    void ensureComponents();

    /// Show or hide the MiniMap overlay.
    void setMiniMapVisible(bool visible) { miniMap.setVisible(visible); resized(); }

    /// Freeze the viewport and show a grey overlay during export.
    void setExportOverlay(bool show);
    bool isExportOverlayActive() const { return exportOverlay_; }

    /// Render a single preview frame at the given resolution using the live
    /// component state.  Returns a software-backed Image.
    juce::Image renderPreviewFrame(int videoW, int videoH);

    /// Show a popup window displaying the render preview.
    void showRenderPreview();

    /// Set the toolbox view mode (1=List, 2=Grid, 3=Compact).
    void setToolboxViewMode(int mode)
    {
        using VM = CanvasToolbox::ViewMode;
        if (mode == 2)      toolbox.setViewMode(VM::Grid);
        else if (mode == 3) toolbox.setViewMode(VM::Compact);
        else                toolbox.setViewMode(VM::List);
    }

    // CanvasView::Listener
    void canvasItemDoubleClicked(CanvasItem* item) override;
    void canvasContextMenu(CanvasItem* item, juce::Point<int> screenPos) override;
    void exitAllInteractiveModes() override;
    void groupSelection() override;
    void ungroupSelection() override;

    // DragAndDropTarget — receive meters dragged from toolbox
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

private:
    CanvasModel          model;
    CanvasView           canvasView;
    CanvasToolbox        toolbox;
    CanvasPropertyPanel  propertyPanel;
    MeterSettingsPanel   meterSettings;
    LayerPanel           layerPanel;
    MiniMap              miniMap;
    AlignmentToolbar     alignToolbar;
    MeterFactory         meterFactory;

    // Splitter positions
    static constexpr int kToolboxWidth    = 180;
    static constexpr int kAlignBarHeight  = 30;
    static constexpr int kMiniMapSize     = 150;
    static constexpr int kRightDividerW   = 5;    ///< width of the vertical resize grip

    int   rightPanelWidth_  = 240;  ///< width of the right properties column (draggable)
    float propPanelRatio_   = 0.4f; ///< fraction of right panel height used by property+settings panels

    //-- Vertical resize grip between canvas and right panel ----------------
    struct PanelEdgeDivider : juce::Component
    {
        std::function<void(int deltaX)> onDragged;
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
    private:
        int dragStartX_ = 0;
    };
    PanelEdgeDivider rightEdgeDivider_;

    /// Draggable divider ratio — fraction of right panel used by layer panel (bottom)
    float layerPanelRatio_ = 0.35f;

    /// Export overlay state
    bool exportOverlay_ = false;

    void showContextMenu(CanvasItem* item, juce::Point<int> screenPos);

    /// Toggle interactive mode for an item (enables/disables mouse passthrough).
    void setItemInteractiveMode(CanvasItem* item, bool interactive);
};
