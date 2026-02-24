#pragma once

#include <JuceHeader.h>
#include "VJScene.h"
#include "VJTransitionEngine.h"

class CanvasEditor;

//==============================================================================
/// Manages the list of VJ scenes: snapshot, restore, persistence.
class VJSceneManager
{
public:
    explicit VJSceneManager(CanvasEditor& editor);

    //==========================================================================
    /// Capture the current canvas state and add it as a new scene.
    /// Returns the index of the newly created scene.
    int addSceneFromCurrent(const juce::String& name = {});

    /// Overwrite scene at 'index' with the current canvas state + fresh thumbnail.
    void updateSceneFromCurrent(int index);

    /// Restore scene at 'index' into the canvas.
    void loadScene(int index);

    void deleteScene(int index);
    void duplicateScene(int index);
    void moveScene(int fromIndex, int toIndex);
    void renameScene(int index, const juce::String& newName);

    //==========================================================================
    const std::vector<VJScene>& getScenes() const  { return scenes_; }
    int  getActiveIndex()        const              { return activeIndex_; }
    int  getSceneCount()         const              { return static_cast<int>(scenes_.size()); }

    //==========================================================================
    /// Serialise all scenes to a juce::var array (embedded in project file).
    juce::var serializeAll() const;

    /// Restore from a juce::var array previously produced by serializeAll().
    void deserializeAll(const juce::var& v);

    //==========================================================================
    /// Fired on the message thread after a scene is loaded.
    std::function<void(int newIndex)> onSceneLoaded;

    /// Fired whenever the scene list changes (add/remove/rename).
    std::function<void()> onScenesChanged;

private:
    juce::Image captureThumbnail() const;
    juce::var   captureCanvasState() const;
    void        applyCanvasState(const juce::var& state);

    CanvasEditor&       editor_;
    std::vector<VJScene> scenes_;
    int                 activeIndex_ = -1;
};
