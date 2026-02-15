#include "KeyboardShortcutManager.h"

//==============================================================================
KeyboardShortcutManager::KeyboardShortcutManager()
{
    setupDefaults();
}

void KeyboardShortcutManager::setupDefaults()
{
    using K = juce::KeyPress;
    auto ctrl  = juce::ModifierKeys::ctrlModifier;
    auto shift = juce::ModifierKeys::shiftModifier;
    auto ctrlShift = ctrl | shift;

    // File
    shortcuts[ShortcutId::NewProject]    = { K('n', ctrl, 0), nullptr };
    shortcuts[ShortcutId::OpenProject]   = { K('o', ctrl, 0), nullptr };
    shortcuts[ShortcutId::SaveProject]   = { K('s', ctrl, 0), nullptr };
    shortcuts[ShortcutId::SaveProjectAs] = { K('s', ctrlShift, 0), nullptr };
    shortcuts[ShortcutId::ExportVideo]   = { K('e', ctrl, 0), nullptr };
    shortcuts[ShortcutId::BatchExport]   = { K('e', ctrlShift, 0), nullptr };

    // Edit
    shortcuts[ShortcutId::Undo]       = { K('z', ctrl, 0), nullptr };
    shortcuts[ShortcutId::Redo]       = { K('y', ctrl, 0), nullptr };
    shortcuts[ShortcutId::Cut]        = { K('x', ctrl, 0), nullptr };
    shortcuts[ShortcutId::Copy]       = { K('c', ctrl, 0), nullptr };
    shortcuts[ShortcutId::Paste]      = { K('v', ctrl, 0), nullptr };
    shortcuts[ShortcutId::Duplicate]  = { K('d', ctrl, 0), nullptr };
    shortcuts[ShortcutId::Delete]     = { K(K::deleteKey, 0, 0), nullptr };
    shortcuts[ShortcutId::SelectAll]  = { K('a', ctrl, 0), nullptr };

    // View
    shortcuts[ShortcutId::ZoomIn]      = { K('=', ctrl, 0), nullptr };  // Ctrl+=
    shortcuts[ShortcutId::ZoomOut]     = { K('-', ctrl, 0), nullptr };  // Ctrl+-
    shortcuts[ShortcutId::ZoomToFit]   = { K('0', ctrl, 0), nullptr };  // Ctrl+0
    shortcuts[ShortcutId::ZoomReset]   = { K('1', ctrl, 0), nullptr };  // Ctrl+1
    shortcuts[ShortcutId::ToggleGrid]  = { K('g', ctrl, 0), nullptr };
    shortcuts[ShortcutId::ToggleSnap]  = { K('g', ctrlShift, 0), nullptr };
    shortcuts[ShortcutId::ToggleTheme] = { K('t', ctrl, 0), nullptr };
    shortcuts[ShortcutId::ToggleFullscreen] = { K(K::F11Key, 0, 0), nullptr };

    // Canvas
    shortcuts[ShortcutId::BringToFront]    = { K(']', ctrl, 0), nullptr };
    shortcuts[ShortcutId::SendToBack]      = { K('[', ctrl, 0), nullptr };
    shortcuts[ShortcutId::LockSelected]    = { K('l', ctrl, 0), nullptr };
    shortcuts[ShortcutId::GroupSelected]   = { K('g', ctrl | juce::ModifierKeys::altModifier, 0), nullptr };
    shortcuts[ShortcutId::UngroupSelected] = { K('g', ctrlShift | juce::ModifierKeys::altModifier, 0), nullptr };

    shortcuts[ShortcutId::AlignLeft]    = { K(0, 0, 0), nullptr };   // unbound
    shortcuts[ShortcutId::AlignRight]   = { K(0, 0, 0), nullptr };
    shortcuts[ShortcutId::AlignTop]     = { K(0, 0, 0), nullptr };
    shortcuts[ShortcutId::AlignBottom]  = { K(0, 0, 0), nullptr };
    shortcuts[ShortcutId::AlignCenterH] = { K(0, 0, 0), nullptr };
    shortcuts[ShortcutId::AlignCenterV] = { K(0, 0, 0), nullptr };
    shortcuts[ShortcutId::DistributeH]  = { K(0, 0, 0), nullptr };
    shortcuts[ShortcutId::DistributeV]  = { K(0, 0, 0), nullptr };

    // Transport
    shortcuts[ShortcutId::PlayPause] = { K(K::spaceKey, 0, 0), nullptr };
    shortcuts[ShortcutId::Stop]      = { K(K::escapeKey, 0, 0), nullptr };
    shortcuts[ShortcutId::Rewind]    = { K(K::homeKey, 0, 0), nullptr };
}

