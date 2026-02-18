#include "CanvasView.h"
#include "CanvasCommands.h"
#include "../UI/ThemeManager.h"
#include "../UI/ShapeComponent.h"
#include "CustomPluginComponent.h"
#include "../Project/AppSettings.h"
#include <cmath>

//==============================================================================
CanvasView::CanvasView(CanvasModel& m) : model(m)
{
    model.addListener(this);
    setWantsKeyboardFocus(true);
}

CanvasView::~CanvasView()
{
    model.removeListener(this);
}

//==============================================================================
void CanvasView::updateChildBounds()
{
    // Update JUCE child component z-order to match model's zOrder.
    // Items are already sorted by zOrder (low = back, high = front).
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item->component) continue;

        // Ensure JUCE child stacking matches z-order
        item->component->toFront(false);

        auto canvasRect = item->getBounds();
        auto screenRect = model.canvasToScreen(canvasRect);

        // CustomPluginComponent: keep bounds at canvas-space size and use
        // setTransform for zoom + position so that resized() reports the
        // logical (unzoomed) dimensions to the Python bridge.
        const bool isCustomPlugin = (item->meterType == MeterType::CustomPlugin);

        if (isCustomPlugin)
        {
            int cw = static_cast<int>(canvasRect.getWidth());
            int ch = static_cast<int>(canvasRect.getHeight());
            item->component->setBounds(0, 0, std::max(1, cw), std::max(1, ch));

            auto transform = juce::AffineTransform::scale(model.zoom)
                                 .translated(screenRect.getX(), screenRect.getY());

            if (item->rotation != 0)
            {
                auto centre = screenRect.getCentre();
                float rad = static_cast<float>(item->rotation)
                            * juce::MathConstants<float>::pi / 180.0f;
                transform = transform.followedBy(
                    juce::AffineTransform::rotation(rad, centre.x, centre.y));
            }

            item->component->setTransform(transform);
        }
        else
        {
            item->component->setBounds(screenRect.toNearestInt());

            // Apply rotation via AffineTransform
            if (item->rotation != 0)
            {
                auto centre = screenRect.getCentre();
                float rad = static_cast<float>(item->rotation)
                            * juce::MathConstants<float>::pi / 180.0f;
                item->component->setTransform(
                    juce::AffineTransform::rotation(rad, centre.x, centre.y));
            }
            else
            {
                item->component->setTransform({});
            }
        }

        // Hide actual components if we are in performance placeholder mode
        if (placeholderMode_)
            item->component->setVisible(false);
        else
            item->component->setVisible(item->visible);

        // Apply opacity
        item->component->setAlpha(item->opacity);
    }
}

//==============================================================================
void CanvasView::paint(juce::Graphics& g)
{
    // 1. Background
    auto area = getLocalBounds().toFloat();
    model.background.paint(g, area);

    // 2. Grid
    if (model.grid.showGrid)
        drawGrid(g);

    // 2b. Per-item backgrounds (behind the components)
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item->visible) continue;
        if (item->itemBackground.getAlpha() > 0)
        {
            auto screenRect = model.canvasToScreen(item->getBounds());
            g.setColour(item->itemBackground);
            g.fillRect(screenRect);
        }
    }

    // 3. Smart guides (while dragging)
    drawSmartGuides(g);

    // 4. Selection rectangle
    drawSelectionRect(g);

    // 5. Selection handles for each selected item
    for (auto* item : model.getSelectedItems())
        drawItemHandles(g, *item);

    // 5b. Interactive mode highlight border
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (item->interactiveMode && item->visible)
        {
            auto screenRect = model.canvasToScreen(item->getBounds());
            g.setColour(juce::Colour(0xFF00BFFF)); // cyan
            g.drawRect(screenRect.expanded(2.0f), 2.0f);
            // Draw label
            g.setFont(11.0f);
            g.drawText("Interactive (Esc to exit)", screenRect.translated(0, -16.0f).withHeight(14.0f),
                        juce::Justification::centredLeft);
        }
    }

    // 6. Placeholder outlines (when in performance safe-mode)
    if (placeholderMode_)
        drawPlaceholderItems(g);
}

//==============================================================================
void CanvasView::paintOverChildren(juce::Graphics& g)
{
    drawShapeStrokeOverlay(g);

    // Rulers and FPS overlay on top of all child components
    if (model.grid.showRuler)
        drawRulers(g);

    drawFpsOverlay(g);
}

