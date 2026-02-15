#pragma once

#include <JuceHeader.h>
#include "CanvasModel.h"

//==============================================================================
/// Move one or more items by a delta.
class MoveItemsAction : public juce::UndoableAction
{
public:
    MoveItemsAction(CanvasModel& m, std::vector<juce::Uuid> ids, float dx, float dy)
        : model(m), itemIds(std::move(ids)), deltaX(dx), deltaY(dy) {}

    bool perform() override
    {
        for (auto& id : itemIds)
            if (auto* item = model.findItem(id))
            { item->x += deltaX; item->y += deltaY; }
        model.notifyItemsChanged();
        return true;
    }

    bool undo() override
    {
        for (auto& id : itemIds)
            if (auto* item = model.findItem(id))
            { item->x -= deltaX; item->y -= deltaY; }
        model.notifyItemsChanged();
        return true;
    }

    int getSizeInUnits() override { return 16; }

private:
    CanvasModel& model;
    std::vector<juce::Uuid> itemIds;
    float deltaX, deltaY;
};

//==============================================================================
/// Resize a single item.
class ResizeItemAction : public juce::UndoableAction
{
public:
    ResizeItemAction(CanvasModel& m, juce::Uuid id,
                     juce::Rectangle<float> oldBounds,
                     juce::Rectangle<float> newBounds)
        : model(m), itemId(id), oldB(oldBounds), newB(newBounds) {}

    bool perform() override
    {
        if (auto* item = model.findItem(itemId))
            item->setBounds(newB);
        model.notifyItemsChanged();
        return true;
    }

    bool undo() override
    {
        if (auto* item = model.findItem(itemId))
            item->setBounds(oldB);
        model.notifyItemsChanged();
        return true;
    }

    int getSizeInUnits() override { return 16; }

private:
    CanvasModel& model;
    juce::Uuid itemId;
    juce::Rectangle<float> oldB, newB;
};

//==============================================================================
/// Change z-order of an item.
class ChangeZOrderAction : public juce::UndoableAction
{
public:
    ChangeZOrderAction(CanvasModel& m, juce::Uuid id, int oldZ, int newZ)
        : model(m), itemId(id), oldZOrder(oldZ), newZOrder(newZ) {}

    bool perform() override
    {
        if (auto* item = model.findItem(itemId))
            item->zOrder = newZOrder;
        model.sortByZOrder();
        model.notifyItemsChanged();
        return true;
    }

    bool undo() override
    {
        if (auto* item = model.findItem(itemId))
            item->zOrder = oldZOrder;
        model.sortByZOrder();
        model.notifyItemsChanged();
        return true;
    }

    int getSizeInUnits() override { return 8; }

private:
    CanvasModel& model;
    juce::Uuid itemId;
    int oldZOrder, newZOrder;
};

//==============================================================================
/// Swap z-order of two items atomically (single undo step, one notify).
class SwapZOrderAction : public juce::UndoableAction
{
public:
    SwapZOrderAction(CanvasModel& m, juce::Uuid idA, int zA, juce::Uuid idB, int zB)
        : model(m), itemIdA(idA), zOrderA(zA), itemIdB(idB), zOrderB(zB) {}

    bool perform() override
    {
        if (auto* a = model.findItem(itemIdA)) a->zOrder = zOrderB;
        if (auto* b = model.findItem(itemIdB)) b->zOrder = zOrderA;
        model.sortByZOrder();
        model.notifyItemsChanged();
        return true;
    }

    bool undo() override
    {
        if (auto* a = model.findItem(itemIdA)) a->zOrder = zOrderA;
        if (auto* b = model.findItem(itemIdB)) b->zOrder = zOrderB;
        model.sortByZOrder();
        model.notifyItemsChanged();
        return true;
    }

    int getSizeInUnits() override { return 16; }

private:
    CanvasModel& model;
    juce::Uuid itemIdA, itemIdB;
    int zOrderA, zOrderB;
};

//==============================================================================
/// Rotate a single item (0/90/180/270 snap).
class RotateItemAction : public juce::UndoableAction
{
public:
    RotateItemAction(CanvasModel& m, juce::Uuid id, int oldRot, int newRot)
        : model(m), itemId(id), oldRotation(oldRot), newRotation(newRot) {}

    bool perform() override
    {
        if (auto* item = model.findItem(itemId))
            item->rotation = newRotation;
        model.notifyItemsChanged();
        return true;
    }

    bool undo() override
    {
        if (auto* item = model.findItem(itemId))
            item->rotation = oldRotation;
        model.notifyItemsChanged();
        return true;
    }

    int getSizeInUnits() override { return 8; }

private:
    CanvasModel& model;
    juce::Uuid itemId;
    int oldRotation, newRotation;
};
