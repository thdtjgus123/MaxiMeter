#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include <map>

//==============================================================================
/// Identifiers for all application shortcuts.
enum class ShortcutId
{
    // File
    NewProject,
    OpenProject,
    SaveProject,
    SaveProjectAs,
    ExportVideo,
    BatchExport,

    // Edit
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    Duplicate,
    Delete,
    SelectAll,

    // View
    ZoomIn,
    ZoomOut,
    ZoomToFit,
    ZoomReset,
    ToggleGrid,
    ToggleSnap,
    ToggleTheme,
    ToggleFullscreen,

    // Canvas
    BringToFront,
    SendToBack,
    LockSelected,
    GroupSelected,
    UngroupSelected,
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenterH,
    AlignCenterV,
    DistributeH,
    DistributeV,

    // Transport
    PlayPause,
    Stop,
    Rewind,

    NumShortcuts
};

//==============================================================================
/// Centralised keyboard shortcut manager.
/// Maps ShortcutId → KeyPress + action callback.  MainComponent delegates
/// keyPressed() here.
class KeyboardShortcutManager
{
public:
    KeyboardShortcutManager();

    /// Register / override a shortcut binding.
    void setBinding(ShortcutId id, const juce::KeyPress& key);

    /// Register an action callback for a shortcut.
    void setAction(ShortcutId id, std::function<void()> action);

    /// Try to handle a keypress.  Returns true if consumed.
    bool handleKeyPress(const juce::KeyPress& key);

    /// Get the human-readable description of a shortcut.
    static juce::String getDescription(ShortcutId id);

    /// Get the current binding for a shortcut.
    juce::KeyPress getBinding(ShortcutId id) const;

    /// Build a tooltip string:  "Description (Ctrl+X)"
    juce::String getTooltip(ShortcutId id) const;

private:
    struct ShortcutEntry
    {
        juce::KeyPress            key;
        std::function<void()>     action;
    };

    std::map<ShortcutId, ShortcutEntry> shortcuts;

    void setupDefaults();
};
