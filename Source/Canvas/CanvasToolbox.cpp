#include "CanvasToolbox.h"

//==============================================================================
CanvasToolbox::CanvasToolbox()
{
    ThemeManager::getInstance().addListener(this);

    viewport.setViewedComponent(&itemContainer, false);
    viewport.setScrollBarsShown(true, false); // vertical only
    addAndMakeVisible(viewport);

    for (int i = 0; i < static_cast<int>(MeterType::NumTypes); ++i)
    {
        auto mt = static_cast<MeterType>(i);
        auto item = std::make_unique<ToolboxItem>(mt, *this);
        itemContainer.addAndMakeVisible(item.get());
        toolItems.push_back(std::move(item));
    }

    // Scan for custom plugins
    refreshCustomPlugins();

    // "CUSTOM" section label (inside itemContainer)
    customSectionLabel.setText("CUSTOM", juce::dontSendNotification);
    customSectionLabel.setFont(juce::Font(10.0f));
    customSectionLabel.setJustificationType(juce::Justification::centredLeft);
    customSectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    itemContainer.addAndMakeVisible(customSectionLabel);

    // "+ New Component" button at the bottom
    addComponentBtn.setButtonText("+ New Component");
    addComponentBtn.onClick = [this]() { createNewCustomComponent(); };
    addAndMakeVisible(addComponentBtn);

    // View mode toggle button
    viewModeBtn.setButtonText("List");
    viewModeBtn.setTooltip("Switch view: List / Grid / Compact");
    viewModeBtn.onClick = [this]()
    {
        if (viewMode == ViewMode::List)
            viewMode = ViewMode::Grid;
        else if (viewMode == ViewMode::Grid)
            viewMode = ViewMode::Compact;
        else
            viewMode = ViewMode::List;

        switch (viewMode)
        {
            case ViewMode::List:    viewModeBtn.setButtonText("List"); break;
            case ViewMode::Grid:    viewModeBtn.setButtonText("Grid"); break;
            case ViewMode::Compact: viewModeBtn.setButtonText("Compact"); break;
        }
        resized();
        repaint();
        for (auto& item : toolItems) item->repaint();
        for (auto& ci : customItems) ci->repaint();
    };
    addAndMakeVisible(viewModeBtn);
}

CanvasToolbox::~CanvasToolbox()
{
    ThemeManager::getInstance().removeListener(this);
}

juce::File CanvasToolbox::getPluginsDirectory() const
{
    return juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory()
        .getChildFile("CustomComponents").getChildFile("plugins");
}

void CanvasToolbox::refreshCustomPlugins()
{
    // Remove old custom items from the container
    for (auto& ci : customItems)
        itemContainer.removeChildComponent(ci.get());
    customItems.clear();

    // Scan plugins directory
    auto pluginsDir = getPluginsDirectory();
    if (pluginsDir.isDirectory())
    {
        auto files = pluginsDir.findChildFiles(
            juce::File::findFiles, false, "*.py");

        // Sort alphabetically
        files.sort();

        for (auto& f : files)
        {
            // Skip __init__.py and files starting with _
            if (f.getFileName().startsWith("_"))
                continue;

            auto item = std::make_unique<CustomPluginItem>(f, *this);
            itemContainer.addAndMakeVisible(item.get());
            customItems.push_back(std::move(item));
        }
    }

    resized();
    repaint();
}

void CanvasToolbox::paint(juce::Graphics& g)
{
    auto& pal = ThemeManager::getInstance().getPalette();
    g.fillAll(pal.toolboxBg);

    g.setColour(pal.headerText);
    g.setFont(11.0f);
    g.drawText("TOOLBOX", getLocalBounds().removeFromTop(24).reduced(8, 0).withTrimmedRight(60),
               juce::Justification::centredLeft);
}

