#include "CanvasEditor.h"
#include "CanvasCommands.h"
#include "CanvasToolbox.h"
#include "CustomPluginComponent.h"
#include "PythonPluginBridge.h"
#include "../UI/Spectrogram.h"
#include "../UI/LevelHistogram.h"
#include "../UI/ThemeManager.h"
#include "../UI/DebugLogWindow.h"
#include "../UI/TextLabelComponent.h"
#include "../UI/MeterBase.h"
#include "../UI/ShapeComponent.h"
#include "../UI/SkinnedTitleBarLookAndFeel.h"

//==============================================================================
CanvasEditor::CanvasEditor(AudioEngine& ae, FFTProcessor& fft,
                           LevelAnalyzer& la, LoudnessAnalyzer& loud,
                           StereoFieldAnalyzer& stereo)
    : canvasView(model),
      propertyPanel(model),
      meterSettings(model),
      layerPanel(model),
      miniMap(model),
      alignToolbar(model),
      meterFactory(ae, fft, la, loud, stereo)
{
    addAndMakeVisible(canvasView);
    addAndMakeVisible(toolbox);
    addAndMakeVisible(propertyPanel);
    addAndMakeVisible(meterSettings);
    addAndMakeVisible(layerPanel);
    addAndMakeVisible(miniMap);
    addAndMakeVisible(alignToolbar);

    // Wire freeze button on alignment toolbar to render preview
    alignToolbar.onFreezeClicked = [this] { showRenderPreview(); };

    canvasView.addListener(this);

    // Wire toolbox: clicking a meter type adds it to the canvas centre
    toolbox.onMeterSelected = [this](MeterType type)
    {
        auto centre = model.screenToCanvas(canvasView.getLocalBounds().getCentre().toFloat());
        addMeter(type, centre);
    };

    // Wire toolbox: clicking a custom plugin adds a CustomPlugin meter
    toolbox.onCustomPluginSelected = [this](const juce::String& pluginName)
    {
        auto centre = model.screenToCanvas(canvasView.getLocalBounds().getCentre().toFloat());
        auto* item = addMeter(MeterType::CustomPlugin, centre);
        if (item && item->component)
        {
            auto& bridge = PythonPluginBridge::getInstance();
            if (!bridge.isRunning())
            {
                auto pluginsDir = juce::File::getSpecialLocation(
                    juce::File::currentExecutableFile).getParentDirectory()
                    .getChildFile("CustomComponents").getChildFile("plugins");
                bridge.start(pluginsDir);
            }

            // Look up actual manifest ID from scanned plugins.
            // Match by: source filename stem, manifest id suffix, or manifest name.
            juce::String manifestId;
            for (auto& m : bridge.getAvailablePlugins())
            {
                if (m.sourceFile.equalsIgnoreCase(pluginName) ||
                    m.id.endsWithIgnoreCase(pluginName) ||
                    m.name.removeCharacters(" ").equalsIgnoreCase(pluginName))
                {
                    manifestId = m.id;
                    break;
                }
            }
            if (manifestId.isEmpty())
                manifestId = "com.maximeter.custom." + pluginName;

            auto instanceId = juce::Uuid().toString();

            item->customPluginId = manifestId;
            item->customInstanceId = instanceId;
            item->name = pluginName.replace("_", " ");

            // Initialize the plugin via bridge
            auto* cpc = static_cast<CustomPluginComponent*>(item->component.get());

            if (bridge.isRunning())
            {
                auto props = bridge.createInstance(manifestId, instanceId);
                if (props.empty())
                {
                    DBG("CanvasEditor: createInstance FAILED for " + manifestId
                        + " — will retry automatically via auto-recovery");
                    MAXIMETER_LOG("ERROR", "createInstance FAILED for " + manifestId + " / " + instanceId);
                }
                else
                {
                    MAXIMETER_LOG("INSTANCE", "createInstance OK for " + manifestId + " / " + instanceId
                        + " (" + juce::String((int)props.size()) + " props)");
                    cpc->setPluginProperties(props);
                }
            }

            cpc->setPluginId(manifestId, instanceId);
        }
    };

    // Wire layer panel divider drag
    layerPanel.onDividerDragged = [this](int deltaY)
    {
        // deltaY < 0 means mouse moved up → layer panel should grow (ratio increases)
        auto rightArea = getLocalBounds();
        rightArea.removeFromTop(kAlignBarHeight);
        int belowPropH = rightArea.getHeight() - 220;
        if (belowPropH <= 0) return;
        float ratioDelta = static_cast<float>(-deltaY) / static_cast<float>(belowPropH);
        layerPanelRatio_ = juce::jlimit(0.1f, 0.8f, layerPanelRatio_ + ratioDelta);
        resized();
    };
}

