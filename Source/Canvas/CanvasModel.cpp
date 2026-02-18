#include "CanvasModel.h"
#include <algorithm>
#include <cmath>

//==============================================================================
CanvasModel::CanvasModel() {}

//==============================================================================
// Items
//==============================================================================
CanvasItem* CanvasModel::addItem(std::unique_ptr<CanvasItem> item)
{
    item->zOrder = nextZOrder++;
    auto* ptr = item.get();
    items.push_back(std::move(item));
    notifyItemsChanged();
    return ptr;
}

void CanvasModel::removeItem(const juce::Uuid& id)
{
    selection.erase(id);
    items.erase(std::remove_if(items.begin(), items.end(),
        [&](auto& p) { return p->id == id; }), items.end());
    notifyItemsChanged();
}

CanvasItem* CanvasModel::findItem(const juce::Uuid& id)
{
    for (auto& p : items) if (p->id == id) return p.get();
    return nullptr;
}

const CanvasItem* CanvasModel::findItem(const juce::Uuid& id) const
{
    for (auto& p : items) if (p->id == id) return p.get();
    return nullptr;
}

void CanvasModel::sortByZOrder()
{
    std::stable_sort(items.begin(), items.end(),
        [](auto& a, auto& b) { return a->zOrder < b->zOrder; });
}

//==============================================================================
// Selection
//==============================================================================
void CanvasModel::selectItem(const juce::Uuid& id, bool addToSelection)
{
    if (!addToSelection) selection.clear();
    selection.insert(id);
    notifySelectionChanged();
}

void CanvasModel::deselectItem(const juce::Uuid& id)
{
    selection.erase(id);
    notifySelectionChanged();
}

void CanvasModel::clearSelection()
{
    if (selection.empty()) return;
    selection.clear();
    notifySelectionChanged();
}

void CanvasModel::selectAll()
{
    for (auto& p : items)
        if (!p->locked) selection.insert(p->id);
    notifySelectionChanged();
}

bool CanvasModel::isSelected(const juce::Uuid& id) const
{
    return selection.count(id) > 0;
}

std::vector<CanvasItem*> CanvasModel::getSelectedItems()
{
    std::vector<CanvasItem*> result;
    for (auto& p : items)
        if (selection.count(p->id)) result.push_back(p.get());
    return result;
}

std::vector<const CanvasItem*> CanvasModel::getSelectedItems() const
{
    std::vector<const CanvasItem*> result;
    for (auto& p : items)
        if (selection.count(p->id)) result.push_back(p.get());
    return result;
}

CanvasItem* CanvasModel::hitTest(juce::Point<float> canvasPt)
{
    // Iterate back-to-front (highest z on top)
    for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i)
    {
        auto* item = items[static_cast<size_t>(i)].get();
        if (!item->visible) continue;
        if (item->getBounds().contains(canvasPt))
            return item;
    }
    return nullptr;
}

//==============================================================================
// Zoom / Pan
//==============================================================================
void CanvasModel::setZoom(float z, juce::Point<float> pivot)
{
    float newZoom = juce::jlimit(0.1f, 5.0f, z);
    if (std::abs(newZoom - zoom) < 1e-6f) return;

    // Adjust pan so the pivot stays in the same screen position
    panX = pivot.x - (pivot.x - panX) * (newZoom / zoom);
    panY = pivot.y - (pivot.y - panY) * (newZoom / zoom);
    zoom = newZoom;
    notifyZoomPanChanged();
}

juce::Point<float> CanvasModel::screenToCanvas(juce::Point<float> screen) const
{
    return { (screen.x - panX) / zoom, (screen.y - panY) / zoom };
}

juce::Point<float> CanvasModel::canvasToScreen(juce::Point<float> canvas) const
{
    return { canvas.x * zoom + panX, canvas.y * zoom + panY };
}

juce::Rectangle<float> CanvasModel::screenToCanvas(juce::Rectangle<float> r) const
{
    auto tl = screenToCanvas(r.getTopLeft());
    auto br = screenToCanvas(r.getBottomRight());
    return { tl, br };
}

