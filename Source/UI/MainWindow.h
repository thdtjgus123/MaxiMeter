#pragma once

#include <JuceHeader.h>
#include "ThemeManager.h"
#include "SkinnedTitleBarLookAndFeel.h"
#include "DocumentationWindow.h"
#include "DebugLogWindow.h"

class MainComponent;

//==============================================================================
//==============================================================================
/// MainWindow — top-level window with custom skinned title bar and menu bar.
class MainWindow : public juce::DocumentWindow,
                   public juce::MenuBarModel,
                   public ThemeManager::Listener
{
public:
    explicit MainWindow(const juce::String& name);
    ~MainWindow() override;

    void closeButtonPressed() override;

    //--- MenuBarModel ---
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // ThemeManager::Listener
    void themeChanged(AppTheme newTheme) override;

    MainComponent* getMainComponent() const;

private:
    SkinnedTitleBarLookAndFeel titleBarLookAndFeel;

    enum CommandIDs
    {
        // File
        cmdNewProject = 1,
        cmdOpenProject,
        cmdSaveProject,
        cmdSaveProjectAs,
        cmdOpenAudioFile,
        cmdOpenSkinFile,
        cmdSettings,         // New Settings command
        cmdExportVideo,
        cmdImportComponent,
        cmdQuit,

        // Edit

        // Edit
        cmdUndo,
        cmdRedo,
        cmdSelectAll,

        // View
        cmdToggleTheme,
        cmdToggleGrid,
        cmdZoomIn,
        cmdZoomOut,
        cmdZoomReset,

        // Presets (base ID — actual IDs are cmdPresetBase + index)
        cmdPresetBase = 200,
        cmdPresetEnd  = 220,

        // Debug
        cmdDebugLog = 400,
        cmdDebugRestartBridge,
        cmdDebugRescan,
        cmdDebugTestConnection,

        // Help
        cmdDocumentation = 500,
        cmdAbout
    };

    std::unique_ptr<DocumentationWindow> docWindow;
    std::unique_ptr<DebugLogWindow> debugLogWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};