CanvasEditor::~CanvasEditor()
{
    canvasView.removeListener(this);
}

//==============================================================================
void CanvasEditor::applyThemeToAllPanels()
{
    alignToolbar.applyThemeColours();
    propertyPanel.applyThemeColours();
    layerPanel.applyThemeColours();
    meterSettings.refresh();
}

//==============================================================================
void CanvasEditor::paint(juce::Graphics& g)
{
    g.fillAll(ThemeManager::getInstance().getPalette().panelBg);

    // Grey overlay when export is running
    if (exportOverlay_)
    {
        g.setColour(juce::Colour(0xAA000000));
        g.fillRect(canvasView.getBounds());

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(18.0f);
        g.drawText("Exporting video\u2026",
                   canvasView.getBounds(), juce::Justification::centred);
    }
}

void CanvasEditor::resized()
{
    auto area = getLocalBounds();

    // Top: Alignment toolbar
    alignToolbar.setBounds(area.removeFromTop(kAlignBarHeight));

    // Left: Toolbox
    toolbox.setBounds(area.removeFromLeft(kToolboxWidth));

    // Right panel: stacked — Properties (top), Settings (middle), Layers (bottom)
    auto rightArea = area.removeFromRight(kRightPanelWidth);
    int rightH = rightArea.getHeight();
    int propH  = 350;                          // fixed property panel height
    int belowPropH = rightH - propH;
    int layerH = juce::jlimit(60, belowPropH - 60,
                               static_cast<int>(belowPropH * layerPanelRatio_));
    int settH  = belowPropH - layerH;

    propertyPanel.setBounds(rightArea.removeFromTop(propH));
    meterSettings.setBounds(rightArea.removeFromTop(settH));
    layerPanel.setBounds(rightArea);

    // MiniMap: overlay bottom-left corner of canvas view
    auto canvasArea = area;
    canvasView.setBounds(canvasArea);

    miniMap.setBounds(canvasArea.getX() + 8,
                      canvasArea.getBottom() - kMiniMapSize - 8,
                      kMiniMapSize, kMiniMapSize);
    miniMap.toFront(false);
}

//==============================================================================
void CanvasEditor::timerTick()
{
    canvasView.tickFps();

    // Freeze viewport during export — don't feed meters
    if (exportOverlay_) return;

    // Skip feeding meters if in placeholder mode (save CPU)
    if (!canvasView.isInPlaceholderMode())
    {
        for (int i = 0; i < model.getNumItems(); ++i)
            meterFactory.feedMeter(*model.getItem(i));
    }
}

//==============================================================================
CanvasItem* CanvasEditor::addMeter(MeterType type, juce::Point<float> canvasPos)
{
    auto item = std::make_unique<CanvasItem>();
    item->meterType = type;
    item->name = meterTypeName(type);

    // TextLabel defaults to 24pt; all other meters default to 12 (neutral scale)
    if (type != MeterType::TextLabel)
    {
        item->fontSize   = 12.0f;
        item->fontFamily = "Default";
    }

    auto defSize = meterDefaultSize(type);
    item->x = canvasPos.x - defSize.getWidth() * 0.5f;
    item->y = canvasPos.y - defSize.getHeight() * 0.5f;
    item->width = static_cast<float>(defSize.getWidth());
    item->height = static_cast<float>(defSize.getHeight());

    // Create the actual component
    item->component = meterFactory.createMeter(type);
    if (item->component)
    {
        // Let mouse events pass through to CanvasView for selection/dragging
        item->component->setInterceptsMouseClicks(false, false);
        canvasView.addAndMakeVisible(item->component.get());
    }

    auto* ptr = model.addItem(std::move(item));
    model.selectItem(ptr->id);
    canvasView.updateChildBounds();
    return ptr;
}

void CanvasEditor::pasteItems(juce::Point<float> canvasPos)
{
    model.paste(canvasPos);
    ensureComponents();
    canvasView.updateChildBounds();
}