//==============================================================================
void CanvasView::drawShapeStrokeOverlay(juce::Graphics& g)
{
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item->visible || !item->component) continue;

        // Only process shape types
        bool isShape = (item->meterType == MeterType::ShapeRectangle
                     || item->meterType == MeterType::ShapeEllipse
                     || item->meterType == MeterType::ShapeTriangle
                     || item->meterType == MeterType::ShapeLine
                     || item->meterType == MeterType::ShapeStar
                     || item->meterType == MeterType::ShapeSVG);
        if (!isShape) continue;

        auto align = static_cast<StrokeAlignment>(item->strokeAlignment);

        // Inside and Center strokes are drawn by ShapeComponent::paint() – skip.
        // Only Outside strokes need the overlay (they extend beyond the component bounds).
        if (align == StrokeAlignment::Inside) continue;
        if (align == StrokeAlignment::Center) continue;
        if (item->strokeWidth <= 0.0f) continue;

        auto* sc = dynamic_cast<ShapeComponent*>(item->component.get());
        if (!sc) continue;

        // ── Use cached path from ShapeComponent (local coords) ──
        // Transform from local component bounds to screen coordinates
        auto screenRect = model.canvasToScreen(item->getBounds());
        auto localBounds = sc->getLocalBounds().toFloat();
        if (localBounds.getWidth() <= 0.0f || localBounds.getHeight() <= 0.0f)
            continue;

        float scaleX = screenRect.getWidth()  / localBounds.getWidth();
        float scaleY = screenRect.getHeight() / localBounds.getHeight();

        juce::Path shapePath = sc->getCachedPath();
        shapePath.applyTransform(
            juce::AffineTransform::scale(scaleX, scaleY)
                .translated(screenRect.getX(), screenRect.getY()));

        // ── Apply rotation ──
        if (item->rotation != 0)
        {
            auto centre = screenRect.getCentre();
            float rad = static_cast<float>(item->rotation)
                        * juce::MathConstants<float>::pi / 180.0f;
            shapePath.applyTransform(
                juce::AffineTransform::rotation(rad, centre.x, centre.y));
        }

        // ── Apply item opacity ──
        float alpha = item->opacity;

        // ── Stroke parameters ──
        float screenStrokeW = item->strokeWidth * model.zoom;

        juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::butt;
        switch (static_cast<LineCap>(item->lineCap))
        {
            case LineCap::Butt:   cap = juce::PathStrokeType::butt;    break;
            case LineCap::Round:  cap = juce::PathStrokeType::rounded; break;
            case LineCap::Square: cap = juce::PathStrokeType::square;  break;
        }

        g.setColour(item->strokeColour.withMultipliedAlpha(alpha));

        if (align == StrokeAlignment::Center)
        {
            // Normal centered stroke
            g.strokePath(shapePath, juce::PathStrokeType(screenStrokeW,
                                                          juce::PathStrokeType::mitered, cap));
        }
        else // Outside
        {
            // Clip to outside the shape using even-odd rule, draw 2x width
            juce::Path outsideClip;
            // Use the entire CanvasView area as the outer rectangle
            outsideClip.addRectangle(getLocalBounds().toFloat());
            outsideClip.addPath(shapePath);
            outsideClip.setUsingNonZeroWinding(false);

            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(outsideClip);
            g.strokePath(shapePath, juce::PathStrokeType(screenStrokeW * 2.0f,
                                                          juce::PathStrokeType::mitered, cap));
        }
    }
}

void CanvasView::resized()
{
    updateChildBounds();
}

//==============================================================================
// Grid
//==============================================================================
void CanvasView::drawGrid(juce::Graphics& g)
{
    int spacing = model.grid.spacing;
    if (spacing < 1) return;

    float screenSpacing = spacing * model.zoom;
    if (screenSpacing < 4.0f) return; // too dense

    g.setColour(model.grid.gridColour);

    float w = static_cast<float>(getWidth());
    float h = static_cast<float>(getHeight());

    // Offset = panX mod screenSpacing
    float offX = std::fmod(model.panX, screenSpacing);
    float offY = std::fmod(model.panY, screenSpacing);
    if (offX < 0) offX += screenSpacing;
    if (offY < 0) offY += screenSpacing;

    for (float x = offX; x < w; x += screenSpacing)
        g.drawVerticalLine(static_cast<int>(x), 0.0f, h);
    for (float y = offY; y < h; y += screenSpacing)
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
}

void CanvasView::drawSmartGuides(juce::Graphics& g)
{
    if (!model.grid.smartGuides) return;

    g.setColour(ThemeManager::getInstance().getPalette().accent.withAlpha(0.8f));

    if (activeVGuide.active)
    {
        float sx = model.canvasToScreen(juce::Point<float>(activeVGuide.position, 0.0f)).x;
        g.drawVerticalLine(static_cast<int>(sx), 0.0f, static_cast<float>(getHeight()));
    }
    if (activeHGuide.active)
    {
        float sy = model.canvasToScreen(juce::Point<float>(0.0f, activeHGuide.position)).y;
        g.drawHorizontalLine(static_cast<int>(sy), 0.0f, static_cast<float>(getWidth()));
    }
}

void CanvasView::drawSelectionRect(juce::Graphics& g)
{
    if (dragMode != DragMode::SelectRect) return;
    auto& pal = ThemeManager::getInstance().getPalette();

    g.setColour(pal.selection);
    g.fillRect(selectionRect);
    g.setColour(pal.selectionOutline);
    g.drawRect(selectionRect, 1.0f);
}