juce::Rectangle<float> CanvasModel::canvasToScreen(juce::Rectangle<float> r) const
{
    auto tl = canvasToScreen(r.getTopLeft());
    auto br = canvasToScreen(r.getBottomRight());
    return { tl, br };
}

//==============================================================================
// Grid snap
//==============================================================================
float CanvasModel::snapToGrid(float v) const
{
    if (!grid.enabled || grid.spacing < 1) return v;
    float s = static_cast<float>(grid.spacing);
    return std::round(v / s) * s;
}

juce::Point<float> CanvasModel::snapToGrid(juce::Point<float> p) const
{
    return { snapToGrid(p.x), snapToGrid(p.y) };
}

//==============================================================================
// Smart Guides
//==============================================================================
CanvasModel::SmartGuideResult CanvasModel::computeSmartGuides(
    const juce::Rectangle<float>& movingRect, const juce::Uuid& excludeId) const
{
    SmartGuideResult result;
    constexpr float kSnapThresh = 5.0f;

    float moveCX = movingRect.getCentreX();
    float moveCY = movingRect.getCentreY();

    float bestDx = kSnapThresh + 1.0f;
    float bestDy = kSnapThresh + 1.0f;

    for (auto& p : items)
    {
        if (p->id == excludeId || !p->visible) continue;
        auto r = p->getBounds();

        // Horizontal guides (match X edges and centres)
        float xEdges[] = { r.getX(), r.getCentreX(), r.getRight() };
        float movXEdges[] = { movingRect.getX(), moveCX, movingRect.getRight() };

        for (float xe : xEdges)
            for (float me : movXEdges)
            {
                float d = std::abs(xe - me);
                if (d < bestDx) { bestDx = d; result.vGuide = { true, xe }; }
            }

        // Vertical guides (Y edges and centres)
        float yEdges[] = { r.getY(), r.getCentreY(), r.getBottom() };
        float movYEdges[] = { movingRect.getY(), moveCY, movingRect.getBottom() };

        for (float ye : yEdges)
            for (float me : movYEdges)
            {
                float d = std::abs(ye - me);
                if (d < bestDy) { bestDy = d; result.hGuide = { true, ye }; }
            }
    }

    if (bestDx > kSnapThresh) result.vGuide.active = false;
    if (bestDy > kSnapThresh) result.hGuide.active = false;
    return result;
}

//==============================================================================
// Alignment
//==============================================================================
void CanvasModel::alignSelection(AlignEdge edge)
{
    auto sel = getSelectedItems();
    if (sel.size() < 2) return;

    // Calculate reference from bounding box of selection
    float refVal = 0.0f;
    auto bb = sel[0]->getBounds();
    for (auto* item : sel) bb = bb.getUnion(item->getBounds());

    switch (edge)
    {
        case AlignEdge::Left:    refVal = bb.getX(); break;
        case AlignEdge::CenterH: refVal = bb.getCentreX(); break;
        case AlignEdge::Right:   refVal = bb.getRight(); break;
        case AlignEdge::Top:     refVal = bb.getY(); break;
        case AlignEdge::CenterV: refVal = bb.getCentreY(); break;
        case AlignEdge::Bottom:  refVal = bb.getBottom(); break;
    }

    for (auto* item : sel)
    {
        if (item->locked) continue;
        switch (edge)
        {
            case AlignEdge::Left:    item->x = refVal; break;
            case AlignEdge::CenterH: item->x = refVal - item->width * 0.5f; break;
            case AlignEdge::Right:   item->x = refVal - item->width; break;
            case AlignEdge::Top:     item->y = refVal; break;
            case AlignEdge::CenterV: item->y = refVal - item->height * 0.5f; break;
            case AlignEdge::Bottom:  item->y = refVal - item->height; break;
        }
    }
    notifyItemsChanged();
}