void CanvasEditor::duplicateItems()
{
    model.duplicateSelection();
    ensureComponents();
    canvasView.updateChildBounds();
}

void CanvasEditor::ensureComponents()
{
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* ci = model.getItem(i);
        if (!ci->component)
        {
            ci->component = meterFactory.createMeter(ci->meterType);
            if (ci->component)
            {
                ci->component->setInterceptsMouseClicks(false, false);
                canvasView.addAndMakeVisible(ci->component.get());
            }
        }
    }
}

void CanvasEditor::applySkinToAll(const Skin::SkinModel* skin)
{
    for (int i = 0; i < model.getNumItems(); ++i)
        meterFactory.applySkin(*model.getItem(i), skin);
}

void CanvasEditor::onFileLoaded(double sampleRate)
{
    meterFactory.onFileLoaded(sampleRate);
    // Reset spectrograms and histograms
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item->component) continue;
        if (item->meterType == MeterType::Spectrogram)
            static_cast<Spectrogram*>(item->component.get())->setSampleRate(sampleRate);
        else if (item->meterType == MeterType::LevelHistogram)
            static_cast<LevelHistogram*>(item->component.get())->reset();
    }
}

//==============================================================================
void CanvasEditor::canvasItemDoubleClicked(CanvasItem* item)
{
    if (item != nullptr && (item->meterType == MeterType::SkinnedPlayer
                         || item->meterType == MeterType::Equalizer))
    {
        // Toggle interactive mode — let the component receive mouse events directly
        setItemInteractiveMode(item, !item->interactiveMode);
        return;
    }
    // Bring property panel to focus — it auto-refreshes via listener
    propertyPanel.refresh();
}

void CanvasEditor::canvasContextMenu(CanvasItem* item, juce::Point<int> screenPos)
{
    showContextMenu(item, screenPos);
}

//==============================================================================
// DragAndDropTarget — receive meters dragged from the toolbox
//==============================================================================

bool CanvasEditor::isInterestedInDragSource(const SourceDetails& details)
{
    // Accept drags whose description is an int (the MeterType enum value)
    return details.description.isInt();
}

void CanvasEditor::itemDragEnter(const SourceDetails& /*details*/)
{
    // Could show a drop highlight — for now do nothing
}

void CanvasEditor::itemDragExit(const SourceDetails& /*details*/)
{
    // Could clear a drop highlight — for now do nothing
}

void CanvasEditor::itemDropped(const SourceDetails& details)
{
    int typeInt = static_cast<int>(details.description);
    if (typeInt < 0 || typeInt >= static_cast<int>(MeterType::NumTypes))
        return;

    auto type = static_cast<MeterType>(typeInt);

    // details.localPosition is relative to this CanvasEditor.
    // Convert to canvasView-local coordinates, then to canvas space.
    auto posInEditor = details.localPosition;
    auto posInCanvas = canvasView.getLocalPoint(this, posInEditor).toFloat();
    auto canvasPos   = model.screenToCanvas(posInCanvas);

    addMeter(type, canvasPos);
}

void CanvasEditor::setItemInteractiveMode(CanvasItem* item, bool interactive)
{
    if (item == nullptr || item->component == nullptr) return;

    // Exit all other interactive items first
    if (interactive)
        exitAllInteractiveModes();

    item->interactiveMode = interactive;
    item->component->setInterceptsMouseClicks(interactive, interactive);
    canvasView.repaint();
}

void CanvasEditor::exitAllInteractiveModes()
{
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (item->interactiveMode)
        {
            item->interactiveMode = false;
            if (item->component)
                item->component->setInterceptsMouseClicks(false, false);
        }
    }
    canvasView.repaint();
}

void CanvasEditor::groupSelection()
{
    model.groupSelection();
}

void CanvasEditor::ungroupSelection()
{
    model.ungroupSelection();
}