void CanvasView::drawRulers(juce::Graphics& g)
{
    constexpr float rulerH        = 20.0f;
    constexpr float kMinLabelPx   = 50.0f;   // minimum screen pixels between labelled ticks
    constexpr float kMinMinorPx   = 8.0f;    // minimum screen pixels between minor (unlabelled) ticks
    auto& pal = ThemeManager::getInstance().getPalette();

    // Backgrounds
    g.setColour(pal.rulerBg.withAlpha(0.9f));
    g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), rulerH);
    g.fillRect(0.0f, 0.0f, rulerH, static_cast<float>(getHeight()));

    g.setColour(pal.rulerText);
    g.setFont(9.0f);

    // ── "Nice number" label interval ─────────────────────────────────────
    // Given the current zoom, find the smallest "nice" canvas-unit interval
    // such that adjacent labels are at least kMinLabelPx pixels apart.
    // Nice sequence: 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, …
    auto niceInterval = [](float minCanvasUnits) -> int
    {
        if (minCanvasUnits <= 1.0f) return 1;
        float mag = std::pow(10.0f, std::floor(std::log10(minCanvasUnits)));
        for (float mult : { 1.0f, 2.0f, 5.0f, 10.0f })
        {
            float candidate = mag * mult;
            if (candidate >= minCanvasUnits)
                return static_cast<int>(candidate);
        }
        return static_cast<int>(mag * 10.0f);
    };

    const float minCanvasUnits = (model.zoom > 1e-6f) ? kMinLabelPx / model.zoom : 1e9f;
    const int   labelInterval  = niceInterval(minCanvasUnits);
    const float labelScreenSpc = labelInterval * model.zoom;

    // Minor tick: subdivide major interval into 5 or 10 parts (if visible)
    int minorDivisions = 5;
    if (labelInterval / minorDivisions * model.zoom < kMinMinorPx)
        minorDivisions = 0;   // too dense — skip minor ticks entirely
    const float minorScreenSpc = (minorDivisions > 0)
                                    ? labelScreenSpc / minorDivisions
                                    : 0.0f;

    // ── Horizontal (top) ruler ────────────────────────────────────────────
    {
        float offX = std::fmod(model.panX, labelScreenSpc);
        if (offX < 0) offX += labelScreenSpc;

        // Minor ticks
        if (minorDivisions > 0)
        {
            float offMinorX = std::fmod(model.panX, minorScreenSpc);
            if (offMinorX < 0) offMinorX += minorScreenSpc;
            g.setColour(pal.rulerText.withAlpha(0.35f));
            for (float x = offMinorX; x < getWidth(); x += minorScreenSpc)
                g.drawVerticalLine(static_cast<int>(x), rulerH - 4.0f, rulerH);
        }

        // Major ticks + labels
        g.setColour(pal.rulerText);
        for (float x = offX; x < getWidth(); x += labelScreenSpc)
        {
            int canvasX = static_cast<int>(
                model.screenToCanvas(juce::Point<float>(x, 0)).x);
            g.drawVerticalLine(static_cast<int>(x), rulerH - 8.0f, rulerH);
            g.drawText(juce::String(canvasX),
                       static_cast<int>(x) - 15, 1, 30,
                       static_cast<int>(rulerH) - 4,
                       juce::Justification::centred);
        }
    }

    // ── Vertical (left) ruler ─────────────────────────────────────────────
    {
        float offY = std::fmod(model.panY, labelScreenSpc);
        if (offY < 0) offY += labelScreenSpc;

        // Minor ticks
        if (minorDivisions > 0)
        {
            float offMinorY = std::fmod(model.panY, minorScreenSpc);
            if (offMinorY < 0) offMinorY += minorScreenSpc;
            g.setColour(pal.rulerText.withAlpha(0.35f));
            for (float y = offMinorY; y < getHeight(); y += minorScreenSpc)
                g.drawHorizontalLine(static_cast<int>(y), rulerH - 4.0f, rulerH);
        }

        // Major ticks + labels
        g.setColour(pal.rulerText);
        for (float y = offY; y < getHeight(); y += labelScreenSpc)
        {
            int canvasY = static_cast<int>(
                model.screenToCanvas(juce::Point<float>(0, y)).y);
            g.drawHorizontalLine(static_cast<int>(y), rulerH - 8.0f, rulerH);
            g.saveState();
            g.addTransform(juce::AffineTransform::rotation(
                -juce::MathConstants<float>::halfPi, 10.0f, y));
            g.drawText(juce::String(canvasY),
                       -5, static_cast<int>(y) - 6, 30, 12,
                       juce::Justification::centredLeft);
            g.restoreState();
        }
    }
}

//==============================================================================
// Selection handles
//==============================================================================
void CanvasView::drawItemHandles(juce::Graphics& g, const CanvasItem& item)
{
    auto sr = model.canvasToScreen(item.getBounds());
    auto& pal = ThemeManager::getInstance().getPalette();

    // Selection outline
    g.setColour(pal.selectionOutline);
    g.drawRect(sr, 1.5f);

    if (item.locked)
    {
        // Show lock icon (just a padlock indicator)
        g.setColour(juce::Colour(0xFFFF6644));
        g.drawRect(sr, 2.0f);
        return;
    }

    // Corner handles
    auto drawHandle = [&](HandlePos hp)
    {
        auto hr = getHandleRect(sr, hp);
        g.setColour(pal.selectionOutline);
        g.fillRect(hr);
        g.setColour(pal.bodyText);
        g.drawRect(hr, 1.0f);
    };

    drawHandle(HandlePos::TopLeft);
    drawHandle(HandlePos::TopRight);
    drawHandle(HandlePos::BottomLeft);
    drawHandle(HandlePos::BottomRight);
    drawHandle(HandlePos::Top);
    drawHandle(HandlePos::Bottom);
    drawHandle(HandlePos::Left);
    drawHandle(HandlePos::Right);
}

