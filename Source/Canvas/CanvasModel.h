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

    /// Zoom and pan so that all visible items fit inside viewBounds with padding.
    /// Pass the canvas view's current pixel bounds.
    void frameToAll(juce::Rectangle<int> viewBounds, float padding = 40.0f);

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

    //-- Grouping ------------------------------------------------------------
    /// Assign a shared groupId to all currently selected items.
    void groupSelection();
    /// Clear groupId on all currently selected items.
    void ungroupSelection();
    /// Return all items that share the given groupId (non-null).
    std::vector<CanvasItem*> getGroupMembers(const juce::Uuid& gid);
    /// Select all items that belong to the same group as `id`.
    void selectGroup(const juce::Uuid& itemId, bool addToSelection = false);

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

    // Clipboard buffer — carries every visual property so paste is a full clone.
    struct ClipItem
    {
        // Basic geometry
        MeterType    type         = MeterType::MultiBandAnalyzer;
        float        relX = 0, relY = 0, w = 200, h = 200;
        int          rot          = 0;
        juce::String name;
        int          clipGroupTag = -1;
        // Common visual
        float        opacity      = 1.0f;
        juce::Colour itemBackground  { 0x00000000 };
        juce::Colour meterBgColour   { 0x00000000 };
        juce::Colour meterFgColour   { 0x00000000 };
        BlendMode    blendMode    = BlendMode::Normal;
        bool         aspectLock   = false;
        bool         locked       = false;
        bool         visible      = true;
        int          vuChannel    = 0;
        // Media
        juce::String mediaFilePath;
        juce::String customPluginId;  ///< manifest id; new instance created on paste
        // Shape
        juce::Colour fillColour1       { 0xFF3A7BFF };
        juce::Colour fillColour2       { 0xFF1A4ACA };
        int          gradientDirection = 0;
        float        cornerRadius      = 0.0f;
        juce::Colour strokeColour      { 0xFFFFFFFF };
        float        strokeWidth       = 2.0f;
        int          strokeAlignment   = 0;
        int          lineCap           = 0;
        int          starPoints        = 5;
        float        triangleRoundness = 0.0f;
        juce::String svgPathData;
        juce::String svgFilePath;
        // Loudness
        float        targetLUFS          = -14.0f;
        bool         loudnessShowHistory  = true;
        // Frosted glass
        bool         frostedGlass = false;
        float        blurRadius   = 10.0f;
        juce::Colour frostTint    { 0xFFFFFFFF };
        float        frostOpacity = 0.15f;
        // Text
        juce::String textContent  { "Text" };
        juce::String fontFamily   { "Arial" };
        float        fontSize     = 24.0f;
        bool         fontBold     = false;
        bool         fontItalic   = false;
        juce::Colour textColour   { 0xFFFFFFFF };
        int          textAlignment = 0;
    };
    std::vector<ClipItem> clipboard;

    int nextZOrder = 0;
};
