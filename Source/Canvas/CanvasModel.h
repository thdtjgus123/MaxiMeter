#pragma once

#include <JuceHeader.h>
#include "CanvasItem.h"
#include <vector>
#include <set>

//==============================================================================
/// Background mode for the canvas.
enum class BackgroundMode { SolidColour, LinearGradient, RadialGradient, Image };

struct CanvasBackground
{
    BackgroundMode mode      = BackgroundMode::SolidColour;
    juce::Colour   colour1   = juce::Colour(0xFF16213E);
    juce::Colour   colour2   = juce::Colour(0xFF0A0A1A);
    float          angle     = 0.0f;        // linear gradient angle (degrees)
    juce::File     imageFile;
    enum class FitMode { Fit, Fill, Tile, Stretch };
    FitMode        fitMode   = FitMode::Fill;
    juce::Image    cachedImage;

    void paint(juce::Graphics& g, juce::Rectangle<float> area) const;
};

//==============================================================================
/// Grid / snap settings.
struct GridSettings
{
    bool  enabled    = true;
    int   spacing    = 10;
    bool  showGrid   = true;
    bool  showRuler  = true;
    bool  smartGuides = true;
    juce::Colour gridColour { 0x22FFFFFF }; ///< Grid line colour
};

//==============================================================================
/// Smart-guide result for a single axis.
struct GuideInfo
{
    bool  active = false;
    float position = 0.0f;   ///< Canvas-space X or Y for the guide line
};

//==============================================================================
/// Observer interface for canvas model changes.
class CanvasModelListener
{
public:
    virtual ~CanvasModelListener() = default;

    virtual void itemsChanged()       {}
    virtual void selectionChanged()   {}
    virtual void zoomPanChanged()     {}
    virtual void backgroundChanged()  {}
};

//==============================================================================
/// Central data model for the canvas editor.  Owns all CanvasItems,
/// selection state, zoom/pan, grid, background, and undo manager.
class CanvasModel
{
public:
    CanvasModel();

    //-- Items ---------------------------------------------------------------
    CanvasItem* addItem(std::unique_ptr<CanvasItem> item);
    void removeItem(const juce::Uuid& id);
    CanvasItem* findItem(const juce::Uuid& id);
    const CanvasItem* findItem(const juce::Uuid& id) const;
    int getNumItems() const { return static_cast<int>(items.size()); }
    CanvasItem* getItem(int index) { return items[static_cast<size_t>(index)].get(); }
    const CanvasItem* getItem(int index) const { return items[static_cast<size_t>(index)].get(); }

    /// Re-sort items by z-order (call after z-order changes).
    void sortByZOrder();

    //-- Selection -----------------------------------------------------------
    void selectItem(const juce::Uuid& id, bool addToSelection = false);
    void deselectItem(const juce::Uuid& id);
    void clearSelection();
    void selectAll();
    bool isSelected(const juce::Uuid& id) const;
    int  getNumSelected() const { return static_cast<int>(selection.size()); }
    std::vector<CanvasItem*> getSelectedItems();
    std::vector<const CanvasItem*> getSelectedItems() const;

    /// Hit-test: find topmost item that contains canvas-space point.
    CanvasItem* hitTest(juce::Point<float> canvasPt);

    //-- Zoom / Pan ----------------------------------------------------------
    float zoom         = 1.0f;          // 0.1 .. 5.0
    float panX         = 0.0f;
    float panY         = 0.0f;

    void setZoom(float z, juce::Point<float> pivot = {});
    juce::Point<float> screenToCanvas(juce::Point<float> screen) const;
    juce::Point<float> canvasToScreen(juce::Point<float> canvas) const;
    juce::Rectangle<float> screenToCanvas(juce::Rectangle<float> r) const;
    juce::Rectangle<float> canvasToScreen(juce::Rectangle<float> r) const;

    //-- Grid & Snap ---------------------------------------------------------
    GridSettings grid;
    float snapToGrid(float v) const;
    juce::Point<float> snapToGrid(juce::Point<float> p) const;

    //-- Smart Guides --------------------------------------------------------
    struct SmartGuideResult { GuideInfo hGuide, vGuide; };
    SmartGuideResult computeSmartGuides(const juce::Rectangle<float>& movingRect,
                                        const juce::Uuid& excludeId) const;

    //-- Background ----------------------------------------------------------
    CanvasBackground background;

    //-- Undo / Redo ---------------------------------------------------------
    juce::UndoManager undoManager;

    //-- Alignment helpers ---------------------------------------------------
    enum class AlignEdge { Left, CenterH, Right, Top, CenterV, Bottom };
    void alignSelection(AlignEdge edge);
    void distributeSelectionH();
    void distributeSelectionV();

    //-- Clipboard (simple state, non-serialized) ----------------------------
    void copySelection();
    void paste(juce::Point<float> at);
    void duplicateSelection();

    //-- Listeners -----------------------------------------------------------
    void addListener(CanvasModelListener* l)     { listeners.add(l); }
    void removeListener(CanvasModelListener* l)  { listeners.remove(l); }

    void notifyItemsChanged()      { listeners.call(&CanvasModelListener::itemsChanged); }
    void notifySelectionChanged()   { listeners.call(&CanvasModelListener::selectionChanged); }
    void notifyZoomPanChanged()     { listeners.call(&CanvasModelListener::zoomPanChanged); }
    void notifyBackgroundChanged()  { listeners.call(&CanvasModelListener::backgroundChanged); }

private:
    std::vector<std::unique_ptr<CanvasItem>> items;
    std::set<juce::Uuid>                     selection;
    juce::ListenerList<CanvasModelListener>  listeners;

    // Clipboard buffer (stored as lightweight descriptors)
    struct ClipItem { MeterType type; float relX, relY, w, h; int rot; juce::String name; };
    std::vector<ClipItem> clipboard;

    int nextZOrder = 0;
};