float CanvasView::getHandleSize() const
{
    return (float)AppSettings::getInstance().getDouble(AppSettings::kHandleSize, 8.0);
}

juce::Rectangle<float> CanvasView::getHandleRect(juce::Rectangle<float> sr, HandlePos hp) const
{
    float hs = getHandleSize();
    float hh = hs * 0.5f;

    switch (hp)
    {
        case HandlePos::TopLeft:     return { sr.getX() - hh, sr.getY() - hh, hs, hs };
        case HandlePos::TopRight:    return { sr.getRight() - hh, sr.getY() - hh, hs, hs };
        case HandlePos::BottomLeft:  return { sr.getX() - hh, sr.getBottom() - hh, hs, hs };
        case HandlePos::BottomRight: return { sr.getRight() - hh, sr.getBottom() - hh, hs, hs };
        case HandlePos::Top:         return { sr.getCentreX() - hh, sr.getY() - hh, hs, hs };
        case HandlePos::Bottom:      return { sr.getCentreX() - hh, sr.getBottom() - hh, hs, hs };
        case HandlePos::Left:        return { sr.getX() - hh, sr.getCentreY() - hh, hs, hs };
        case HandlePos::Right:       return { sr.getRight() - hh, sr.getCentreY() - hh, hs, hs };
        default: return {};
    }
}

CanvasView::HandlePos CanvasView::hitTestHandle(const CanvasItem& item, juce::Point<float> screenPt) const
{
    auto sr = model.canvasToScreen(item.getBounds());
    constexpr HandlePos all[] = {
        HandlePos::TopLeft, HandlePos::TopRight,
        HandlePos::BottomLeft, HandlePos::BottomRight,
        HandlePos::Top, HandlePos::Bottom, HandlePos::Left, HandlePos::Right
    };
    for (auto hp : all)
        if (getHandleRect(sr, hp).expanded(2).contains(screenPt))
            return hp;
    return HandlePos::None;
}

juce::MouseCursor CanvasView::cursorForHandle(HandlePos hp) const
{
    switch (hp)
    {
        case HandlePos::TopLeft:
        case HandlePos::BottomRight:
            return juce::MouseCursor::TopLeftCornerResizeCursor;
        case HandlePos::TopRight:
        case HandlePos::BottomLeft:
            return juce::MouseCursor::TopRightCornerResizeCursor;
        case HandlePos::Top:
        case HandlePos::Bottom:
            return juce::MouseCursor::UpDownResizeCursor;
        case HandlePos::Left:
        case HandlePos::Right:
            return juce::MouseCursor::LeftRightResizeCursor;
        default:
            return juce::MouseCursor::NormalCursor;
    }
}

//==============================================================================
// Mouse
//==============================================================================
void CanvasView::mouseDown(const juce::MouseEvent& e)
{
    // Check if user clicked the "Restore" button while in placeholder mode
    if (placeholderMode_)
    {
        auto btnArea = juce::Rectangle<float>(8.f, 36.f, 160.f, 26.f);
        if (btnArea.contains(e.position))
        {
            restoreFromPlaceholderMode();
            return;
        }
    }

    dragStart = e.position;
    dragLast = e.position;
    activeHGuide = {};
    activeVGuide = {};

    // Right-click: context menu
    if (e.mods.isPopupMenu())
    {
        auto canvasPt = model.screenToCanvas(e.position);
        auto* item = model.hitTest(canvasPt);
        viewListeners.call(&Listener::canvasContextMenu, item,
                           e.getScreenPosition());
        return;
    }

    // Middle-click or Alt+click: pan
    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        dragMode = DragMode::Pan;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }

    auto canvasPt = model.screenToCanvas(e.position);

    // Check if clicking on a resize handle of a selected item
    for (auto* selItem : model.getSelectedItems())
    {
        if (selItem->locked) continue;
        auto hp = hitTestHandle(*selItem, e.position);
        if (hp != HandlePos::None)
        {
            dragMode = DragMode::Resize;
            activeHandle = hp;
            resizeItemId = selItem->id;
            resizeOrigBounds = selItem->getBounds();

            // Collect other selected items for proportional group resize
            resizeGroupIds_.clear();
            resizeGroupOrigBounds_.clear();
            resizeGroupOrigBBox_ = selItem->getBounds();
            for (auto* otherItem : model.getSelectedItems())
            {
                if (otherItem->id == selItem->id || otherItem->locked) continue;
                resizeGroupIds_.push_back(otherItem->id);
                resizeGroupOrigBounds_.push_back(otherItem->getBounds());
                resizeGroupOrigBBox_ = resizeGroupOrigBBox_.getUnion(otherItem->getBounds());
            }

            setMouseCursor(cursorForHandle(hp));
            return;
        }
    }

    // Hit-test items
    auto* hitItem = model.hitTest(canvasPt);

    if (hitItem != nullptr)
    {
        // If clicking on a non-interactive item, exit all interactive modes
        if (!hitItem->interactiveMode)
            viewListeners.call(&Listener::exitAllInteractiveModes);

        // Select — group-aware: clicking a grouped item selects all group members
        if (!model.isSelected(hitItem->id))
            model.selectGroup(hitItem->id, e.mods.isShiftDown());

        if (!hitItem->locked)
        {
            dragMode = DragMode::MoveItems;
        }
    }
    else
    {
        // Click on empty canvas — exit interactive modes
        viewListeners.call(&Listener::exitAllInteractiveModes);

        if (!e.mods.isShiftDown())
            model.clearSelection();
        dragMode = DragMode::SelectRect;
        selectionRect = {};
    }
}