void CanvasModel::distributeSelectionH()
{
    auto sel = getSelectedItems();
    if (sel.size() < 3) return;

    std::sort(sel.begin(), sel.end(), [](auto* a, auto* b) { return a->x < b->x; });
    float minX = sel.front()->x;
    float maxX = sel.back()->x + sel.back()->width;
    float totalW = 0.0f;
    for (auto* s : sel) totalW += s->width;
    float gap = (maxX - minX - totalW) / static_cast<float>(sel.size() - 1);

    float cx = minX;
    for (auto* s : sel)
    {
        if (!s->locked) s->x = cx;
        cx += s->width + gap;
    }
    notifyItemsChanged();
}

void CanvasModel::distributeSelectionV()
{
    auto sel = getSelectedItems();
    if (sel.size() < 3) return;

    std::sort(sel.begin(), sel.end(), [](auto* a, auto* b) { return a->y < b->y; });
    float minY = sel.front()->y;
    float maxY = sel.back()->y + sel.back()->height;
    float totalH = 0.0f;
    for (auto* s : sel) totalH += s->height;
    float gap = (maxY - minY - totalH) / static_cast<float>(sel.size() - 1);

    float cy = minY;
    for (auto* s : sel)
    {
        if (!s->locked) s->y = cy;
        cy += s->height + gap;
    }
    notifyItemsChanged();
}

//==============================================================================
// Grouping
//==============================================================================
void CanvasModel::groupSelection()
{
    auto sel = getSelectedItems();
    if (sel.size() < 2) return;

    auto gid = juce::Uuid();
    for (auto* item : sel)
        item->groupId = gid;
    notifyItemsChanged();
}

void CanvasModel::ungroupSelection()
{
    auto sel = getSelectedItems();
    for (auto* item : sel)
        item->groupId = juce::Uuid();       // null Uuid = not grouped
    notifyItemsChanged();
}

std::vector<CanvasItem*> CanvasModel::getGroupMembers(const juce::Uuid& gid)
{
    std::vector<CanvasItem*> result;
    if (gid.isNull()) return result;
    for (auto& p : items)
        if (p->groupId == gid) result.push_back(p.get());
    return result;
}

void CanvasModel::selectGroup(const juce::Uuid& itemId, bool addToSelection)
{
    auto* item = findItem(itemId);
    if (!item) return;

    if (item->groupId.isNull())
    {
        selectItem(itemId, addToSelection);
        return;
    }

    if (!addToSelection) selection.clear();
    for (auto& p : items)
        if (p->groupId == item->groupId)
            selection.insert(p->id);
    notifySelectionChanged();
}

//==============================================================================
// Clipboard (lightweight — copies descriptors, new components created on paste)
//==============================================================================
void CanvasModel::copySelection()
{
    auto sel = getSelectedItems();
    if (sel.empty()) return;

    clipboard.clear();
    // Store positions relative to selection bounding-box top-left
    auto bb = sel[0]->getBounds();
    for (auto* s : sel) bb = bb.getUnion(s->getBounds());

    // Map each unique groupId to a tag integer so we can rebuild groups on paste
    std::map<juce::Uuid, int> groupTagMap;
    int nextTag = 0;
    for (auto* s : sel)
    {
        int tag = -1;
        if (!s->groupId.isNull())
        {
            auto it = groupTagMap.find(s->groupId);
            if (it != groupTagMap.end())
                tag = it->second;
            else
            {
                tag = nextTag++;
                groupTagMap[s->groupId] = tag;
            }
        }
        clipboard.push_back({ s->meterType,
                              s->x - bb.getX(), s->y - bb.getY(),
                              s->width, s->height, s->rotation, s->name, tag });
    }
}

void CanvasModel::paste(juce::Point<float> at)
{
    if (clipboard.empty()) return;

    clearSelection();

    // Build new groupIds for each tag so pasted items share fresh group IDs
    std::map<int, juce::Uuid> tagToGroup;
    for (auto& ci : clipboard)
    {
        if (ci.clipGroupTag >= 0 && tagToGroup.find(ci.clipGroupTag) == tagToGroup.end())
            tagToGroup[ci.clipGroupTag] = juce::Uuid();
    }

    for (auto& ci : clipboard)
    {
        auto item = std::make_unique<CanvasItem>();
        item->meterType = ci.type;
        item->x = at.x + ci.relX;
        item->y = at.y + ci.relY;
        item->width = ci.w;
        item->height = ci.h;
        item->rotation = ci.rot;
        item->name = ci.name.isEmpty() ? meterTypeName(ci.type) : ci.name;
        if (ci.clipGroupTag >= 0)
            item->groupId = tagToGroup[ci.clipGroupTag];
        // Note: component is NOT created here — CanvasEditor's addItem wrapper does that.
        auto* added = addItem(std::move(item));
        selection.insert(added->id);
    }
    notifySelectionChanged();
}

