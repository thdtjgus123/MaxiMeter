#include "MainWindow.h"
#include "../MainComponent.h"
#include "../Project/PresetTemplates.h"
#include "ThemeManager.h"

//==============================================================================
MainWindow::MainWindow(const juce::String& name)
    : DocumentWindow(name,
                     juce::Colour(0xFF16213E),
                     DocumentWindow::allButtons)
{
    // Apply our custom LookAndFeel for skinned title bar buttons
    setLookAndFeel(&titleBarLookAndFeel);

    // Use custom (non-native) title bar so we can skin the -□X buttons
    setUsingNativeTitleBar(false);
    setTitleBarHeight(32);

    setContentOwned(new MainComponent(), true);

    // Menu bar
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
#else
    setMenuBar(this);
#endif

    // Register for theme changes
    ThemeManager::getInstance().addListener(this);

    setResizable(true, true);
    setResizeLimits(800, 500, 3840, 2160);
    centreWithSize(1440, 900);
    setVisible(true);

    // Apply initial theme colors
    themeChanged(ThemeManager::getInstance().getCurrentTheme());
}

MainWindow::~MainWindow()
{
    ThemeManager::getInstance().removeListener(this);
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#else
    setMenuBar(nullptr);
#endif
    setLookAndFeel(nullptr);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

void MainWindow::themeChanged(AppTheme /*newTheme*/)
{
    titleBarLookAndFeel.updateColours();
    auto& pal = ThemeManager::getInstance().getPalette();
    setBackgroundColour(pal.windowBg);
    repaint();
}

//==============================================================================
juce::StringArray MainWindow::getMenuBarNames()
{
    return { "File", "Edit", "View", "Debug", "Help" };
}

juce::PopupMenu MainWindow::getMenuForIndex(int menuIndex, const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(cmdNewProject,    "New Project\tCtrl+N");
        menu.addItem(cmdOpenProject,   "Open Project...\tCtrl+O");
        menu.addItem(cmdSaveProject,   "Save Project\tCtrl+S");
        menu.addItem(cmdSaveProjectAs, "Save Project As...\tCtrl+Shift+S");
        menu.addSeparator();
        menu.addItem(cmdOpenAudioFile, "Open Audio File...");
        menu.addItem(cmdOpenSkinFile,  "Open Skin File (.wsz)...");
        menu.addSeparator();
        menu.addItem(cmdSettings,      "Settings...");
        menu.addItem(cmdExportVideo,   "Export Video...\tCtrl+E");
        menu.addSeparator();
        menu.addItem(cmdImportComponent, "Import Component...");
        menu.addSeparator();
        menu.addItem(cmdQuit,          "Quit");
    }
    else if (menuIndex == 1) // Edit
    {
        menu.addItem(cmdUndo,      "Undo\tCtrl+Z");
        menu.addItem(cmdRedo,      "Redo\tCtrl+Y");
        menu.addSeparator();
        menu.addItem(cmdSelectAll, "Select All\tCtrl+A");
    }
    else if (menuIndex == 2) // View
    {
        auto& theme = ThemeManager::getInstance();
        bool isDark = (theme.getCurrentTheme() == AppTheme::Dark);
        menu.addItem(cmdToggleTheme, isDark ? "Switch to Light Theme" : "Switch to Dark Theme");
        menu.addSeparator();
        menu.addItem(cmdToggleGrid, "Toggle Grid\tCtrl+G");
        menu.addSeparator();
        menu.addItem(cmdZoomIn,    "Zoom In\tCtrl+=");
        menu.addItem(cmdZoomOut,   "Zoom Out\tCtrl+-");
        menu.addItem(cmdZoomReset, "Zoom 100%\tCtrl+1");
    }
    else if (menuIndex == 3) // Debug
    {
        menu.addItem(cmdDebugLog,            "Show Debug Log...");
        menu.addSeparator();
        menu.addItem(cmdDebugTestConnection,  "Test Bridge Connection");
        menu.addItem(cmdDebugRestartBridge,   "Restart Python Bridge");
        menu.addItem(cmdDebugRescan,          "Re-scan Plugins");
    }
    else if (menuIndex == 4) // Help
    {
        menu.addItem(cmdDocumentation, "Documentation\tF1");
        menu.addSeparator();
        menu.addItem(cmdAbout, "About MaxiMeter");
    }

    return menu;
}