void CanvasToolbox::resized()
{
    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(26);

    // View mode button in the header area (right side)
    viewModeBtn.setBounds(headerArea.removeFromRight(60).reduced(4, 3));

    // Reserve space for the "+ New Component" button at the bottom
    auto btnArea = area.removeFromBottom(kAddButtonHeight);
    addComponentBtn.setBounds(btnArea.reduced(6, 4));

    viewport.setBounds(area);

    int w = viewport.getMaximumVisibleWidth();

    if (viewMode == ViewMode::Grid)
    {
        // Grid layout: items in a wrapping grid
        int cellW = kGridCellSize;
        int cellH = kGridCellSize;
        int cols = juce::jmax(1, (w - 4) / cellW);
        int x = 2, y = 0;
        int col = 0;

        for (auto& item : toolItems)
        {
            item->setBounds(x + col * cellW, y, cellW, cellH);
            col++;
            if (col >= cols) { col = 0; y += cellH + 2; }
        }
        if (col != 0) { y += cellH + 2; col = 0; }

        // Custom section
        if (!customItems.empty())
        {
            y += 4;
            customSectionLabel.setBounds(8, y, w - 16, kSectionHeaderHeight);
            customSectionLabel.setVisible(true);
            y += kSectionHeaderHeight;

            for (auto& ci : customItems)
            {
                ci->setBounds(x + col * cellW, y, cellW, cellH);
                col++;
                if (col >= cols) { col = 0; y += cellH + 2; }
            }
            if (col != 0) y += cellH + 2;
        }
        else
        {
            customSectionLabel.setVisible(false);
        }

        itemContainer.setSize(w, y);
    }
    else if (viewMode == ViewMode::Compact)
    {
        // Compact: smaller rows
        int y = 0;
        for (auto& item : toolItems)
        {
            item->setBounds(2, y, w - 4, kCompactHeight);
            y += kCompactHeight + 1;
        }

        if (!customItems.empty())
        {
            y += 4;
            customSectionLabel.setBounds(8, y, w - 16, kSectionHeaderHeight);
            customSectionLabel.setVisible(true);
            y += kSectionHeaderHeight;

            for (auto& ci : customItems)
            {
                ci->setBounds(2, y, w - 4, kCompactHeight);
                y += kCompactHeight + 1;
            }
        }
        else
        {
            customSectionLabel.setVisible(false);
        }

        itemContainer.setSize(w, y);
    }
    else
    {
        // List (default)
        int y = 0;
        for (auto& item : toolItems)
        {
            item->setBounds(2, y, w - 4, kItemHeight);
            y += kItemHeight + 2;
        }

        if (!customItems.empty())
        {
            y += 6;
            customSectionLabel.setBounds(8, y, w - 16, kSectionHeaderHeight);
            customSectionLabel.setVisible(true);
            y += kSectionHeaderHeight;

            for (auto& ci : customItems)
            {
                ci->setBounds(2, y, w - 4, kItemHeight);
                y += kItemHeight + 2;
            }
        }
        else
        {
            customSectionLabel.setVisible(false);
        }

        itemContainer.setSize(w, y);
    }
}

void CanvasToolbox::themeChanged(AppTheme /*newTheme*/)
{
    repaint();
    for (auto& item : toolItems)
        item->repaint();
    for (auto& ci : customItems)
        ci->repaint();
}