//==============================================================================
void KeyboardShortcutManager::setBinding(ShortcutId id, const juce::KeyPress& key)
{
    shortcuts[id].key = key;
}

void KeyboardShortcutManager::setAction(ShortcutId id, std::function<void()> action)
{
    shortcuts[id].action = std::move(action);
}

bool KeyboardShortcutManager::handleKeyPress(const juce::KeyPress& key)
{
    for (auto& [id, entry] : shortcuts)
    {
        if (entry.key.isValid() && key == entry.key && entry.action)
        {
            entry.action();
            return true;
        }
    }
    return false;
}

juce::KeyPress KeyboardShortcutManager::getBinding(ShortcutId id) const
{
    auto it = shortcuts.find(id);
    if (it != shortcuts.end()) return it->second.key;
    return {};
}

juce::String KeyboardShortcutManager::getTooltip(ShortcutId id) const
{
    auto desc = getDescription(id);
    auto binding = getBinding(id);
    if (binding.isValid())
        return desc + " (" + binding.getTextDescriptionWithIcons() + ")";
    return desc;
}

//==============================================================================
juce::String KeyboardShortcutManager::getDescription(ShortcutId id)
{
    switch (id)
    {
        case ShortcutId::NewProject:     return "New Project";
        case ShortcutId::OpenProject:    return "Open Project";
        case ShortcutId::SaveProject:    return "Save Project";
        case ShortcutId::SaveProjectAs:  return "Save Project As";
        case ShortcutId::ExportVideo:    return "Export Video";
        case ShortcutId::BatchExport:    return "Batch Export";

        case ShortcutId::Undo:           return "Undo";
        case ShortcutId::Redo:           return "Redo";
        case ShortcutId::Cut:            return "Cut";
        case ShortcutId::Copy:           return "Copy";
        case ShortcutId::Paste:          return "Paste";
        case ShortcutId::Duplicate:      return "Duplicate";
        case ShortcutId::Delete:         return "Delete";
        case ShortcutId::SelectAll:      return "Select All";

        case ShortcutId::ZoomIn:         return "Zoom In";
        case ShortcutId::ZoomOut:        return "Zoom Out";
        case ShortcutId::ZoomToFit:      return "Zoom to Fit";
        case ShortcutId::ZoomReset:      return "Zoom 100%";
        case ShortcutId::ToggleGrid:     return "Toggle Grid";
        case ShortcutId::ToggleSnap:     return "Toggle Snap";
        case ShortcutId::ToggleTheme:    return "Toggle Theme";
        case ShortcutId::ToggleFullscreen: return "Toggle Fullscreen";

        case ShortcutId::BringToFront:   return "Bring to Front";
        case ShortcutId::SendToBack:     return "Send to Back";
        case ShortcutId::LockSelected:   return "Lock / Unlock";
        case ShortcutId::GroupSelected:   return "Group";
        case ShortcutId::UngroupSelected: return "Ungroup";
        case ShortcutId::AlignLeft:      return "Align Left";
        case ShortcutId::AlignRight:     return "Align Right";
        case ShortcutId::AlignTop:       return "Align Top";
        case ShortcutId::AlignBottom:    return "Align Bottom";
        case ShortcutId::AlignCenterH:   return "Align Center H";
        case ShortcutId::AlignCenterV:   return "Align Center V";
        case ShortcutId::DistributeH:    return "Distribute Horizontally";
        case ShortcutId::DistributeV:    return "Distribute Vertically";

        case ShortcutId::PlayPause:      return "Play / Pause";
        case ShortcutId::Stop:           return "Stop";
        case ShortcutId::Rewind:         return "Rewind";
        default:                          return "Unknown";
    }
}
