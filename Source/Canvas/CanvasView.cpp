#include "CanvasView.h"
#include "CanvasCommands.h"
#include "../UI/ThemeManager.h"
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

        auto screenRect = model.canvasToScreen(item->getBounds());
        item->component->setBounds(screenRect.toNearestInt());
        
        // Hide actual components if we are in performance placeholder mode
        if (placeholderMode_)
            item->component->setVisible(false);
        else
            item->component->setVisible(item->visible);

        // Apply opacity
        item->component->setAlpha(item->opacity);

        // Apply rotation via AffineTransform
        if (item->rotation != 0)
        {
            auto centre = screenRect.getCentre();
            float rad = static_cast<float>(item->rotation) * juce::MathConstants<float>::pi / 180.0f;
            item->component->setTransform(
                juce::AffineTransform::rotation(rad, centre.x, centre.y));
        }
        else
        {
            item->component->setTransform({});
        }
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

    // 6. Rulers
    if (model.grid.showRuler)
        drawRulers(g);

    // 7. Placeholder outlines (when in performance safe-mode)
    if (placeholderMode_)
        drawPlaceholderItems(g);

    // 8. FPS overlay (always on top)
    drawFpsOverlay(g);
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
    constexpr float rulerH = 20.0f;
    auto& pal = ThemeManager::getInstance().getPalette();

    // Top ruler
    g.setColour(pal.rulerBg.withAlpha(0.9f));
    g.fillRect(0.0f, 0.0f, static_cast<float>(getWidth()), rulerH);

    // Left ruler
    g.fillRect(0.0f, 0.0f, rulerH, static_cast<float>(getHeight()));

    g.setColour(pal.rulerText);
    g.setFont(9.0f);

    int spacing = std::max(50, model.grid.spacing * 5);
    float screenSpacing = spacing * model.zoom;

    // Horizontal tick marks
    float offX = std::fmod(model.panX, screenSpacing);
    if (offX < 0) offX += screenSpacing;
    for (float x = offX; x < getWidth(); x += screenSpacing)
    {
        float canvasX = model.screenToCanvas(juce::Point<float>(x, 0)).x;
        g.drawVerticalLine(static_cast<int>(x), rulerH - 6.0f, rulerH);
        g.drawText(juce::String(static_cast<int>(canvasX)),
                   static_cast<int>(x) - 15, 1, 30, static_cast<int>(rulerH) - 4,
                   juce::Justification::centred);
    }

    // Vertical tick marks
    float offY = std::fmod(model.panY, screenSpacing);
    if (offY < 0) offY += screenSpacing;
    for (float y = offY; y < getHeight(); y += screenSpacing)
    {
        float canvasY = model.screenToCanvas(juce::Point<float>(0, y)).y;
        g.drawHorizontalLine(static_cast<int>(y), rulerH - 6.0f, rulerH);
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(
            -juce::MathConstants<float>::halfPi, 10.0f, y));
        g.drawText(juce::String(static_cast<int>(canvasY)),
                   -5, static_cast<int>(y) - 6, 30, 12,
                   juce::Justification::centredLeft);
        g.restoreState();
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

        // Select
        if (!model.isSelected(hitItem->id))
            model.selectItem(hitItem->id, e.mods.isShiftDown());

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
            if (resizeOrigBounds != newBounds)
            {
                // Move back to original, then perform to get undo support
                item->setBounds(resizeOrigBounds);
                model.undoManager.beginNewTransaction("Resize Item");
                model.undoManager.perform(
                    new ResizeItemAction(model, resizeItemId, resizeOrigBounds, newBounds),
                    "Resize");
            }
        }
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
        if (!placeholderMode_)
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