void CanvasEditor::showContextMenu(CanvasItem* item, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;

    if (item != nullptr)
    {
        menu.addItem(1, "Delete");
        menu.addItem(2, "Duplicate (Ctrl+D)");
        menu.addItem(6, "Rename");
        menu.addSeparator();
        menu.addItem(3, item->locked ? "Unlock" : "Lock");
        menu.addItem(4, item->visible ? "Hide" : "Show");
        menu.addItem(5, "Rotate 90 CW");
        menu.addSeparator();

        // Group / Ungroup
        {
            bool hasGroup = !item->groupId.isNull();
            int numSelected = model.getNumSelected();

            if (numSelected >= 2)
                menu.addItem(30, "Group (Ctrl+G)");
            if (hasGroup)
                menu.addItem(31, "Ungroup (Ctrl+Shift+G)");
            if (numSelected >= 2 || hasGroup)
                menu.addSeparator();
        }

        menu.addItem(10, "Bring to Front");
        menu.addItem(11, "Send to Back");
        menu.addSeparator();
        menu.addItem(12, item->aspectLock ? "Unlock Aspect" : "Lock Aspect");
    }
    else
    {
        // Clicked on empty canvas
        menu.addItem(20, "Paste (Ctrl+V)");
        menu.addItem(21, "Select All (Ctrl+A)");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ screenPos.x, screenPos.y, 1, 1 }),
        [this, item](int result)
        {
            if (result == 0) return;

            switch (result)
            {
                case 1: // Delete
                    if (item) model.removeItem(item->id);
                    break;
                case 2: // Duplicate
                    duplicateItems();
                    break;
                case 3: // Lock/Unlock
                    if (item) { item->locked = !item->locked; model.notifyItemsChanged(); }
                    break;
                case 4: // Show/Hide
                    if (item) { item->visible = !item->visible; model.notifyItemsChanged(); }
                    break;
                case 5: // Rotate
                    if (item && !item->locked)
                    {
                        int oldR = item->rotation;
                        int newR = (oldR + 90) % 360;
                        model.undoManager.beginNewTransaction("Rotate");
                        model.undoManager.perform(new RotateItemAction(model, item->id, oldR, newR), "Rot");
                    }
                    break;
                case 6: // Rename
                    if (item)
                    {
                        auto currentName = item->name.isEmpty() ? meterTypeName(item->meterType) : item->name;
                        auto* alertWindow = new juce::AlertWindow("Rename", "Enter a new name:", juce::MessageBoxIconType::NoIcon);
                        alertWindow->addTextEditor("name", currentName, "Name:");
                        alertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
                        alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

                        auto itemId = item->id;
                        alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
                            [this, alertWindow, itemId](int result)
                            {
                                if (result == 1)
                                {
                                    auto newName = alertWindow->getTextEditorContents("name").trim();
                                    if (newName.isNotEmpty())
                                    {
                                        if (auto* targetItem = model.findItem(itemId))
                                        {
                                            targetItem->name = newName;
                                            model.notifyItemsChanged();
                                        }
                                    }
                                }
                                delete alertWindow;
                            }), true);
                    }
                    break;
                case 10: // Bring to Front
                    if (item)
                    {
                        int maxZ = 0;
                        for (int i = 0; i < model.getNumItems(); ++i)
                            maxZ = std::max(maxZ, model.getItem(i)->zOrder);
                        model.undoManager.beginNewTransaction("Bring to Front");
                        model.undoManager.perform(new ChangeZOrderAction(model, item->id, item->zOrder, maxZ + 1), "Front");
                    }
                    break;
                case 11: // Send to Back
                    if (item)
                    {
                        model.undoManager.beginNewTransaction("Send to Back");
                        model.undoManager.perform(new ChangeZOrderAction(model, item->id, item->zOrder, -1), "Back");
                    }
                    break;
                case 12: // Toggle aspect
                    if (item) { item->aspectLock = !item->aspectLock; model.notifyItemsChanged(); }
                    break;
                case 20: // Paste
                {
                    auto centre = model.screenToCanvas(canvasView.getLocalBounds().getCentre().toFloat());
                    pasteItems(centre);
                    break;
                }
                case 21: // Select all
                    model.selectAll();
                    break;
                case 30: // Group
                    model.groupSelection();
                    break;
                case 31: // Ungroup
                    model.ungroupSelection();
                    break;
                default: break;
            }
        });
}