void CanvasModel::duplicateSelection()
{
    auto sel = getSelectedItems();
    if (sel.empty()) return;

    copySelection();
    auto bb = sel[0]->getBounds();
    for (auto* s : sel) bb = bb.getUnion(s->getBounds());
    paste({ bb.getX() + 20.0f, bb.getY() + 20.0f });
}

//==============================================================================
// Background painting
//==============================================================================
void CanvasBackground::paint(juce::Graphics& g, juce::Rectangle<float> area) const
{
    switch (mode)
    {
        case BackgroundMode::SolidColour:
            g.setColour(colour1);
            g.fillRect(area);
            break;

        case BackgroundMode::LinearGradient:
        {
            float rad = angle * juce::MathConstants<float>::pi / 180.0f;
            float dx = std::cos(rad) * area.getWidth() * 0.5f;
            float dy = std::sin(rad) * area.getHeight() * 0.5f;
            auto cx = area.getCentreX(), cy = area.getCentreY();
            g.setGradientFill(juce::ColourGradient(colour1, cx - dx, cy - dy,
                                                    colour2, cx + dx, cy + dy, false));
            g.fillRect(area);
            break;
        }
        case BackgroundMode::RadialGradient:
        {
            auto cx = area.getCentreX(), cy = area.getCentreY();
            float radius = std::sqrt(area.getWidth() * area.getWidth() +
                                     area.getHeight() * area.getHeight()) * 0.5f;
            g.setGradientFill(juce::ColourGradient(colour1, cx, cy,
                                                    colour2, cx + radius, cy, true));
            g.fillRect(area);
            break;
        }
        case BackgroundMode::Image:
        {
            if (cachedImage.isValid())
            {
                switch (fitMode)
                {
                    case FitMode::Stretch:
                        g.drawImage(cachedImage, area);
                        break;
                    case FitMode::Fill:
                    {
                        float scaleX = area.getWidth() / cachedImage.getWidth();
                        float scaleY = area.getHeight() / cachedImage.getHeight();
                        float scale = std::max(scaleX, scaleY);
                        float w = cachedImage.getWidth() * scale;
                        float h = cachedImage.getHeight() * scale;
                        g.drawImage(cachedImage,
                                    area.getCentreX() - w * 0.5f,
                                    area.getCentreY() - h * 0.5f, w, h,
                                    0, 0, cachedImage.getWidth(), cachedImage.getHeight());
                        break;
                    }
                    case FitMode::Fit:
                    {
                        float scaleX = area.getWidth() / cachedImage.getWidth();
                        float scaleY = area.getHeight() / cachedImage.getHeight();
                        float scale = std::min(scaleX, scaleY);
                        float w = cachedImage.getWidth() * scale;
                        float h = cachedImage.getHeight() * scale;
                        g.drawImage(cachedImage,
                                    area.getCentreX() - w * 0.5f,
                                    area.getCentreY() - h * 0.5f, w, h,
                                    0, 0, cachedImage.getWidth(), cachedImage.getHeight());
                        break;
                    }
                    case FitMode::Tile:
                    {
                        float iw = static_cast<float>(cachedImage.getWidth());
                        float ih = static_cast<float>(cachedImage.getHeight());
                        for (float ty = area.getY(); ty < area.getBottom(); ty += ih)
                            for (float tx = area.getX(); tx < area.getRight(); tx += iw)
                                g.drawImageAt(cachedImage, static_cast<int>(tx), static_cast<int>(ty));
                        break;
                    }
                }
            }
            else
            {
                g.setColour(colour1);
                g.fillRect(area);
            }
            break;
        }
    }
}
