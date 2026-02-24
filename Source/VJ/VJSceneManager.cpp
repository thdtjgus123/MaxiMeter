#include "VJSceneManager.h"
#include "../Canvas/CanvasEditor.h"
#include "../Project/ProjectSerializer.h"

//==============================================================================
VJSceneManager::VJSceneManager(CanvasEditor& editor)
    : editor_(editor)
{}

//==============================================================================
int VJSceneManager::addSceneFromCurrent(const juce::String& nameIn)
{
    VJScene scene;
    scene.id        = juce::Uuid();
    scene.name      = nameIn.isEmpty() ? ("Scene " + juce::String(scenes_.size() + 1)) : nameIn;
    scene.canvasState = captureCanvasState();
    scene.thumbnail   = captureThumbnail();

    scenes_.push_back(std::move(scene));
    const int idx = static_cast<int>(scenes_.size()) - 1;
    activeIndex_ = idx;

    if (onScenesChanged) onScenesChanged();
    return idx;
}

//==============================================================================
void VJSceneManager::updateSceneFromCurrent(int index)
{
    if (index < 0 || index >= getSceneCount()) return;
    scenes_[index].canvasState = captureCanvasState();
    scenes_[index].thumbnail   = captureThumbnail();
    if (onScenesChanged) onScenesChanged();
}

//==============================================================================
void VJSceneManager::loadScene(int index)
{
    if (index < 0 || index >= getSceneCount()) return;
    activeIndex_ = index;

    if (onSceneLoaded) onSceneLoaded(index);   // caller applies the state
}

//==============================================================================
void VJSceneManager::deleteScene(int index)
{
    if (index < 0 || index >= getSceneCount()) return;
    scenes_.erase(scenes_.begin() + index);

    if (activeIndex_ >= getSceneCount())
        activeIndex_ = getSceneCount() - 1;

    if (onScenesChanged) onScenesChanged();
}

//==============================================================================
void VJSceneManager::duplicateScene(int index)
{
    if (index < 0 || index >= getSceneCount()) return;
    VJScene copy   = scenes_[index];
    copy.id        = juce::Uuid();
    copy.name     += " (copy)";
    scenes_.insert(scenes_.begin() + index + 1, std::move(copy));
    if (onScenesChanged) onScenesChanged();
}

//==============================================================================
void VJSceneManager::moveScene(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= getSceneCount()) return;
    if (toIndex   < 0 || toIndex   >= getSceneCount()) return;
    if (fromIndex == toIndex) return;

    VJScene tmp = std::move(scenes_[fromIndex]);
    scenes_.erase(scenes_.begin() + fromIndex);
    scenes_.insert(scenes_.begin() + toIndex, std::move(tmp));

    if (activeIndex_ == fromIndex)
        activeIndex_ = toIndex;

    if (onScenesChanged) onScenesChanged();
}

//==============================================================================
void VJSceneManager::renameScene(int index, const juce::String& newName)
{
    if (index < 0 || index >= getSceneCount()) return;
    scenes_[index].name = newName;
    if (onScenesChanged) onScenesChanged();
}

//==============================================================================
juce::var VJSceneManager::serializeAll() const
{
    auto* arr = new juce::DynamicObject();
    juce::Array<juce::var> list;
    for (const auto& s : scenes_)
        list.add(s.toVar());
    arr->setProperty("scenes", list);
    arr->setProperty("activeIndex", activeIndex_);
    return juce::var(arr);
}

//==============================================================================
void VJSceneManager::deserializeAll(const juce::var& v)
{
    scenes_.clear();
    if (const auto* obj = v.getDynamicObject())
    {
        if (const auto* arr = obj->getProperty("scenes").getArray())
            for (const auto& item : *arr)
                scenes_.push_back(VJScene::fromVar(item));

        activeIndex_ = obj->getProperty("activeIndex");
    }
    if (onScenesChanged) onScenesChanged();
}

//==============================================================================
juce::Image VJSceneManager::captureThumbnail() const
{
    return editor_.renderPreviewFrame(160, 90);
}

juce::var VJSceneManager::captureCanvasState() const
{
    const juce::String json = ProjectSerializer::serialise(editor_.getModel());
    return juce::var(json);
}

void VJSceneManager::applyCanvasState(const juce::var& state)
{
    // Intentionally left for the owner (VJEditor / MainComponent) to handle
    // by subscribing to onSceneLoaded and calling loadProjectResult().
    juce::ignoreUnused(state);
}