//==============================================================================
juce::Image CanvasEditor::renderPreviewFrame(int videoW, int videoH)
{
    juce::Image image(juce::Image::RGB, videoW, videoH, true,
                      juce::SoftwareImageType());
    juce::Graphics g(image);

    // Paint canvas background
    model.background.paint(g, juce::Rectangle<float>(0, 0,
        static_cast<float>(videoW), static_cast<float>(videoH)));

    // Compute content bounding box
    juce::Rectangle<float> content;
    bool first = true;
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item || !item->visible || !item->component) continue;
        auto r = item->getBounds();
        if (first) { content = r; first = false; }
        else       content = content.getUnion(r);
    }
    if (first) return image;  // nothing to render

    content = content.expanded(10.0f);

    float scaleX = static_cast<float>(videoW) / content.getWidth();
    float scaleY = static_cast<float>(videoH) / content.getHeight();
    float scale  = std::min(scaleX, scaleY);

    float offsetX = (videoW  - content.getWidth()  * scale) * 0.5f;
    float offsetY = (videoH  - content.getHeight() * scale) * 0.5f;

    // Save original sizes & font sizes so we can restore after rendering
    struct SavedState { int origW, origH; float origFontSize; float origMeterFontSize; };
    std::vector<SavedState> saved;
    saved.reserve(model.getNumItems());

    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item || !item->visible || !item->component) { saved.push_back({}); continue; }

        SavedState ss;
        ss.origW = item->component->getWidth();
        ss.origH = item->component->getHeight();

        // Save original font sizes
        ss.origFontSize = 0;
        if (item->meterType == MeterType::TextLabel)
        {
            if (auto* tlc = dynamic_cast<TextLabelComponent*>(item->component.get()))
                ss.origFontSize = tlc->getFontSize();
        }
        ss.origMeterFontSize = 0;
        if (auto* mb = dynamic_cast<MeterBase*>(item->component.get()))
            ss.origMeterFontSize = mb->getMeterFontSize();
        saved.push_back(ss);

        float iw = item->width  * scale;
        float ih = item->height * scale;
        int pw = std::max(1, static_cast<int>(iw));
        int ph = std::max(1, static_cast<int>(ih));

        // Scale font sizes
        if (item->meterType == MeterType::TextLabel)
        {
            if (auto* tlc = dynamic_cast<TextLabelComponent*>(item->component.get()))
                tlc->setFontSize(item->fontSize * scale);
        }
        if (auto* mb = dynamic_cast<MeterBase*>(item->component.get()))
            mb->setMeterFontSize(item->fontSize * scale);

        item->component->setSize(pw, ph);

        // Render to sub-image
        juce::Image meterImg(juce::Image::ARGB, pw, ph, true,
                             juce::SoftwareImageType());
        {
            juce::Graphics mg(meterImg);

            if (!item->itemBackground.isTransparent())
            {
                mg.setColour(item->itemBackground);
                mg.fillRect(0, 0, pw, ph);
            }

            item->component->paint(mg);

            for (int c = 0; c < item->component->getNumChildComponents(); ++c)
            {
                auto* child = item->component->getChildComponent(c);
                if (child && child->isVisible())
                {
                    mg.saveState();
                    mg.setOrigin(child->getPosition());
                    child->paint(mg);
                    mg.restoreState();
                }
            }
        }

        float ix = (item->x - content.getX()) * scale + offsetX;
        float iy = (item->y - content.getY()) * scale + offsetY;
        float alpha = juce::jlimit(0.0f, 1.0f, item->opacity);

        if (item->rotation != 0)
        {
            float cx = ix + iw * 0.5f;
            float cy = iy + ih * 0.5f;
            auto t = juce::AffineTransform::translation(-pw * 0.5f, -ph * 0.5f)
                .rotated(item->rotation * juce::MathConstants<float>::pi / 180.0f)
                .translated(cx, cy);
            g.setOpacity(alpha);
            g.drawImageTransformed(meterImg, t);
            g.setOpacity(1.0f);
        }
        else
        {
            g.setOpacity(alpha);
            g.drawImageAt(meterImg, static_cast<int>(ix), static_cast<int>(iy));
            g.setOpacity(1.0f);
        }

        // Draw Center/Outside strokes for shapes
        bool isShape = (item->meterType == MeterType::ShapeRectangle
                     || item->meterType == MeterType::ShapeEllipse
                     || item->meterType == MeterType::ShapeTriangle
                     || item->meterType == MeterType::ShapeLine
                     || item->meterType == MeterType::ShapeStar
                     || item->meterType == MeterType::ShapeSVG);
        if (isShape && item->strokeWidth > 0.0f)
        {
            auto strokeAlign = static_cast<StrokeAlignment>(item->strokeAlignment);
            if (strokeAlign != StrokeAlignment::Inside)
            {
                auto* sc = dynamic_cast<ShapeComponent*>(item->component.get());
                if (sc)
                {
                    juce::Rectangle<float> shapeRect(ix, iy, iw, ih);
                    auto localBounds = sc->getLocalBounds().toFloat();
                    if (localBounds.getWidth() > 0.0f && localBounds.getHeight() > 0.0f)
                    {
                        float sx = shapeRect.getWidth()  / localBounds.getWidth();
                        float sy = shapeRect.getHeight() / localBounds.getHeight();
                        juce::Path shapePath = sc->getCachedPath();
                        shapePath.applyTransform(
                            juce::AffineTransform::scale(sx, sy)
                                .translated(shapeRect.getX(), shapeRect.getY()));
                        if (item->rotation != 0)
                        {
                            auto centre = shapeRect.getCentre();
                            float rad = item->rotation * juce::MathConstants<float>::pi / 180.0f;
                            shapePath.applyTransform(
                                juce::AffineTransform::rotation(rad, centre.x, centre.y));
                        }
                        float sw = item->strokeWidth * scale;
                        if (strokeAlign == StrokeAlignment::Center)
                            sw *= 0.5f;
                        g.setColour(item->strokeColour);
                        g.strokePath(shapePath, juce::PathStrokeType(sw));
                    }
                }
            }
        }
    }

    // Restore original sizes and font sizes
    for (int i = 0; i < model.getNumItems(); ++i)
    {
        auto* item = model.getItem(i);
        if (!item || !item->visible || !item->component) continue;
        if (i >= (int)saved.size()) break;
        auto& ss = saved[i];

        item->component->setSize(ss.origW, ss.origH);

        if (item->meterType == MeterType::TextLabel)
        {
            if (auto* tlc = dynamic_cast<TextLabelComponent*>(item->component.get()))
                tlc->setFontSize(ss.origFontSize);
        }
        if (auto* mb = dynamic_cast<MeterBase*>(item->component.get()))
            mb->setMeterFontSize(ss.origMeterFontSize);
    }

    return image;
}