void MainWindow::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    auto* mc = getMainComponent();
    if (!mc) return;

    // Check if it's a preset
    if (menuItemID >= cmdPresetBase && menuItemID < cmdPresetEnd)
    {
        mc->loadPresetTemplate(menuItemID - cmdPresetBase);
        return;
    }

    switch (menuItemID)
    {
        // ── File ───────────────────────────────────────────────────────────
        case cmdNewProject:
            mc->newProject();
            break;

        case cmdOpenProject:
            mc->openProject();
            break;

        case cmdSaveProject:
            mc->saveProject();
            break;

        case cmdSaveProjectAs:
            mc->saveProjectAs();
            break;

        case cmdOpenAudioFile:
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Open Audio File", juce::File{},
                "*.wav;*.mp3;*.flac;*.ogg;*.aiff;*.aif;*.wma;*.m4a;*.aac");

            chooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                        if (auto* m = getMainComponent())
                            m->loadAudioFile(file);
                });
            break;
        }

        case cmdOpenSkinFile:
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Open Winamp Skin", juce::File{}, "*.wsz;*.zip");

            chooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                        if (auto* m = getMainComponent())
                            m->loadSkin(file);
                });
            break;
        }

        case cmdSettings:
            mc->showSettings();
            break;

        case cmdExportVideo:
            mc->exportVideo();
            break;

        case cmdQuit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;

        // ── Edit ───────────────────────────────────────────────────────────
        case cmdUndo:
            mc->getCanvasEditor().getModel().undoManager.undo();
            break;

        case cmdRedo:
            mc->getCanvasEditor().getModel().undoManager.redo();
            break;

        case cmdSelectAll:
            mc->getCanvasEditor().getModel().selectAll();
            break;

        // ── View ───────────────────────────────────────────────────────────
        case cmdToggleTheme:
            ThemeManager::getInstance().toggleTheme();
            break;

        case cmdToggleGrid:
        {
            auto& g = mc->getCanvasEditor().getModel().grid;
            g.showGrid = !g.showGrid;
            mc->getCanvasEditor().getModel().notifyZoomPanChanged();
            break;
        }

        case cmdZoomIn:
        {
            auto& m = mc->getCanvasEditor().getModel();
            m.setZoom(m.zoom * 1.25f);
            break;
        }

        case cmdZoomOut:
        {
            auto& m = mc->getCanvasEditor().getModel();
            m.setZoom(m.zoom * 0.8f);
            break;
        }

        case cmdZoomReset:
            mc->getCanvasEditor().getModel().setZoom(1.0f);
            break;

        // ── File (Import Component) ──────────────────────────────────────
        case cmdImportComponent:
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Import Custom Component", juce::File{},
                "*.py");

            chooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, chooser](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                    {
                        // Copy the .py plugin to the CustomComponents/plugins/ directory
                        auto pluginsDir = juce::File::getSpecialLocation(
                            juce::File::currentExecutableFile).getParentDirectory()
                            .getChildFile("CustomComponents").getChildFile("plugins");
                        if (!pluginsDir.isDirectory())
                            pluginsDir.createDirectory();

                        auto dest = pluginsDir.getChildFile(file.getFileName());
                        auto doCopy = [dest, file]()
                        {
                            if (file.copyFileTo(dest))
                            {
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::InfoIcon,
                                    "Component Imported",
                                    "'" + file.getFileNameWithoutExtension() +
                                    "' has been imported successfully.\n"
                                    "It will appear in the TOOLBOX after restart.",
                                    "OK");
                            }
                            else
                            {
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    "Import Failed",
                                    "Could not copy the component file to the plugins directory.",
                                    "OK");
                            }
                        };

                        if (dest.existsAsFile())
                        {
                            auto opts = juce::MessageBoxOptions()
                                .withIconType(juce::MessageBoxIconType::WarningIcon)
                                .withTitle("Overwrite?")
                                .withMessage("A component with the name '" + file.getFileName() +
                                             "' already exists.\nDo you want to replace it?")
                                .withButton("Replace")
                                .withButton("Cancel");
                            juce::AlertWindow::showAsync(opts,
                                [doCopy](int result) { if (result == 1) doCopy(); });
                            return;
                        }

                        if (file.copyFileTo(dest))
                        {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::InfoIcon,
                                "Component Imported",
                                "'" + file.getFileNameWithoutExtension() +
                                "' has been imported successfully.\n"
                                "It will appear in the TOOLBOX after restart.",
                                "OK");
                        }
                        else
                        {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::MessageBoxIconType::WarningIcon,
                                "Import Failed",
                                "Could not copy the component file to the plugins directory.",
                                "OK");
                        }
                    }
                });
            break;
        }

        // ── Debug ──────────────────────────────────────────────────────────
        case cmdDebugLog:
            if (!debugLogWindow)
                debugLogWindow = std::make_unique<DebugLogWindow>();
            debugLogWindow->setVisible(true);
            debugLogWindow->toFront(true);
            break;

        case cmdDebugRestartBridge:
        {
            MAXIMETER_LOG("BRIDGE", "Manual restart requested from Debug menu");
            auto& bridge = PythonPluginBridge::getInstance();
            bridge.stop();
            auto exeDir = juce::File::getSpecialLocation(
                juce::File::currentExecutableFile).getParentDirectory();
            auto pluginsDir = exeDir.getChildFile("CustomComponents").getChildFile("plugins");
            bridge.start(pluginsDir);
            break;
        }

        case cmdDebugRescan:
            MAXIMETER_LOG("BRIDGE", "Manual re-scan requested from Debug menu");
            PythonPluginBridge::getInstance().scanPlugins();
            break;

        case cmdDebugTestConnection:
        {
            // Open the debug log window and run the test
            if (!debugLogWindow)
                debugLogWindow = std::make_unique<DebugLogWindow>();
            debugLogWindow->setVisible(true);
            debugLogWindow->toFront(true);
            // The DebugLogContent has a testBridgeConnection button;
            // here we trigger the test directly via log entries.
            auto& bridge = PythonPluginBridge::getInstance();
            MAXIMETER_LOG("BRIDGE", "--- Quick Connection Test ---");
            MAXIMETER_LOG("BRIDGE", "isRunning() = " + juce::String(bridge.isRunning() ? "true" : "false"));
            auto manifests = bridge.getAvailablePlugins();
            MAXIMETER_LOG("BRIDGE", "Cached manifests: " + juce::String((int)manifests.size()));
            for (auto& m : manifests)
                MAXIMETER_LOG("BRIDGE", "  -> " + m.name + " [" + m.id + "]");
            break;
        }

        // ── Help ───────────────────────────────────────────────────────────
        case cmdDocumentation:
            if (!docWindow)
                docWindow = std::make_unique<DocumentationWindow>();
            docWindow->setVisible(true);
            docWindow->toFront(true);
            break;

        case cmdAbout:
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "About MaxiMeter",
                "MaxiMeter v0.1.0\n\n"
                "The Ultimate Winamp-Styled Audio Visualization Studio\n\n"
                "Where nostalgia meets professional audio visualization.",
                "OK");
            break;

        default: break;
    }
}

MainComponent* MainWindow::getMainComponent() const
{
    return dynamic_cast<MainComponent*>(getContentComponent());
}
