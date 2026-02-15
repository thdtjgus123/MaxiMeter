#pragma once

#include <JuceHeader.h>
#include "CanvasItem.h"
#include "../UI/FontAwesomeIcons.h"
#include "../UI/ThemeManager.h"

//==============================================================================
/// Drag description ID used when dragging a meter type from the toolbox.
/// The var payload is the integer value of the MeterType enum.
static const juce::String kToolboxDragDescription = "ToolboxMeterDrag";

//==============================================================================
/// Sidebar panel listing available meter types.  User clicks a item to add it
/// at canvas centre, or drags it onto the canvas at the drop position.
class CanvasToolbox : public juce::Component,
                      public ThemeManager::Listener
{
public:
    enum class ViewMode { List, Grid, Compact };

    CanvasToolbox();
    ~CanvasToolbox() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /// Callback when user clicks a meter type to add (single-click).
    std::function<void(MeterType)> onMeterSelected;

    /// Callback when user drags a meter type onto the canvas (drop).
    /// The Point is in screen coordinates — the receiver converts to canvas space.
    std::function<void(MeterType, juce::Point<int>)> onMeterDragged;

    /// Callback when user clicks a custom plugin to add.
    std::function<void(const juce::String& pluginName)> onCustomPluginSelected;

    // ThemeManager::Listener
    void themeChanged(AppTheme newTheme) override;

    /// Set the toolbox display mode programmatically.
    void setViewMode(ViewMode mode) { viewMode = mode; resized(); repaint(); }

    /// Rescan the CustomComponents/plugins/ directory and refresh the list.
    void refreshCustomPlugins();

private:
    //==========================================================================
    /// Individual list cell for a single MeterType.  Supports click-to-add
    /// and drag-and-drop onto the canvas.
    class ToolboxItem : public juce::Component
    {
    public:
        ToolboxItem(MeterType t, CanvasToolbox& owner)
            : type(t), toolbox(owner) {}

        void paint(juce::Graphics& g) override
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            bool hover = isMouseOver();
            g.setColour(hover ? pal.toolboxItemHover : pal.toolboxItem);
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2), 4.0f);

            auto mode = toolbox.viewMode;

            if (mode == ViewMode::Grid)
            {
                // Grid: icon on top, name below
                auto iconArea = getLocalBounds().reduced(6).removeFromTop(24).toFloat();
                auto icon = FontAwesomeIcons::iconForMeterType(type);
                FontAwesomeIcons::drawIcon(g, icon, iconArea.withSizeKeepingCentre(20, 20), pal.accent.withAlpha(0.8f));

                g.setColour(pal.buttonText);
                g.setFont(9.0f);
                auto nameArea = getLocalBounds().reduced(2).removeFromBottom(getHeight() - 30);
                g.drawText(meterTypeName(type), nameArea,
                           juce::Justification::centredTop, true);
            }
            else if (mode == ViewMode::Compact)
            {
                // Compact: icon + short name, no dimensions
                auto iconArea = getLocalBounds().reduced(4, 2).removeFromLeft(16).toFloat();
                auto icon = FontAwesomeIcons::iconForMeterType(type);
                FontAwesomeIcons::drawIcon(g, icon, iconArea, pal.accent.withAlpha(0.8f));

                auto textArea = getLocalBounds().reduced(22, 0).withTrimmedRight(4);
                g.setColour(pal.buttonText);
                g.setFont(10.0f);
                g.drawText(meterTypeName(type), textArea,
                           juce::Justification::centredLeft, true);
            }
            else
            {
                // List (default): icon + name + dimensions
                auto iconArea = getLocalBounds().reduced(6, 6).removeFromLeft(20).toFloat();
                auto icon = FontAwesomeIcons::iconForMeterType(type);
                FontAwesomeIcons::drawIcon(g, icon, iconArea, pal.accent.withAlpha(0.8f));

                auto textArea = getLocalBounds().reduced(30, 0).withTrimmedRight(50);
                g.setColour(pal.buttonText);
                g.setFont(12.0f);
                g.drawText(meterTypeName(type), textArea,
                           juce::Justification::centredLeft);

                auto sz = meterDefaultSize(type);
                juce::String dim = juce::String(sz.getWidth()) + "x" + juce::String(sz.getHeight());
                g.setColour(pal.dimText);
                g.setFont(10.0f);
                g.drawText(dim, getLocalBounds().reduced(8, 0), juce::Justification::centredRight);
            }
        }

        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { repaint(); }

        void mouseDown(const juce::MouseEvent&) override
        {
            dragged = false;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!dragged && e.getDistanceFromDragStart() > 5)
            {
                dragged = true;

                // Find the DragAndDropContainer ancestor (MainComponent)
                if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
                {
                    auto& pal = ThemeManager::getInstance().getPalette();
                    // Create a small drag-image showing the meter name
                    juce::Image dragImage(juce::Image::ARGB, 160, 28, true);
                    {
                        juce::Graphics ig(dragImage);
                        ig.setColour(pal.accent.withAlpha(0.8f));
                        ig.fillRoundedRectangle(dragImage.getBounds().toFloat(), 4.0f);
                        ig.setColour(juce::Colours::white);
                        ig.setFont(12.0f);
                        ig.drawText(meterTypeName(type), dragImage.getBounds().reduced(6, 0),
                                    juce::Justification::centredLeft);
                    }

                    // The description carries the meter type index as an int
                    container->startDragging(juce::var(static_cast<int>(type)),
                                             this, dragImage, true);
                }
            }
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            // Only fire the click callback if we didn't start a drag
            if (!dragged && getLocalBounds().contains(e.getPosition()))
                if (toolbox.onMeterSelected)
                    toolbox.onMeterSelected(type);
        }

        MeterType type;
        CanvasToolbox& toolbox;
        bool dragged = false;
    };

    //==========================================================================
    /// Individual list cell for a custom Python plugin component.
    /// Supports click-to-add and right-click context menu (Edit / Delete).
    class CustomPluginItem : public juce::Component
    {
    public:
        CustomPluginItem(const juce::File& file, CanvasToolbox& owner)
            : pluginFile(file), toolbox(owner)
        {
            displayName = file.getFileNameWithoutExtension()
                              .replace("_", " ");
            // Capitalize first letter of each word
            auto words = juce::StringArray::fromTokens(displayName, " ", "");
            for (auto& w : words)
                if (w.isNotEmpty())
                    w = w.substring(0, 1).toUpperCase() + w.substring(1);
            displayName = words.joinIntoString(" ");
        }

        void paint(juce::Graphics& g) override
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            bool hover = isMouseOver();
            g.setColour(hover ? pal.toolboxItemHover : pal.toolboxItem);
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2), 4.0f);

            auto mode = toolbox.viewMode;

            if (mode == ViewMode::Grid)
            {
                auto iconArea = getLocalBounds().reduced(6).removeFromTop(24).toFloat();
                g.setColour(juce::Colour(0xFF4EC9B0));
                g.setFont(14.0f);
                g.drawText("Py", iconArea.withSizeKeepingCentre(20, 20), juce::Justification::centred);

                g.setColour(pal.buttonText);
                g.setFont(9.0f);
                auto nameArea = getLocalBounds().reduced(2).removeFromBottom(getHeight() - 30);
                g.drawText(displayName, nameArea, juce::Justification::centredTop, true);
            }
            else if (mode == ViewMode::Compact)
            {
                auto iconArea = getLocalBounds().reduced(4, 2).removeFromLeft(16).toFloat();
                g.setColour(juce::Colour(0xFF4EC9B0));
                g.setFont(11.0f);
                g.drawText("Py", iconArea, juce::Justification::centred);

                auto textArea = getLocalBounds().reduced(22, 0).withTrimmedRight(4);
                g.setColour(pal.buttonText);
                g.setFont(10.0f);
                g.drawText(displayName, textArea, juce::Justification::centredLeft, true);
            }
            else
            {
                // Python icon (code brackets)
                auto iconArea = getLocalBounds().reduced(6, 6).removeFromLeft(20).toFloat();
                g.setColour(juce::Colour(0xFF4EC9B0));
                g.setFont(14.0f);
                g.drawText("Py", iconArea, juce::Justification::centred);

                // Plugin display name
                auto textArea = getLocalBounds().reduced(30, 0).withTrimmedRight(10);
                g.setColour(pal.buttonText);
                g.setFont(12.0f);
                g.drawText(displayName, textArea, juce::Justification::centredLeft);
            }
        }

        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { repaint(); }

        void mouseDown(const juce::MouseEvent& e) override
        {
            dragged = false;

            if (e.mods.isPopupMenu())
            {
                showContextMenu();
                return;
            }
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            (void) e;
            // Could add drag support later if needed
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return; // already handled in mouseDown

            if (!dragged && getLocalBounds().contains(e.getPosition()))
                if (toolbox.onCustomPluginSelected)
                    toolbox.onCustomPluginSelected(pluginFile.getFileNameWithoutExtension());
        }

        juce::File pluginFile;
        juce::String displayName;
        CanvasToolbox& toolbox;
        bool dragged = false;

    private:
        void showContextMenu()
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Edit in Editor");
            menu.addSeparator();
            menu.addItem(2, "Delete Component");

            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this](int result)
                {
                    if (result == 1)
                        editPlugin();
                    else if (result == 2)
                        deletePlugin();
                });
        }

        void editPlugin()
        {
            // Try VS Code, fall back to system default
#if JUCE_WINDOWS
            juce::ChildProcess vsCodeProc;
            if (!vsCodeProc.start("code \"" + pluginFile.getFullPathName() + "\""))
                pluginFile.startAsProcess();
#else
            juce::ChildProcess proc;
            if (!proc.start("code \"" + pluginFile.getFullPathName() + "\""))
                pluginFile.startAsProcess();
#endif
        }

        void deletePlugin()
        {
            auto fileName = pluginFile.getFileName();
            auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Delete Component")
                .withMessage("Are you sure you want to delete \"" + fileName + "\"?\n\nThis cannot be undone.")
                .withButton("Delete")
                .withButton("Cancel");

            juce::AlertWindow::showAsync(options,
                [this](int result)
                {
                    if (result == 1) // "Delete" button
                    {
                        pluginFile.deleteFile();
                        // Refresh the list
                        toolbox.refreshCustomPlugins();
                    }
                });
        }
    };

    std::vector<std::unique_ptr<ToolboxItem>>      toolItems;
    std::vector<std::unique_ptr<CustomPluginItem>>  customItems;
    static constexpr int kItemHeight = 32;
    static constexpr int kGridCellSize = 70;
    static constexpr int kCompactHeight = 22;
    static constexpr int kAddButtonHeight = 36;
    static constexpr int kSectionHeaderHeight = 22;

    ViewMode viewMode = ViewMode::List;
    juce::TextButton viewModeBtn;

    // Scrollable container
    juce::Viewport viewport;
    juce::Component itemContainer;

    // "CUSTOM" section header label
    juce::Label customSectionLabel;

    // "+ New Component" button at bottom
    juce::TextButton addComponentBtn;
    void createNewCustomComponent();

    /// Get the plugins directory path
    juce::File getPluginsDirectory() const;
};