//==============================================================================
void CanvasEditor::showRenderPreview()
{
    // Use 1080p as default preview resolution
    int videoW = 1920;
    int videoH = 1080;

    auto preview = renderPreviewFrame(videoW, videoH);
    if (!preview.isValid()) return;

    // Create a popup window showing the preview image
    class PreviewImageComponent : public juce::Component
    {
    public:
        explicit PreviewImageComponent(const juce::Image& img) : image_(img) {}
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::black);
            if (image_.isValid())
            {
                float scaleX = static_cast<float>(getWidth())  / image_.getWidth();
                float scaleY = static_cast<float>(getHeight()) / image_.getHeight();
                float scale  = std::min(scaleX, scaleY);
                float w = image_.getWidth() * scale;
                float h = image_.getHeight() * scale;
                float x = (getWidth() - w) * 0.5f;
                float y = (getHeight() - h) * 0.5f;
                g.drawImage(image_, juce::Rectangle<float>(x, y, w, h));
            }
        }
    private:
        juce::Image image_;
    };

    // Create the window — self-deleting on close
    struct SelfDeletingWindow : public juce::DocumentWindow
    {
        SelfDeletingWindow(const juce::String& name, juce::Colour bg, int buttons)
            : juce::DocumentWindow(name, bg, buttons) {}
        void closeButtonPressed() override { setLookAndFeel(nullptr); delete this; }
    };

    static SkinnedTitleBarLookAndFeel previewTitleLnf;
    previewTitleLnf.updateColours();

    auto* window = new SelfDeletingWindow("Render Preview (1920x1080)",
        juce::Colours::black, juce::DocumentWindow::closeButton);
    window->setContentOwned(new PreviewImageComponent(preview), false);
    window->setSize(960, 540 + window->getTitleBarHeight());
    window->setResizable(true, false);
    window->setUsingNativeTitleBar(false);
    window->setLookAndFeel(&previewTitleLnf);
    window->setVisible(true);
    window->centreWithSize(window->getWidth(), window->getHeight());
}

//==============================================================================
void CanvasEditor::setExportOverlay(bool show)
{
    exportOverlay_ = show;
    canvasView.setInterceptsMouseClicks(!show, !show);
    repaint();
}