void CanvasView::mouseDrag(const juce::MouseEvent& e)
{
    auto delta = e.position - dragLast;

    switch (dragMode)
    {
        case DragMode::Pan:
        {
            model.panX += delta.x;
            model.panY += delta.y;
            model.notifyZoomPanChanged();
            break;
        }

        case DragMode::MoveItems:
        {
            float cdx = delta.x / model.zoom;
            float cdy = delta.y / model.zoom;

            auto sel = model.getSelectedItems();
            for (auto* item : sel)
            {
                if (item->locked) continue;
                item->x += cdx;
                item->y += cdy;
            }

            // Smart guides
            if (!sel.empty() && model.grid.smartGuides)
            {
                auto* primary = sel[0];
                auto guides = model.computeSmartGuides(primary->getBounds(), primary->id);
                activeHGuide = guides.hGuide;
                activeVGuide = guides.vGuide;

                // Snap to guide
                if (guides.vGuide.active)
                {
                    float diff = guides.vGuide.position - primary->x;
                    if (std::abs(diff) < 8.0f)
                        for (auto* s : sel) if (!s->locked) s->x += diff;
                    diff = guides.vGuide.position - (primary->x + primary->width);
                    if (std::abs(diff) < 8.0f)
                        for (auto* s : sel) if (!s->locked) s->x += diff;
                    diff = guides.vGuide.position - primary->getCentre().x;
                    if (std::abs(diff) < 8.0f)
                        for (auto* s : sel) if (!s->locked) s->x += diff;
                }
                if (guides.hGuide.active)
                {
                    float diff = guides.hGuide.position - primary->y;
                    if (std::abs(diff) < 8.0f)
                        for (auto* s : sel) if (!s->locked) s->y += diff;
                    diff = guides.hGuide.position - (primary->y + primary->height);
                    if (std::abs(diff) < 8.0f)
                        for (auto* s : sel) if (!s->locked) s->y += diff;
                    diff = guides.hGuide.position - primary->getCentre().y;
                    if (std::abs(diff) < 8.0f)
                        for (auto* s : sel) if (!s->locked) s->y += diff;
                }
            }
            else if (model.grid.enabled)
            {
                // Grid snap
                for (auto* item : sel)
                    if (!item->locked)
                    {
                        item->x = model.snapToGrid(item->x);
                        item->y = model.snapToGrid(item->y);
                    }
            }

            model.notifyItemsChanged();
            break;
        }

        case DragMode::SelectRect:
        {
            selectionRect = juce::Rectangle<float>(dragStart, e.position);
            // Select items that overlap the rect
            auto canvasRect = model.screenToCanvas(selectionRect);
            model.clearSelection();
            for (int i = 0; i < model.getNumItems(); ++i)
            {
                auto* item = model.getItem(i);
                if (item->visible && item->getBounds().intersects(canvasRect))
                    model.selectItem(item->id, true);
            }
            repaint();
            break;
        }

        case DragMode::Resize:
        {
            auto* item = model.findItem(resizeItemId);
            if (!item) break;

            auto canvasDelta = juce::Point<float>(delta.x / model.zoom, delta.y / model.zoom);
            auto b = item->getBounds();

            switch (activeHandle)
            {
                case HandlePos::TopLeft:
                    b.setLeft(b.getX() + canvasDelta.x);
                    b.setTop(b.getY() + canvasDelta.y);
                    break;
                case HandlePos::TopRight:
                    b.setRight(b.getRight() + canvasDelta.x);
                    b.setTop(b.getY() + canvasDelta.y);
                    break;
                case HandlePos::BottomLeft:
                    b.setLeft(b.getX() + canvasDelta.x);
                    b.setBottom(b.getBottom() + canvasDelta.y);
                    break;
                case HandlePos::BottomRight:
                    b.setRight(b.getRight() + canvasDelta.x);
                    b.setBottom(b.getBottom() + canvasDelta.y);
                    break;
                case HandlePos::Top:
                    b.setTop(b.getY() + canvasDelta.y);
                    break;
                case HandlePos::Bottom:
                    b.setBottom(b.getBottom() + canvasDelta.y);
                    break;
                case HandlePos::Left:
                    b.setLeft(b.getX() + canvasDelta.x);
                    break;
                case HandlePos::Right:
                    b.setRight(b.getRight() + canvasDelta.x);
                    break;
                default: break;
            }

            // Enforce minimum size
            if (b.getWidth() < 30.0f) b.setWidth(30.0f);
            if (b.getHeight() < 20.0f) b.setHeight(20.0f);

            // Aspect lock
            if (item->aspectLock && resizeOrigBounds.getWidth() > 0)
            {
                float aspect = resizeOrigBounds.getHeight() / resizeOrigBounds.getWidth();
                b.setHeight(b.getWidth() * aspect);
            }

            if (model.grid.enabled)
            {
                b = juce::Rectangle<float>(
                    model.snapToGrid(b.getX()), model.snapToGrid(b.getY()),
                    model.snapToGrid(b.getWidth()), model.snapToGrid(b.getHeight()));
            }

            item->setBounds(b);
            model.notifyItemsChanged();

            // ── Proportional group resize for all other selected items ──
            if (!resizeGroupIds_.empty())
            {
                // Compute the total edge deltas applied to the primary item
                float dL = b.getX()       - resizeOrigBounds.getX();
                float dR = b.getRight()   - resizeOrigBounds.getRight();
                float dT = b.getY()       - resizeOrigBounds.getY();
                float dB = b.getBottom()  - resizeOrigBounds.getBottom();

                // Apply the same edge deltas to the group bounding box
                auto gb = resizeGroupOrigBBox_;
                switch (activeHandle)
                {
                    case HandlePos::TopLeft:     gb.setLeft(gb.getX() + dL); gb.setTop(gb.getY() + dT);       break;
                    case HandlePos::TopRight:    gb.setRight(gb.getRight() + dR); gb.setTop(gb.getY() + dT);  break;
                    case HandlePos::BottomLeft:  gb.setLeft(gb.getX() + dL); gb.setBottom(gb.getBottom() + dB); break;
                    case HandlePos::BottomRight: gb.setRight(gb.getRight() + dR); gb.setBottom(gb.getBottom() + dB); break;
                    case HandlePos::Top:    gb.setTop(gb.getY() + dT);       break;
                    case HandlePos::Bottom: gb.setBottom(gb.getBottom() + dB); break;
                    case HandlePos::Left:   gb.setLeft(gb.getX() + dL);      break;
                    case HandlePos::Right:  gb.setRight(gb.getRight() + dR); break;
                    default: break;
                }

                const float origW = resizeGroupOrigBBox_.getWidth();
                const float origH = resizeGroupOrigBBox_.getHeight();
                const float sx    = origW > 0.0f ? gb.getWidth()  / origW : 1.0f;
                const float sy    = origH > 0.0f ? gb.getHeight() / origH : 1.0f;
                const float ox    = resizeGroupOrigBBox_.getX();
                const float oy    = resizeGroupOrigBBox_.getY();

                for (int gi = 0; gi < (int)resizeGroupIds_.size(); ++gi)
                {
                    auto* gitem = model.findItem(resizeGroupIds_[gi]);
                    if (!gitem) continue;
                    const auto& ob = resizeGroupOrigBounds_[gi];
                    gitem->setBounds({
                        gb.getX() + (ob.getX() - ox) * sx,
                        gb.getY() + (ob.getY() - oy) * sy,
                        juce::jmax(1.0f, ob.getWidth()  * sx),
                        juce::jmax(1.0f, ob.getHeight() * sy)
                    });
                }
            }

            break;
        }

        default: break;
    }

    dragLast = e.position;
}