//==============================================================================
void CanvasToolbox::createNewCustomComponent()
{
    // Build plugins directory path relative to the executable
    auto pluginsDir = getPluginsDirectory();

    // Create directory if it doesn't exist
    if (!pluginsDir.isDirectory())
        pluginsDir.createDirectory();

    // Generate a unique filename: my_component_01.py, my_component_02.py, ...
    juce::String baseName = "my_component";
    juce::File newFile;
    for (int i = 1; i < 1000; ++i)
    {
        auto name = baseName + "_" + juce::String(i).paddedLeft('0', 2) + ".py";
        newFile = pluginsDir.getChildFile(name);
        if (!newFile.existsAsFile())
            break;
    }

    // Python template with basic example
    juce::String templateCode;
    templateCode
        << "\"\"\"" << juce::newLine
        << "Custom Component: " << newFile.getFileNameWithoutExtension() << juce::newLine
        << "A template for creating your own MaxiMeter custom component." << juce::newLine
        << "\"\"\"" << juce::newLine
        << juce::newLine
        << "from __future__ import annotations" << juce::newLine
        << juce::newLine
        << "from CustomComponents import (" << juce::newLine
        << "    BaseComponent, Manifest, Category," << juce::newLine
        << "    Property, PropertyType," << juce::newLine
        << "    RenderContext, AudioData," << juce::newLine
        << "    Color, Gradient, GradientStop," << juce::newLine
        << "    Font, TextAlign," << juce::newLine
        << ")" << juce::newLine
        << juce::newLine
        << juce::newLine
        << "class MyComponent(BaseComponent):" << juce::newLine
        << "    \"\"\"A simple custom audio visualizer.\"\"\"" << juce::newLine
        << juce::newLine
        << "    @staticmethod" << juce::newLine
        << "    def get_manifest() -> Manifest:" << juce::newLine
        << "        return Manifest(" << juce::newLine
        << "            id=\"com.maximeter.custom." << newFile.getFileNameWithoutExtension() << "\"," << juce::newLine
        << "            name=\"My Custom Component\"," << juce::newLine
        << "            version=\"1.0.0\"," << juce::newLine
        << "            author=\"Your Name\"," << juce::newLine
        << "            description=\"Describe your component here.\"," << juce::newLine
        << "            category=Category.METER," << juce::newLine
        << "            default_size=(200, 200)," << juce::newLine
        << "            min_size=(80, 80)," << juce::newLine
        << "            tags=(\"custom\", \"example\")," << juce::newLine
        << "        )" << juce::newLine
        << juce::newLine
        << "    def get_properties(self):" << juce::newLine
        << "        return [" << juce::newLine
        << "            Property(\"bar_color\", \"Bar Color\", PropertyType.COLOR," << juce::newLine
        << "                     default=Color(0x3A, 0x7B, 0xFF), group=\"Appearance\")," << juce::newLine
        << "            Property(\"bg_color\", \"Background\", PropertyType.COLOR," << juce::newLine
        << "                     default=Color(0x12, 0x12, 0x1A), group=\"Appearance\")," << juce::newLine
        << "        ]" << juce::newLine
        << juce::newLine
        << "    def on_init(self):" << juce::newLine
        << "        \"\"\"Called once when the component is created.\"\"\"" << juce::newLine
        << "        self.state.set(\"smooth_level\", 0.0)" << juce::newLine
        << juce::newLine
        << "    def on_render(self, ctx: RenderContext, audio: AudioData):" << juce::newLine
        << "        \"\"\"Called every frame to render the component.\"\"\"" << juce::newLine
        << "        bg = self.get_property(\"bg_color\", Color(0x12, 0x12, 0x1A))" << juce::newLine
        << "        bar_col = self.get_property(\"bar_color\", Color(0x3A, 0x7B, 0xFF))" << juce::newLine
        << juce::newLine
        << "        # Clear background" << juce::newLine
        << "        ctx.clear(bg)" << juce::newLine
        << juce::newLine
        << "        w, h = ctx.width, ctx.height" << juce::newLine
        << "        margin = 10" << juce::newLine
        << juce::newLine
        << "        # Smooth the audio level" << juce::newLine
        << "        smooth = self.state.get(\"smooth_level\", 0.0)" << juce::newLine
        << "        target = (audio.left.rms_linear + audio.right.rms_linear) / 2.0" << juce::newLine
        << "        smooth += (target - smooth) * 0.15" << juce::newLine
        << "        self.state.set(\"smooth_level\", smooth)" << juce::newLine
        << juce::newLine
        << "        # Draw a simple level bar (bottom-up)" << juce::newLine
        << "        bar_x = margin" << juce::newLine
        << "        bar_w = w - margin * 2" << juce::newLine
        << "        bar_h = (h - margin * 2) * min(1.0, smooth)" << juce::newLine
        << "        bar_y = h - margin - bar_h" << juce::newLine
        << juce::newLine
        << "        ctx.fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, 4, bar_col)" << juce::newLine
        << juce::newLine
        << "        # Draw label" << juce::newLine
        << "        ctx.draw_text(" << juce::newLine
        << "            \"My Component\", 0, margin, w, 20," << juce::newLine
        << "            Color(200, 200, 200), Font(size=12.0), TextAlign.CENTER," << juce::newLine
        << "        )" << juce::newLine;

    // Write the template to file
    newFile.replaceWithText(templateCode);

    // Refresh the toolbox list immediately
    refreshCustomPlugins();

    // Try to open in VS Code first, fall back to system default editor
#if JUCE_WINDOWS
    // Try VS Code first
    bool opened = false;

    // Try "code" command (VS Code in PATH)
    juce::ChildProcess vsCodeProc;
    if (vsCodeProc.start("code \"" + newFile.getFullPathName() + "\""))
    {
        opened = true;
    }

    if (!opened)
    {
        // Fall back to system default editor via ShellExecute
        newFile.startAsProcess();
    }
#elif JUCE_MAC
    // Try VS Code, fall back to open command
    juce::ChildProcess proc;
    if (!proc.start("code \"" + newFile.getFullPathName() + "\""))
        newFile.startAsProcess();
#else
    // Linux: try code, fall back to xdg-open
    juce::ChildProcess proc;
    if (!proc.start("code \"" + newFile.getFullPathName() + "\""))
        newFile.startAsProcess();
#endif

    // Show confirmation
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "New Component Created",
        "Created: " + newFile.getFileName() + "\n\n"
        "The file has been opened in your code editor.\n"
        "It is now listed under CUSTOM in the Toolbox.",
        "OK");
}
