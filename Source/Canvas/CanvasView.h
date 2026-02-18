#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Interactive canvas surface — renders items, handles zoom/pan, selection
/// rectangles, resize/rotate handles, grid, smart guides, and drag interactions.
class CanvasView : public juce::Component,
                   public CanvasModelListener
{
public:
    explicit CanvasView(CanvasModel& model);
    ~CanvasView() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // Mouse interaction
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // Keyboard
    bool keyPressed(const juce::KeyPress& key) override;

    // CanvasModelListener
    void itemsChanged() override       { updateChildBounds(); repaint(); }
    void selectionChanged() override   { repaint(); }
    void zoomPanChanged() override     { updateChildBounds(); repaint(); }
    void backgroundChanged() override  { repaint(); }

    /// Re-position all child components based on the current zoom/pan and item geometry.
    void updateChildBounds();

    /// Listener for double-click on item (opens property panel).
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void canvasItemDoubleClicked(CanvasItem* item) {}
        virtual void canvasContextMenu(CanvasItem* item, juce::Point<int> screenPos) {}
        virtual void exitAllInteractiveModes() {}
        virtual void groupSelection() {}
        virtual void ungroupSelection() {}
    };
    void addListener(Listener* l)    { viewListeners.add(l); }
    void removeListener(Listener* l) { viewListeners.remove(l); }

    //-- FPS overlay & performance safe-mode ---------------------------------
    /// Current measured FPS (updated every frame).
    float getCurrentFps() const { return currentFps_; }

    /// True when components have been replaced with placeholder outlines.
    bool isInPlaceholderMode() const { return placeholderMode_; }

    /// Manually restore full rendering after placeholder mode was triggered.
    void restoreFromPlaceholderMode();

    /// Call once per frame (from CanvasEditor::timerTick) to update FPS counter.
    void tickFps();

    void setFpsThreshold(float newThreshold) { fpsThreshold_ = newThreshold; }
    float getFpsThreshold() const { return fpsThreshold_; }

private:
    CanvasModel& model;

    // Interaction state machine
    enum class DragMode { None, Pan, MoveItems, SelectRect, Resize };
    DragMode dragMode = DragMode::None;

    juce::Point<float> dragStart;          // Screen space
    juce::Point<float> dragLast;
    juce::Rectangle<float> selectionRect;  // Screen space

    // Resize handle
    enum class HandlePos { None, TopLeft, TopRight, BottomLeft, BottomRight,
                           Top, Bottom, Left, Right };
    HandlePos activeHandle = HandlePos::None;
    juce::Uuid resizeItemId;
    juce::Rectangle<float> resizeOrigBounds;

    // Smart guide state (for painting)
    GuideInfo activeHGuide, activeVGuide;

    // Drawing helpers
    void drawGrid(juce::Graphics& g);
    void drawSmartGuides(juce::Graphics& g);
    void drawSelectionRect(juce::Graphics& g);
    void drawItemHandles(juce::Graphics& g, const CanvasItem& item);
    void drawRulers(juce::Graphics& g);

    /// Draw Center/Outside strokes for shape items on top of all components.
    void drawShapeStrokeOverlay(juce::Graphics& g);

    HandlePos hitTestHandle(const CanvasItem& item, juce::Point<float> screenPt) const;
    juce::Rectangle<float> getHandleRect(juce::Rectangle<float> screenBounds, HandlePos hp) const;
    juce::MouseCursor cursorForHandle(HandlePos hp) const;

    float getHandleSize() const;  // reads from AppSettings

    juce::ListenerList<Listener> viewListeners;

    //-- FPS tracking ----------------------------------------------------------
    int       frameCount_    = 0;
    double    fpsAccumulator_ = 0.0;    // seconds accumulated
    float     currentFps_    = 60.0f;
    juce::int64 lastFrameTimeMs_ = 0;

    //-- Placeholder / performance safe-mode -----------------------------------
    bool      placeholderMode_ = false;
    int       lowFpsFrames_    = 0;     // consecutive frames below threshold
    float     fpsThreshold_    = 20.0f; // Mutable threshold (default 20)
    static constexpr int   kLowFpsFramesBeforePlaceholder = 4; // ~4 ticks

    void drawFpsOverlay(juce::Graphics& g);
    void drawPlaceholderItems(juce::Graphics& g);
    void enterPlaceholderMode();
};