void CanvasView::mouseUp(const juce::MouseEvent& e)
{
    // Create undo action for moves/resizes
    if (dragMode == DragMode::MoveItems)
    {
        auto totalDelta = e.position - dragStart;
        float cdx = totalDelta.x / model.zoom;
        float cdy = totalDelta.y / model.zoom;

        if (std::abs(cdx) > 0.5f || std::abs(cdy) > 0.5f)
        {
            auto sel = model.getSelectedItems();
            std::vector<juce::Uuid> ids;
            for (auto* s : sel) ids.push_back(s->id);

            // Items were already moved during drag — undo needs negative delta
            // We use beginNewTransaction + perform that records state
            model.undoManager.beginNewTransaction("Move Items");
            // Note: items are already at new position, so for undo we store the reverse.
            // But we need to undo-move them back. So perform() is identity, undo() reverses.
            // Simpler: move them back, then perform() moves them forward.
            for (auto* s : sel)
            {
                if (s->locked) continue;
                s->x -= cdx;
                s->y -= cdy;
            }
            model.undoManager.perform(new MoveItemsAction(model, ids, cdx, cdy), "Move");
        }
    }
    else if (dragMode == DragMode::Resize)
    {
        auto* item = model.findItem(resizeItemId);
        if (item)
        {
            auto newBounds = item->getBounds();
            bool primaryMoved = (resizeOrigBounds != newBounds);

            // Collect new bounds for group items before resetting anything
            std::vector<juce::Rectangle<float>> groupNewBounds;
            groupNewBounds.reserve(resizeGroupIds_.size());
            for (auto id : resizeGroupIds_)
            {
                auto* gi = model.findItem(id);
                groupNewBounds.push_back(gi ? gi->getBounds() : juce::Rectangle<float>{});
            }

            if (primaryMoved)
            {
                // Restore items to original positions so undo/redo works cleanly
                item->setBounds(resizeOrigBounds);
                for (int gi = 0; gi < (int)resizeGroupIds_.size(); ++gi)
                {
                    auto* gitem = model.findItem(resizeGroupIds_[gi]);
                    if (gitem) gitem->setBounds(resizeGroupOrigBounds_[gi]);
                }

                model.undoManager.beginNewTransaction("Resize Item");
                model.undoManager.perform(
                    new ResizeItemAction(model, resizeItemId, resizeOrigBounds, newBounds),
                    "Resize");

                for (int gi = 0; gi < (int)resizeGroupIds_.size(); ++gi)
                {
                    if (resizeGroupOrigBounds_[gi] != groupNewBounds[gi])
                        model.undoManager.perform(
                            new ResizeItemAction(model, resizeGroupIds_[gi],
                                                 resizeGroupOrigBounds_[gi], groupNewBounds[gi]),
                            "Resize");
                }
            }
        }

        resizeGroupIds_.clear();
        resizeGroupOrigBounds_.clear();
    }

    dragMode = DragMode::None;
    activeHandle = HandlePos::None;
    activeHGuide = {};
    activeVGuide = {};
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void CanvasView::mouseMove(const juce::MouseEvent& e)
{
    // Change cursor when hovering over resize handles
    for (auto* selItem : model.getSelectedItems())
    {
        if (selItem->locked) continue;
        auto hp = hitTestHandle(*selItem, e.position);
        if (hp != HandlePos::None)
        {
            setMouseCursor(cursorForHandle(hp));
            return;
        }
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void CanvasView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom toward mouse position
        float zoomDelta = w.deltaY * 0.15f;
        model.setZoom(model.zoom * (1.0f + zoomDelta), e.position);
    }
    else
    {
        // Pan
        model.panX += w.deltaX * 80.0f;
        model.panY += w.deltaY * 80.0f;
        model.notifyZoomPanChanged();
    }
}

void CanvasView::mouseDoubleClick(const juce::MouseEvent& e)
{
    auto canvasPt = model.screenToCanvas(e.position);
    auto* item = model.hitTest(canvasPt);
    viewListeners.call(&Listener::canvasItemDoubleClicked, item);
}

//==============================================================================
// Keyboard
//==============================================================================
bool CanvasView::keyPressed(const juce::KeyPress& key)
{
    // Escape: exit interactive mode
    if (key == juce::KeyPress::escapeKey)
    {
        viewListeners.call(&Listener::exitAllInteractiveModes);
        return true;
    }

    // Delete selected items
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        auto sel = model.getSelectedItems();
        for (auto* s : sel)
            if (!s->locked)
                model.removeItem(s->id);
        return true;
    }

    // Ctrl+Z undo
    if (key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        model.undoManager.undo();
        return true;
    }
    // Ctrl+Y / Ctrl+Shift+Z redo
    if (key == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0) ||
        key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        model.undoManager.redo();
        return true;
    }

    // Ctrl+A: select all
    if (key == juce::KeyPress('a', juce::ModifierKeys::ctrlModifier, 0))
    {
        model.selectAll();
        return true;
    }

    // Ctrl+G: group selection
    if (key == juce::KeyPress('g', juce::ModifierKeys::ctrlModifier, 0))
    {
        viewListeners.call(&Listener::groupSelection);
        return true;
    }

    // Ctrl+Shift+G: ungroup selection
    if (key == juce::KeyPress('g', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        viewListeners.call(&Listener::ungroupSelection);
        return true;
    }

    // Ctrl+C / Ctrl+V / Ctrl+D
    if (key == juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0))
    {
        model.copySelection();
        return true;
    }
    if (key == juce::KeyPress('v', juce::ModifierKeys::ctrlModifier, 0))
    {
        // Let MainComponent handle paste (it goes through CanvasEditor::pasteItems
        // which creates visual components)
        return false;
    }
    if (key == juce::KeyPress('d', juce::ModifierKeys::ctrlModifier, 0))
    {
        // Let MainComponent handle duplicate (it goes through CanvasEditor::duplicateItems
        // which creates visual components)
        return false;
    }

    // Arrow keys: nudge selected items
    float nudge = key.getModifiers().isShiftDown() ? 10.0f : 1.0f;
    if (key.getKeyCode() == juce::KeyPress::leftKey)
    {
        for (auto* s : model.getSelectedItems()) if (!s->locked) s->x -= nudge;
        model.notifyItemsChanged();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey)
    {
        for (auto* s : model.getSelectedItems()) if (!s->locked) s->x += nudge;
        model.notifyItemsChanged();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey)
    {
        for (auto* s : model.getSelectedItems()) if (!s->locked) s->y -= nudge;
        model.notifyItemsChanged();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        for (auto* s : model.getSelectedItems()) if (!s->locked) s->y += nudge;
        model.notifyItemsChanged();
        return true;
    }

    // R: rotate selection 90° CW
    if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
    {
        for (auto* s : model.getSelectedItems())
        {
            if (s->locked) continue;
            int oldRot = s->rotation;
            int newRot = (oldRot + 90) % 360;
            model.undoManager.beginNewTransaction("Rotate");
            model.undoManager.perform(new RotateItemAction(model, s->id, oldRot, newRot), "Rotate");
        }
        return true;
    }

    return false;
}

