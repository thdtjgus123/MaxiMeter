#include "MiniMap.h"
#include "../UI/ThemeManager.h"
#include <cmath>

//==============================================================================
MiniMap::MiniMap(CanvasModel& m) : model(m)
{
    model.addListener(this);
}

MiniMap::~MiniMap()
{
    model.removeListener(this);
}

juce::Rectangle<float> MiniMap::getContentBounds() const
{
    if (model.getNumItems() == 0)
        return { 0, 0, 1000, 1000 };

    auto bb = model.getItem(0)->getBounds();
    for (int i = 1; i < model.getNumItems(); ++i)
        bb = bb.getUnion(model.getItem(i)->getBounds());
    return bb.expanded(100);
}

void MiniMap::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2);
    auto& pal = ThemeManager::getInstance().getPalette();
    g.setColour(pal.panelBg.withAlpha(0.85f));
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(pal.border.withAlpha(0.4f));
    g.drawRoundedRectangle(area, 4.0f, 0.5f);

    auto content = getContentBounds();
    if (content.getWidth() < 1 || content.getHeight() < 1) return;

    // Compute scale so content fits in minimap
    float scaleX = area.getWidth() / content.getWidth();
    float scaleY = area.getHeight() / content.getHeight();
    float scale = std::min(scaleX, scaleY);

    auto transform = [&](juce::Point<float> p) -> juce::Point<float>
    {
        return {
            area.getX() + (p.x - content.getX()) * scale,
            area.getY() + (p.y - content.getY()) * scale
        };
    };

    // Draw items as small rectangles
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item->visible) continue;

        auto tl = transform(item->getBounds().getTopLeft());
        auto br = transform(item->getBounds().getBottomRight());
        juce::Rectangle<float> r(tl, br);
        r = r.constrainedWithin(area);

        bool sel = model.isSelected(item->id);
        g.setColour(sel ? pal.selectionOutline : pal.accent.withAlpha(0.6f));
        g.fillRect(r);
    }

    // Draw viewport rectangle
    // Viewport in canvas space: from screenToCanvas(0,0) to screenToCanvas(viewW, viewH)
    // We need the parent's size — approximate using model zoom/pan
    float viewW = 1440.0f;  // approximate
    float viewH = 900.0f;
    auto vpTL = model.screenToCanvas(juce::Point<float>(0, 0));
    auto vpBR = model.screenToCanvas(juce::Point<float>(viewW, viewH));
    auto vpTLm = transform(vpTL);
    auto vpBRm = transform(vpBR);
    juce::Rectangle<float> vpRect(vpTLm, vpBRm);
    vpRect = vpRect.constrainedWithin(area);

    g.setColour(pal.bodyText.withAlpha(0.4f));
    g.drawRect(vpRect, 1.0f);
}

void MiniMap::navigateTo(juce::Point<float> minimapPt)
{
    auto area = getLocalBounds().toFloat().reduced(2);
    auto content = getContentBounds();

    float scaleX = area.getWidth() / content.getWidth();
    float scaleY = area.getHeight() / content.getHeight();
    float scale = std::min(scaleX, scaleY);

    float canvasX = content.getX() + (minimapPt.x - area.getX()) / scale;
    float canvasY = content.getY() + (minimapPt.y - area.getY()) / scale;

    // Centre the viewport on this canvas point
    float viewW = 1440.0f;
    float viewH = 900.0f;
    model.panX = viewW * 0.5f - canvasX * model.zoom;
    model.panY = viewH * 0.5f - canvasY * model.zoom;
    model.notifyZoomPanChanged();
}

void MiniMap::mouseDown(const juce::MouseEvent& e)
{
    navigateTo(e.position);
}

void MiniMap::mouseDrag(const juce::MouseEvent& e)
{
    navigateTo(e.position);
}