//==============================================================================
// FPS tracking & performance safe-mode
//==============================================================================
void CanvasView::tickFps()
{
    auto nowMs = juce::Time::getMillisecondCounter();
    if (lastFrameTimeMs_ == 0)
    {
        lastFrameTimeMs_ = nowMs;
        return;
    }

    double deltaS = (nowMs - lastFrameTimeMs_) / 1000.0;
    lastFrameTimeMs_ = nowMs;

    if (deltaS <= 0.0) deltaS = 0.001;

    frameCount_++;
    fpsAccumulator_ += deltaS;

    // Update FPS reading every 0.5 seconds for stability
    if (fpsAccumulator_ >= 0.5)
    {
        currentFps_ = static_cast<float>(frameCount_ / fpsAccumulator_);
        frameCount_ = 0;
        fpsAccumulator_ = 0.0;

        // Low-FPS detection — only trigger if NOT already in placeholder mode
        // AND the feature is enabled by the user.
        if (!placeholderMode_ && placeholderModeEnabled_)
        {
            if (currentFps_ < fpsThreshold_)
                lowFpsFrames_++;
            else
                lowFpsFrames_ = 0;

            if (lowFpsFrames_ >= kLowFpsFramesBeforePlaceholder)
                enterPlaceholderMode();
        }
    }
}

void CanvasView::drawFpsOverlay(juce::Graphics& g)
{
    juce::String fpsText = juce::String(currentFps_, 1) + " fps";

    bool isLow = (currentFps_ < fpsThreshold_);
    auto colour = isLow ? juce::Colours::red : juce::Colours::white;

    auto area = juce::Rectangle<float>(8.f, 8.f, 120.f, 24.f);

    // Semi-transparent background pill
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(area, 6.f);

    g.setColour(colour);
    g.setFont(juce::Font(15.f, juce::Font::bold));
    g.drawText(fpsText, area.reduced(6.f, 0.f), juce::Justification::centredLeft);

    // Show restore button when in placeholder mode
    if (placeholderMode_)
    {
        // Status message right of FPS
        {
            auto msgArea = juce::Rectangle<float>(136.f, 8.f, 280.f, 24.f);
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(msgArea, 6.f);
            
            g.setColour(juce::Colours::orange);
            g.setFont(juce::Font(14.f));
            g.drawText("Low FPS Detected - Performance Mode", msgArea, juce::Justification::centred);
        }

        // Restore button below
        auto btnArea = juce::Rectangle<float>(8.f, 36.f, 160.f, 26.f);
        g.setColour(juce::Colours::red.withAlpha(0.7f));
        g.fillRoundedRectangle(btnArea, 6.f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.f, juce::Font::bold));
        g.drawText("Click to Restore", btnArea, juce::Justification::centred);
    }
}

void CanvasView::drawPlaceholderItems(juce::Graphics& g)
{
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item->visible) continue;

        auto screenRect = model.canvasToScreen(item->getBounds());

        // Dashed outline
        g.setColour(juce::Colours::cyan.withAlpha(0.6f));
        g.drawRect(screenRect, 1.5f);

        // Meter name label in center
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(12.f));
        g.drawText(item->name, screenRect, juce::Justification::centred);
    }
}

void CanvasView::enterPlaceholderMode()
{
    placeholderMode_ = true;
    lowFpsFrames_ = 0;

    // Hide all real components — they won't render, saving GPU/CPU
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (item->component)
            item->component->setVisible(false);
    }

    // juce::Logger::writeToLog("[Performance] FPS dropped below "
    //     + juce::String(fpsThreshold_, 1)
    //     + " — entering placeholder mode.");
    repaint();
}

void CanvasView::restoreFromPlaceholderMode()
{
    if (!placeholderMode_) return;

    placeholderMode_ = false;
    lowFpsFrames_ = 0;

    // Re-show all components
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (item->component && item->visible)
            item->component->setVisible(true);
    }


    updateChildBounds();
    repaint();
}

void CanvasView::setPlaceholderModeEnabled(bool enabled)
{
    placeholderModeEnabled_ = enabled;

    // If the user disables the feature while we are currently in placeholder mode,
    // immediately restore full rendering so the canvas is not stuck hidden.
    if (!enabled && placeholderMode_)
        restoreFromPlaceholderMode();
}
