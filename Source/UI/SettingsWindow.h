#pragma once

#include <JuceHeader.h>
#include "../Canvas/CanvasEditor.h"
#include "../Audio/AudioEngine.h"
#include "../Project/AppSettings.h"
#include "../Export/FFmpegProcess.h"
#include "ThemeManager.h"
#include "KeyboardShortcutManager.h"

//==============================================================================
/// Full-featured application settings window — tabbed with persistent storage.
class SettingsWindow : public juce::DocumentWindow
{
public:
    SettingsWindow(CanvasEditor& editor, AudioEngine& audio,
                   KeyboardShortcutManager& shortcuts)
        : DocumentWindow("Settings",
                         ThemeManager::getInstance().getPalette().panelBg,
                         DocumentWindow::closeButton),
          editor_(editor), audio_(audio), shortcuts_(shortcuts)
    {
        auto* content = new SettingsContent(editor, audio, shortcuts);
        content->onAnySettingChanged = [this]()
        {
            if (onSettingsChanged)
                onSettingsChanged();
        };
        setContentOwned(content, true);
        setResizable(true, true);
        setResizeLimits(560, 460, 1000, 800);
        setUsingNativeTitleBar(true);
        centreWithSize(640, 560);
    }

    void closeButtonPressed() override
    {
        if (onSettingsChanged)
            onSettingsChanged();
        delete this;
    }

    /// Called whenever any setting value changes — wire to MainComponent::applyLiveSettings
    std::function<void()> onSettingsChanged;

private:
    CanvasEditor& editor_;
    AudioEngine& audio_;
    KeyboardShortcutManager& shortcuts_;

    //==========================================================================
    //  Settings Content — tabbed component
    //==========================================================================
    class SettingsContent : public juce::Component
    {
    public:
        /// Called when any setting changes — wired by SettingsWindow to propagate
        std::function<void()> onAnySettingChanged;

        SettingsContent(CanvasEditor& editor, AudioEngine& audio,
                        KeyboardShortcutManager& shortcuts)
            : editor_(editor), audio_(audio), shortcuts_(shortcuts)
        {
            tabs.setTabBarDepth(32);
            tabs.setOutline(0);
            auto& pal = ThemeManager::getInstance().getPalette();

            tabs.addTab("General",     pal.panelBg, new GeneralPage(),                     true);
            tabs.addTab("Appearance",  pal.panelBg, new AppearancePage(),                  true);
            tabs.addTab("Canvas",      pal.panelBg, new CanvasPage(editor),                true);
            tabs.addTab("Performance", pal.panelBg, new PerformancePage(editor),           true);
            tabs.addTab("Audio",       pal.panelBg, new AudioPage(audio),                  true);
            tabs.addTab("Export",      pal.panelBg, new ExportPage(),                      true);
            tabs.addTab("Shortcuts",   pal.panelBg, new ShortcutsPage(shortcuts),          true);

            addAndMakeVisible(tabs);

            addAndMakeVisible(resetBtn);
            resetBtn.setButtonText("Reset All to Defaults");
            resetBtn.onClick = [this]
            {
                auto result = juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::WarningIcon,
                    "Reset Settings",
                    "Reset all settings to their default values?\nThis cannot be undone.",
                    "Reset", "Cancel", nullptr, nullptr);
                if (result)
                    resetAllDefaults();
            };

            setSize(640, 540);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(ThemeManager::getInstance().getPalette().panelBg);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            auto bottom = area.removeFromBottom(44).reduced(8, 6);
            resetBtn.setBounds(bottom.removeFromRight(180));
            tabs.setBounds(area);
        }

    private:
        CanvasEditor& editor_;
        AudioEngine& audio_;
        KeyboardShortcutManager& shortcuts_;
        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        juce::TextButton resetBtn;

        void resetAllDefaults()
        {
            auto* pf = AppSettings::getInstance().getPropertiesFile();
            pf->clear();
            pf->saveIfNeeded();

            // Refresh all tab pages
            for (int i = 0; i < tabs.getNumTabs(); ++i)
                if (auto* c = tabs.getTabContentComponent(i))
                    if (auto* r = dynamic_cast<Refreshable*>(c))
                        r->refreshFromSettings();

            if (onAnySettingChanged)
                onAnySettingChanged();
        }

        //======================================================================
        /// Interface for pages that can refresh from settings
        //======================================================================
        struct Refreshable { virtual void refreshFromSettings() = 0; virtual ~Refreshable() = default; };

        //======================================================================
        //  Helper: styled label
        //======================================================================
        static void makeLabel(juce::Label& lbl, const juce::String& text = {})
        {
            lbl.setText(text, juce::dontSendNotification);
            lbl.setColour(juce::Label::textColourId,
                          ThemeManager::getInstance().getPalette().bodyText);
            lbl.setFont(juce::Font(13.0f));
        }

        static void makeSectionHeader(juce::Label& lbl, const juce::String& text)
        {
            lbl.setText(text, juce::dontSendNotification);
            lbl.setColour(juce::Label::textColourId,
                          ThemeManager::getInstance().getPalette().accent);
            lbl.setFont(juce::Font(14.0f, juce::Font::bold));
        }

        static void styleCombo(juce::ComboBox& cb)
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            cb.setColour(juce::ComboBox::backgroundColourId, pal.panelBg.brighter(0.12f));
            cb.setColour(juce::ComboBox::textColourId, pal.bodyText);
            cb.setColour(juce::ComboBox::outlineColourId, pal.border);
        }

        static void styleSlider(juce::Slider& s, double min, double max, double step, double val)
        {
            s.setRange(min, max, step);
            s.setValue(val, juce::dontSendNotification);
            s.setSliderStyle(juce::Slider::LinearBar);
            auto& pal = ThemeManager::getInstance().getPalette();
            s.setColour(juce::Slider::trackColourId, pal.accent);
            s.setColour(juce::Slider::textBoxTextColourId, pal.bodyText);
        }

        static void styleToggle(juce::ToggleButton& tb)
        {
            auto& pal = ThemeManager::getInstance().getPalette();
            tb.setColour(juce::ToggleButton::textColourId, pal.bodyText);
            tb.setColour(juce::ToggleButton::tickColourId, pal.accent);
        }

        //======================================================================
        //  GENERAL PAGE
        //======================================================================
        class GeneralPage : public juce::Component, public Refreshable
        {
        public:
            GeneralPage()
            {
                auto& s = AppSettings::getInstance();

                makeSectionHeader(startupHeader, "Startup");
                addAndMakeVisible(startupHeader);

                styleToggle(openLastProjectToggle);
                openLastProjectToggle.setButtonText("Reopen last project on startup");
                openLastProjectToggle.setToggleState(s.getBool(AppSettings::kOpenLastProject, false), juce::dontSendNotification);
                openLastProjectToggle.onClick = [this] {
                    AppSettings::getInstance().set(AppSettings::kOpenLastProject, openLastProjectToggle.getToggleState());
                };
                addAndMakeVisible(openLastProjectToggle);

                // Auto-save
                makeSectionHeader(autoSaveHeader, "Auto-Save");
                addAndMakeVisible(autoSaveHeader);

                styleToggle(autoSaveToggle);
                autoSaveToggle.setButtonText("Enable auto-save");
                autoSaveToggle.setToggleState(s.getAutoSave(), juce::dontSendNotification);
                autoSaveToggle.onClick = [this] {
                    AppSettings::getInstance().set(AppSettings::kAutoSave, autoSaveToggle.getToggleState());
                    autoSaveIntervalSlider.setEnabled(autoSaveToggle.getToggleState());
                };
                addAndMakeVisible(autoSaveToggle);

                makeLabel(autoSaveIntervalLabel, "Interval (seconds):");
                addAndMakeVisible(autoSaveIntervalLabel);

                styleSlider(autoSaveIntervalSlider, 30, 1800, 30, s.getAutoSaveIntervalSec());
                autoSaveIntervalSlider.setTextValueSuffix(" s");
                autoSaveIntervalSlider.setEnabled(s.getAutoSave());
                autoSaveIntervalSlider.onValueChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kAutoSaveInterval, (int)autoSaveIntervalSlider.getValue());
                };
                addAndMakeVisible(autoSaveIntervalSlider);

                // Undo
                makeSectionHeader(historyHeader, "History");
                addAndMakeVisible(historyHeader);

                makeLabel(undoSizeLabel, "Undo history size:");
                addAndMakeVisible(undoSizeLabel);

                styleSlider(undoSizeSlider, 10, 500, 10, s.getInt(AppSettings::kUndoHistorySize, 100));
                undoSizeSlider.setTextValueSuffix(" steps");
                undoSizeSlider.onValueChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kUndoHistorySize, (int)undoSizeSlider.getValue());
                };
                addAndMakeVisible(undoSizeSlider);

            }

            void refreshFromSettings() override
            {
                auto& s = AppSettings::getInstance();
                openLastProjectToggle.setToggleState(s.getBool(AppSettings::kOpenLastProject, false), juce::dontSendNotification);
                autoSaveToggle.setToggleState(s.getAutoSave(), juce::dontSendNotification);
                autoSaveIntervalSlider.setValue(s.getAutoSaveIntervalSec(), juce::dontSendNotification);
                undoSizeSlider.setValue(s.getInt(AppSettings::kUndoHistorySize, 100), juce::dontSendNotification);
            }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);
                auto row = [&](int h = 26) { auto r = area.removeFromTop(h); area.removeFromTop(4); return r; };

                startupHeader.setBounds(row(22));
                openLastProjectToggle.setBounds(row());

                area.removeFromTop(6);
                autoSaveHeader.setBounds(row(22));
                autoSaveToggle.setBounds(row());
                { auto r = row(); autoSaveIntervalLabel.setBounds(r.removeFromLeft(150)); autoSaveIntervalSlider.setBounds(r); }

                area.removeFromTop(6);
                historyHeader.setBounds(row(22));
                { auto r = row(); undoSizeLabel.setBounds(r.removeFromLeft(150)); undoSizeSlider.setBounds(r); }
            }

        private:
            juce::Label startupHeader, autoSaveHeader, historyHeader;
            juce::ToggleButton openLastProjectToggle;
            juce::ToggleButton autoSaveToggle;
            juce::Label autoSaveIntervalLabel;
            juce::Slider autoSaveIntervalSlider;
            juce::Label undoSizeLabel;
            juce::Slider undoSizeSlider;
        };

        //======================================================================
        //  APPEARANCE PAGE
        //======================================================================
        class AppearancePage : public juce::Component, public Refreshable
        {
        public:
            AppearancePage()
            {
                auto& s = AppSettings::getInstance();

                makeSectionHeader(themeHeader, "Theme");
                addAndMakeVisible(themeHeader);

                styleCombo(themeCombo);
                themeCombo.addItem("Dark", 1);
                themeCombo.addItem("Light", 2);
                {
                    int saved = s.getInt(AppSettings::kTheme, 1);
                    auto cur = ThemeManager::getInstance().getCurrentTheme();
                    if (cur == AppTheme::Dark) saved = 1;
                    else if (cur == AppTheme::Light) saved = 2;
                    themeCombo.setSelectedId(saved, juce::dontSendNotification);
                }
                themeCombo.onChange = [this]
                {
                    int id = themeCombo.getSelectedId();
                    auto theme = (id == 2) ? AppTheme::Light : AppTheme::Dark;
                    ThemeManager::getInstance().setTheme(theme);
                    AppSettings::getInstance().set(AppSettings::kTheme, id);
                };
                addAndMakeVisible(themeCombo);

                // Accent colour
                makeLabel(accentLabel, "Accent colour:");
                addAndMakeVisible(accentLabel);

                juce::String savedHex = s.getString(AppSettings::kAccentColour, "FF4A90D9");
                currentAccent = juce::Colour::fromString(savedHex);
                accentButton.setColour(juce::TextButton::buttonColourId, currentAccent);
                accentButton.setButtonText("");
                accentButton.onClick = [this]
                {
                    auto* cs = new juce::ColourSelector(juce::ColourSelector::showColourAtTop
                                                        | juce::ColourSelector::showAlphaChannel
                                                        | juce::ColourSelector::showColourspace);
                    cs->setCurrentColour(currentAccent);
                    cs->setSize(300, 400);
                    cs->addChangeListener(accentListener.get());
                    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(cs),
                                                           accentButton.getScreenBounds(),
                                                           nullptr);
                };
                addAndMakeVisible(accentButton);

                accentListener = std::make_unique<AccentListener>(*this);

                // UI Scale
                makeSectionHeader(uiHeader, "Interface");
                addAndMakeVisible(uiHeader);

                makeLabel(uiScaleLabel, "UI scale:");
                addAndMakeVisible(uiScaleLabel);

                styleSlider(uiScaleSlider, 75, 200, 5, s.getUIScale());
                uiScaleSlider.setTextValueSuffix("%");
                uiScaleSlider.onValueChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kUIScale, uiScaleSlider.getValue());
                };
                addAndMakeVisible(uiScaleSlider);

                // Toolbox view mode
                makeLabel(toolboxViewLabel, "Toolbox view:");
                addAndMakeVisible(toolboxViewLabel);

                styleCombo(toolboxViewCombo);
                toolboxViewCombo.addItem("List", 1);
                toolboxViewCombo.addItem("Grid", 2);
                toolboxViewCombo.addItem("Compact", 3);
                toolboxViewCombo.setSelectedId(s.getInt(AppSettings::kToolboxViewMode, 1), juce::dontSendNotification);
                toolboxViewCombo.onChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kToolboxViewMode, toolboxViewCombo.getSelectedId());
                };
                addAndMakeVisible(toolboxViewCombo);

                // Show/hide UI elements
                styleToggle(showStatusBarToggle);
                showStatusBarToggle.setButtonText("Show status bar");
                showStatusBarToggle.setToggleState(s.getBool(AppSettings::kShowStatusBar, true), juce::dontSendNotification);
                showStatusBarToggle.onClick = [this] {
                    AppSettings::getInstance().set(AppSettings::kShowStatusBar, showStatusBarToggle.getToggleState());
                };
                addAndMakeVisible(showStatusBarToggle);

                styleToggle(showMiniMapToggle);
                showMiniMapToggle.setButtonText("Show mini-map");
                showMiniMapToggle.setToggleState(s.getBool(AppSettings::kShowMiniMap, true), juce::dontSendNotification);
                showMiniMapToggle.onClick = [this] {
                    AppSettings::getInstance().set(AppSettings::kShowMiniMap, showMiniMapToggle.getToggleState());
                };
                addAndMakeVisible(showMiniMapToggle);

            }

            void refreshFromSettings() override
            {
                auto& s = AppSettings::getInstance();
                themeCombo.setSelectedId(s.getInt(AppSettings::kTheme, 1), juce::dontSendNotification);
                uiScaleSlider.setValue(s.getUIScale(), juce::dontSendNotification);
                toolboxViewCombo.setSelectedId(s.getInt(AppSettings::kToolboxViewMode, 1), juce::dontSendNotification);
                showStatusBarToggle.setToggleState(s.getBool(AppSettings::kShowStatusBar, true), juce::dontSendNotification);
                showMiniMapToggle.setToggleState(s.getBool(AppSettings::kShowMiniMap, true), juce::dontSendNotification);
            }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);
                auto row = [&](int h = 26) { auto r = area.removeFromTop(h); area.removeFromTop(4); return r; };

                themeHeader.setBounds(row(22));
                themeCombo.setBounds(row().removeFromLeft(200));
                { auto r = row(); accentLabel.setBounds(r.removeFromLeft(120)); accentButton.setBounds(r.removeFromLeft(60)); }

                area.removeFromTop(6);
                uiHeader.setBounds(row(22));
                { auto r = row(); uiScaleLabel.setBounds(r.removeFromLeft(120)); uiScaleSlider.setBounds(r); }
                { auto r = row(); toolboxViewLabel.setBounds(r.removeFromLeft(120)); toolboxViewCombo.setBounds(r.removeFromLeft(140)); }
                showStatusBarToggle.setBounds(row());
                showMiniMapToggle.setBounds(row());
            }

        private:
            juce::Colour currentAccent;

            struct AccentListener : public juce::ChangeListener
            {
                AccentListener(AppearancePage& o) : owner(o) {}
                void changeListenerCallback(juce::ChangeBroadcaster* src) override
                {
                    if (auto* cs = dynamic_cast<juce::ColourSelector*>(src))
                    {
                        owner.currentAccent = cs->getCurrentColour();
                        owner.accentButton.setColour(juce::TextButton::buttonColourId, owner.currentAccent);
                        AppSettings::getInstance().set(AppSettings::kAccentColour,
                                                       owner.currentAccent.toString());
                    }
                }
                AppearancePage& owner;
            };
            std::unique_ptr<AccentListener> accentListener;

            juce::Label themeHeader, uiHeader;
            juce::ComboBox themeCombo;
            juce::Label accentLabel;
            juce::TextButton accentButton;
            juce::Label uiScaleLabel;
            juce::Slider uiScaleSlider;
            juce::Label toolboxViewLabel;
            juce::ComboBox toolboxViewCombo;
            juce::ToggleButton showStatusBarToggle, showMiniMapToggle;
        };

        //======================================================================
        //  CANVAS PAGE
        //======================================================================
        class CanvasPage : public juce::Component, public Refreshable
        {
        public:
            CanvasPage(CanvasEditor& editor) : editor_(editor)
            {
                auto& s = AppSettings::getInstance();
                auto& grid = editor.getModel().grid;

                makeSectionHeader(gridHeader, "Grid & Guides");
                addAndMakeVisible(gridHeader);

                styleToggle(gridEnabledToggle);
                gridEnabledToggle.setButtonText("Enable grid snapping");
                gridEnabledToggle.setToggleState(grid.enabled, juce::dontSendNotification);
                gridEnabledToggle.onClick = [this] {
                    editor_.getModel().grid.enabled = gridEnabledToggle.getToggleState();
                    AppSettings::getInstance().set(AppSettings::kDefaultGridEnabled, gridEnabledToggle.getToggleState());
                    editor_.getModel().notifyItemsChanged();
                };
                addAndMakeVisible(gridEnabledToggle);

                styleToggle(showGridToggle);
                showGridToggle.setButtonText("Show grid lines");
                showGridToggle.setToggleState(grid.showGrid, juce::dontSendNotification);
                showGridToggle.onClick = [this] {
                    editor_.getModel().grid.showGrid = showGridToggle.getToggleState();
                    AppSettings::getInstance().set(AppSettings::kDefaultShowGrid, showGridToggle.getToggleState());
                    editor_.getModel().notifyItemsChanged();
                };
                addAndMakeVisible(showGridToggle);

                styleToggle(showRulerToggle);
                showRulerToggle.setButtonText("Show rulers");
                showRulerToggle.setToggleState(grid.showRuler, juce::dontSendNotification);
                showRulerToggle.onClick = [this] {
                    editor_.getModel().grid.showRuler = showRulerToggle.getToggleState();
                    AppSettings::getInstance().set(AppSettings::kDefaultShowRuler, showRulerToggle.getToggleState());
                    editor_.getModel().notifyItemsChanged();
                };
                addAndMakeVisible(showRulerToggle);

                styleToggle(smartGuidesToggle);
                smartGuidesToggle.setButtonText("Smart guides");
                smartGuidesToggle.setToggleState(grid.smartGuides, juce::dontSendNotification);
                smartGuidesToggle.onClick = [this] {
                    editor_.getModel().grid.smartGuides = smartGuidesToggle.getToggleState();
                    AppSettings::getInstance().set(AppSettings::kDefaultSmartGuides, smartGuidesToggle.getToggleState());
                };
                addAndMakeVisible(smartGuidesToggle);

                makeLabel(gridSpacingLabel, "Grid spacing:");
                addAndMakeVisible(gridSpacingLabel);

                styleSlider(gridSpacingSlider, 2, 100, 1, grid.spacing);
                gridSpacingSlider.setTextValueSuffix(" px");
                gridSpacingSlider.onValueChange = [this] {
                    int sp = (int)gridSpacingSlider.getValue();
                    editor_.getModel().grid.spacing = sp;
                    AppSettings::getInstance().set(AppSettings::kDefaultGridSpacing, sp);
                    editor_.getModel().notifyItemsChanged();
                };
                addAndMakeVisible(gridSpacingSlider);

                // Grid colour
                makeLabel(gridColourLabel, "Grid colour:");
                addAndMakeVisible(gridColourLabel);

                gridColourBtn.setColour(juce::TextButton::buttonColourId, grid.gridColour);
                gridColourBtn.setButtonText("");
                gridColourBtn.onClick = [this]
                {
                    auto* cs = new juce::ColourSelector(juce::ColourSelector::showColourAtTop
                                                        | juce::ColourSelector::showAlphaChannel
                                                        | juce::ColourSelector::showColourspace);
                    cs->setCurrentColour(editor_.getModel().grid.gridColour);
                    cs->setSize(300, 400);
                    cs->addChangeListener(gridColourListener.get());
                    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(cs),
                                                           gridColourBtn.getScreenBounds(), nullptr);
                };
                addAndMakeVisible(gridColourBtn);

                gridColourListener = std::make_unique<GridColourListener>(*this);

                // Handle size
                makeSectionHeader(editHeader, "Editing");
                addAndMakeVisible(editHeader);

                makeLabel(handleSizeLabel, "Selection handle size:");
                addAndMakeVisible(handleSizeLabel);
                styleSlider(handleSizeSlider, 4, 16, 1, s.getDouble(AppSettings::kHandleSize, 8.0));
                handleSizeSlider.setTextValueSuffix(" px");
                handleSizeSlider.onValueChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kHandleSize, handleSizeSlider.getValue());
                };
                addAndMakeVisible(handleSizeSlider);
            }

            void refreshFromSettings() override
            {
                auto& grid = editor_.getModel().grid;
                gridEnabledToggle.setToggleState(grid.enabled, juce::dontSendNotification);
                showGridToggle.setToggleState(grid.showGrid, juce::dontSendNotification);
                showRulerToggle.setToggleState(grid.showRuler, juce::dontSendNotification);
                smartGuidesToggle.setToggleState(grid.smartGuides, juce::dontSendNotification);
                gridSpacingSlider.setValue(grid.spacing, juce::dontSendNotification);
                gridColourBtn.setColour(juce::TextButton::buttonColourId, grid.gridColour);
            }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);
                auto row = [&](int h = 26) { auto r = area.removeFromTop(h); area.removeFromTop(4); return r; };

                gridHeader.setBounds(row(22));
                gridEnabledToggle.setBounds(row());
                showGridToggle.setBounds(row());
                showRulerToggle.setBounds(row());
                smartGuidesToggle.setBounds(row());
                { auto r = row(); gridSpacingLabel.setBounds(r.removeFromLeft(120)); gridSpacingSlider.setBounds(r); }
                { auto r = row(); gridColourLabel.setBounds(r.removeFromLeft(120)); gridColourBtn.setBounds(r.removeFromLeft(60)); }

                area.removeFromTop(6);
                editHeader.setBounds(row(22));
                { auto r = row(); handleSizeLabel.setBounds(r.removeFromLeft(150)); handleSizeSlider.setBounds(r); }
            }

        private:
            CanvasEditor& editor_;

            struct GridColourListener : public juce::ChangeListener
            {
                GridColourListener(CanvasPage& o) : owner(o) {}
                void changeListenerCallback(juce::ChangeBroadcaster* src) override
                {
                    if (auto* cs = dynamic_cast<juce::ColourSelector*>(src))
                    {
                        owner.editor_.getModel().grid.gridColour = cs->getCurrentColour();
                        owner.gridColourBtn.setColour(juce::TextButton::buttonColourId, cs->getCurrentColour());
                        owner.editor_.getModel().notifyItemsChanged();
                    }
                }
                CanvasPage& owner;
            };
            std::unique_ptr<GridColourListener> gridColourListener;

            juce::Label gridHeader, editHeader;
            juce::ToggleButton gridEnabledToggle, showGridToggle, showRulerToggle, smartGuidesToggle;
            juce::Label gridSpacingLabel, gridColourLabel;
            juce::Slider gridSpacingSlider;
            juce::TextButton gridColourBtn;
            juce::Label handleSizeLabel;
            juce::Slider handleSizeSlider;
        };

        //======================================================================
        //  PERFORMANCE PAGE
        //======================================================================
        class PerformancePage : public juce::Component, public Refreshable
        {
        public:
            PerformancePage(CanvasEditor& editor) : editor_(editor)
            {
                auto& s = AppSettings::getInstance();

                makeSectionHeader(renderHeader, "Rendering");
                addAndMakeVisible(renderHeader);

                makeLabel(fpsLabel, "FPS threshold:");
                addAndMakeVisible(fpsLabel);
                styleSlider(fpsSlider, 5, 60, 1, editor.getCanvasView().getFpsThreshold());
                fpsSlider.setTextValueSuffix(" fps");
                fpsSlider.onValueChange = [this] {
                    float v = (float)fpsSlider.getValue();
                    editor_.getCanvasView().setFpsThreshold(v);
                    AppSettings::getInstance().set(AppSettings::kFpsThreshold, (double)v);
                };
                addAndMakeVisible(fpsSlider);

                makeLabel(fpsHint, "Below this FPS, meters switch to placeholder mode to maintain performance.");
                fpsHint.setFont(juce::Font(11.0f));
                fpsHint.setColour(juce::Label::textColourId, ThemeManager::getInstance().getPalette().dimText);
                addAndMakeVisible(fpsHint);

                makeLabel(timerLabel, "Timer rate:");
                addAndMakeVisible(timerLabel);
                styleSlider(timerSlider, 15, 120, 1, s.getTimerRateHz());
                timerSlider.setTextValueSuffix(" Hz");
                timerSlider.onValueChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kTimerRateHz, (int)timerSlider.getValue());
                };
                addAndMakeVisible(timerSlider);

                makeLabel(timerHint, "Main loop update rate. Lower values save CPU but reduce smoothness. Restart required.");
                timerHint.setFont(juce::Font(11.0f));
                timerHint.setColour(juce::Label::textColourId, ThemeManager::getInstance().getPalette().dimText);
                addAndMakeVisible(timerHint);

                makeLabel(restartNote, "* Some performance settings require a restart to take effect.");
                restartNote.setFont(juce::Font(11.0f, juce::Font::italic));
                restartNote.setColour(juce::Label::textColourId, ThemeManager::getInstance().getPalette().dimText);
                addAndMakeVisible(restartNote);
            }

            void refreshFromSettings() override
            {
                fpsSlider.setValue(editor_.getCanvasView().getFpsThreshold(), juce::dontSendNotification);
                timerSlider.setValue(AppSettings::getInstance().getTimerRateHz(), juce::dontSendNotification);
            }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);
                auto row = [&](int h = 26) { auto r = area.removeFromTop(h); area.removeFromTop(4); return r; };

                renderHeader.setBounds(row(22));
                { auto r = row(); fpsLabel.setBounds(r.removeFromLeft(120)); fpsSlider.setBounds(r); }
                fpsHint.setBounds(row(18));
                area.removeFromTop(2);
                { auto r = row(); timerLabel.setBounds(r.removeFromLeft(120)); timerSlider.setBounds(r); }
                timerHint.setBounds(row(18));

                area.removeFromTop(10);
                restartNote.setBounds(row(18));
            }

        private:
            CanvasEditor& editor_;
            juce::Label renderHeader;
            juce::Label fpsLabel, fpsHint, timerLabel, timerHint, restartNote;
            juce::Slider fpsSlider, timerSlider;
        };

        //======================================================================
        //  AUDIO PAGE
        //======================================================================
        class AudioPage : public juce::Component, public Refreshable
        {
        public:
            AudioPage(AudioEngine& audio) : audio_(audio)
            {
                makeSectionHeader(deviceHeader, "Audio Device");
                addAndMakeVisible(deviceHeader);

                // JUCE's built-in audio device selector
                deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
                    audio.getDeviceManager(),
                    0, 0,   // min/max input channels
                    1, 2,   // min/max output channels
                    false,  // show MIDI inputs
                    false,  // show MIDI outputs
                    false,  // show channels as stereo pairs
                    false   // hide advanced options
                );
                addAndMakeVisible(*deviceSelector);

                // Master gain
                makeSectionHeader(gainHeader, "Master Volume");
                addAndMakeVisible(gainHeader);

                makeLabel(gainLabel, "Master gain:");
                addAndMakeVisible(gainLabel);

                styleSlider(gainSlider, 0.0, 2.0, 0.01, audio.getGain());
                gainSlider.setTextValueSuffix("x");
                gainSlider.onValueChange = [this] {
                    float g = (float)gainSlider.getValue();
                    audio_.setGain(g);
                    AppSettings::getInstance().set(AppSettings::kMasterGain, (double)g);
                };
                addAndMakeVisible(gainSlider);

                makeLabel(gainHint, "1.0x = unity gain. Values above 1.0 may cause clipping.");
                gainHint.setFont(juce::Font(11.0f));
                gainHint.setColour(juce::Label::textColourId, ThemeManager::getInstance().getPalette().dimText);
                addAndMakeVisible(gainHint);
            }

            void refreshFromSettings() override
            {
                gainSlider.setValue(audio_.getGain(), juce::dontSendNotification);
            }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);

                deviceHeader.setBounds(area.removeFromTop(22));
                area.removeFromTop(4);
                // Give device selector enough space
                int selectorH = juce::jmin(250, area.getHeight() - 100);
                deviceSelector->setBounds(area.removeFromTop(selectorH));

                area.removeFromTop(10);
                gainHeader.setBounds(area.removeFromTop(22));
                area.removeFromTop(4);
                { auto r = area.removeFromTop(26); gainLabel.setBounds(r.removeFromLeft(120)); gainSlider.setBounds(r); }
                area.removeFromTop(4);
                gainHint.setBounds(area.removeFromTop(18));
            }

        private:
            AudioEngine& audio_;
            std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
            juce::Label deviceHeader, gainHeader;
            juce::Label gainLabel, gainHint;
            juce::Slider gainSlider;
        };

        //======================================================================
        //  EXPORT PAGE
        //======================================================================
        class ExportPage : public juce::Component, public Refreshable
        {
        public:
            ExportPage()
            {
                auto& s = AppSettings::getInstance();
                auto& pal = ThemeManager::getInstance().getPalette();

                // FFmpeg
                makeSectionHeader(ffmpegHeader, "FFmpeg");
                addAndMakeVisible(ffmpegHeader);

                makeLabel(ffmpegStatusLabel, "");
                addAndMakeVisible(ffmpegStatusLabel);
                updateFFmpegStatus();

                ffmpegPathEditor.setReadOnly(true);
                ffmpegPathEditor.setMultiLine(false);
                ffmpegPathEditor.setText(s.getFFmpegPath(), juce::dontSendNotification);
                ffmpegPathEditor.setColour(juce::TextEditor::backgroundColourId, pal.panelBg.brighter(0.1f));
                ffmpegPathEditor.setColour(juce::TextEditor::textColourId, pal.bodyText);
                ffmpegPathEditor.setColour(juce::TextEditor::outlineColourId, pal.border);
                addAndMakeVisible(ffmpegPathEditor);

                browseBtn.setButtonText("Browse...");
                browseBtn.onClick = [this]
                {
                    fileChooser = std::make_unique<juce::FileChooser>(
                        "Locate ffmpeg.exe", juce::File(), "*.exe");
                    fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                             | juce::FileBrowserComponent::canSelectFiles,
                        [this](const juce::FileChooser& fc)
                        {
                            auto result = fc.getResult();
                            if (result.existsAsFile())
                            {
                                ffmpegPathEditor.setText(result.getFullPathName(), juce::dontSendNotification);
                                AppSettings::getInstance().set(AppSettings::kFFmpegPath, result.getFullPathName());
                                updateFFmpegStatus();
                            }
                        });
                };
                addAndMakeVisible(browseBtn);

                autoDetectBtn.setButtonText("Auto-detect");
                autoDetectBtn.onClick = [this]
                {
                    auto found = FFmpegProcess::locateFFmpeg();
                    if (found.existsAsFile())
                    {
                        ffmpegPathEditor.setText(found.getFullPathName(), juce::dontSendNotification);
                        AppSettings::getInstance().set(AppSettings::kFFmpegPath, found.getFullPathName());
                    }
                    else
                    {
                        ffmpegPathEditor.setText("", juce::dontSendNotification);
                        AppSettings::getInstance().set(AppSettings::kFFmpegPath, juce::String());
                    }
                    updateFFmpegStatus();
                };
                addAndMakeVisible(autoDetectBtn);

                // Default export settings
                makeSectionHeader(defaultsHeader, "Default Export Settings");
                addAndMakeVisible(defaultsHeader);

                makeLabel(resolutionLabel, "Resolution:");
                addAndMakeVisible(resolutionLabel);

                styleCombo(resolutionCombo);
                resolutionCombo.addItem("1080p (1920x1080)", 1);
                resolutionCombo.addItem("1440p (2560x1440)", 2);
                resolutionCombo.addItem("4K (3840x2160)", 3);
                resolutionCombo.addItem("Vertical (1080x1920)", 4);
                resolutionCombo.addItem("Square (1080x1080)", 5);
                resolutionCombo.setSelectedId(s.getInt(AppSettings::kDefaultResolution, 1), juce::dontSendNotification);
                resolutionCombo.onChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kDefaultResolution, resolutionCombo.getSelectedId());
                };
                addAndMakeVisible(resolutionCombo);

                makeLabel(fpsLabel, "Frame rate:");
                addAndMakeVisible(fpsLabel);

                styleCombo(fpsCombo);
                fpsCombo.addItem("24 fps", 1);
                fpsCombo.addItem("25 fps", 2);
                fpsCombo.addItem("30 fps", 3);
                fpsCombo.addItem("50 fps", 4);
                fpsCombo.addItem("60 fps", 5);
                fpsCombo.setSelectedId(s.getInt(AppSettings::kDefaultFrameRate, 3), juce::dontSendNotification);
                fpsCombo.onChange = [this] {
                    AppSettings::getInstance().set(AppSettings::kDefaultFrameRate, fpsCombo.getSelectedId());
                };
                addAndMakeVisible(fpsCombo);

            }

            void refreshFromSettings() override
            {
                auto& s = AppSettings::getInstance();
                ffmpegPathEditor.setText(s.getFFmpegPath(), juce::dontSendNotification);
                updateFFmpegStatus();
                resolutionCombo.setSelectedId(s.getInt(AppSettings::kDefaultResolution, 1), juce::dontSendNotification);
                fpsCombo.setSelectedId(s.getInt(AppSettings::kDefaultFrameRate, 3), juce::dontSendNotification);
            }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);
                auto row = [&](int h = 26) { auto r = area.removeFromTop(h); area.removeFromTop(4); return r; };

                ffmpegHeader.setBounds(row(22));
                ffmpegStatusLabel.setBounds(row(20));
                {
                    auto r = row(28);
                    autoDetectBtn.setBounds(r.removeFromRight(90));
                    r.removeFromRight(4);
                    browseBtn.setBounds(r.removeFromRight(80));
                    r.removeFromRight(4);
                    ffmpegPathEditor.setBounds(r);
                }

                area.removeFromTop(8);
                defaultsHeader.setBounds(row(22));
                { auto r = row(); resolutionLabel.setBounds(r.removeFromLeft(120)); resolutionCombo.setBounds(r.removeFromLeft(200)); }
                { auto r = row(); fpsLabel.setBounds(r.removeFromLeft(120)); fpsCombo.setBounds(r.removeFromLeft(200)); }
            }

        private:
            void updateFFmpegStatus()
            {
                auto customPath = AppSettings::getInstance().getFFmpegPath();
                juce::File ff;
                if (customPath.isNotEmpty())
                    ff = juce::File(customPath);
                else
                    ff = FFmpegProcess::locateFFmpeg();

                if (ff.existsAsFile())
                {
                    ffmpegStatusLabel.setText("FFmpeg found: " + ff.getFullPathName(),
                                              juce::dontSendNotification);
                    ffmpegStatusLabel.setColour(juce::Label::textColourId,
                                                juce::Colour(0xFF4CAF50)); // green
                }
                else
                {
                    ffmpegStatusLabel.setText("FFmpeg not found — export will not work",
                                              juce::dontSendNotification);
                    ffmpegStatusLabel.setColour(juce::Label::textColourId,
                                                juce::Colour(0xFFFF5252)); // red
                }
            }

            std::unique_ptr<juce::FileChooser> fileChooser;
            juce::Label ffmpegHeader, defaultsHeader;
            juce::Label ffmpegStatusLabel;
            juce::TextEditor ffmpegPathEditor;
            juce::TextButton browseBtn, autoDetectBtn;
            juce::Label resolutionLabel, fpsLabel;
            juce::ComboBox resolutionCombo, fpsCombo;
        };

        //======================================================================
        //  SHORTCUTS PAGE
        //======================================================================
        class ShortcutsPage : public juce::Component, public Refreshable
        {
        public:
            ShortcutsPage(KeyboardShortcutManager& sm) : shortcuts_(sm)
            {
                makeSectionHeader(header, "Keyboard Shortcuts");
                addAndMakeVisible(header);

                makeLabel(hint, "Click a binding to change it. Press Escape to cancel. Press Backspace to clear.");
                hint.setFont(juce::Font(11.0f));
                hint.setColour(juce::Label::textColourId, ThemeManager::getInstance().getPalette().dimText);
                addAndMakeVisible(hint);

                rebuildList();

                addAndMakeVisible(viewport);
                viewport.setViewedComponent(&listComp, false);
                viewport.setScrollBarsShown(true, false);
            }

            void refreshFromSettings() override { rebuildList(); }

            void paint(juce::Graphics& g) override { g.fillAll(ThemeManager::getInstance().getPalette().panelBg); }

            void resized() override
            {
                auto area = getLocalBounds().reduced(16, 10);
                header.setBounds(area.removeFromTop(22));
                area.removeFromTop(4);
                hint.setBounds(area.removeFromTop(18));
                area.removeFromTop(6);
                viewport.setBounds(area);
                listComp.setSize(area.getWidth() - 16, (int)rows.size() * 30 + 10);
            }

        private:
            KeyboardShortcutManager& shortcuts_;
            juce::Label header, hint;
            juce::Viewport viewport;
            juce::Component listComp;

            struct ShortcutRow
            {
                std::unique_ptr<juce::Label> nameLabel;
                std::unique_ptr<juce::TextButton> bindingBtn;
                ShortcutId id;
            };
            std::vector<ShortcutRow> rows;

            void rebuildList()
            {
                rows.clear();
                listComp.removeAllChildren();

                auto addRow = [&](ShortcutId sid, const juce::String& name)
                {
                    ShortcutRow row;
                    row.id = sid;
                    row.nameLabel = std::make_unique<juce::Label>();
                    makeLabel(*row.nameLabel, name);
                    listComp.addAndMakeVisible(*row.nameLabel);

                    row.bindingBtn = std::make_unique<juce::TextButton>();
                    auto binding = shortcuts_.getBinding(sid);
                    row.bindingBtn->setButtonText(binding.isValid()
                        ? binding.getTextDescriptionWithIcons()
                        : juce::String("(none)"));
                    auto& pal = ThemeManager::getInstance().getPalette();
                    row.bindingBtn->setColour(juce::TextButton::buttonColourId, pal.panelBg.brighter(0.08f));
                    row.bindingBtn->setColour(juce::TextButton::textColourOffId, pal.bodyText);

                    row.bindingBtn->onClick = [this, sid, btn = row.bindingBtn.get()]
                    {
                        btn->setButtonText("Press key...");
                        capturingSid = sid;
                        captureBtn = btn;
                    };

                    listComp.addAndMakeVisible(*row.bindingBtn);
                    rows.push_back(std::move(row));
                };

                // File
                addRow(ShortcutId::NewProject,      "New Project");
                addRow(ShortcutId::OpenProject,      "Open Project");
                addRow(ShortcutId::SaveProject,      "Save Project");
                addRow(ShortcutId::SaveProjectAs,    "Save Project As");
                addRow(ShortcutId::ExportVideo,      "Export Video");
                addRow(ShortcutId::BatchExport,      "Batch Export");

                // Edit
                addRow(ShortcutId::Undo,             "Undo");
                addRow(ShortcutId::Redo,             "Redo");
                addRow(ShortcutId::Cut,              "Cut");
                addRow(ShortcutId::Copy,             "Copy");
                addRow(ShortcutId::Paste,            "Paste");
                addRow(ShortcutId::Duplicate,        "Duplicate");
                addRow(ShortcutId::Delete,           "Delete");
                addRow(ShortcutId::SelectAll,        "Select All");

                // View
                addRow(ShortcutId::ZoomIn,           "Zoom In");
                addRow(ShortcutId::ZoomOut,          "Zoom Out");
                addRow(ShortcutId::ZoomToFit,        "Zoom to Fit");
                addRow(ShortcutId::ZoomReset,        "Zoom Reset");
                addRow(ShortcutId::ToggleGrid,       "Toggle Grid");
                addRow(ShortcutId::ToggleSnap,       "Toggle Snap");
                addRow(ShortcutId::ToggleTheme,      "Toggle Theme");
                addRow(ShortcutId::ToggleFullscreen,  "Toggle Fullscreen");

                // Canvas
                addRow(ShortcutId::BringToFront,     "Bring to Front");
                addRow(ShortcutId::SendToBack,       "Send to Back");
                addRow(ShortcutId::LockSelected,     "Lock Selected");
                addRow(ShortcutId::GroupSelected,     "Group Selected");
                addRow(ShortcutId::UngroupSelected,   "Ungroup Selected");

                // Transport
                addRow(ShortcutId::PlayPause,        "Play / Pause");
                addRow(ShortcutId::Stop,             "Stop");
                addRow(ShortcutId::Rewind,           "Rewind");

                layoutRows();
            }

            void layoutRows()
            {
                int y = 4;
                for (auto& row : rows)
                {
                    int h = 26;
                    row.nameLabel->setBounds(0, y, 200, h);
                    row.bindingBtn->setBounds(210, y, 200, h);
                    y += 30;
                }
                listComp.setSize(listComp.getWidth(), y + 4);
            }

            // Key capture
            ShortcutId capturingSid = ShortcutId::NewProject;
            juce::TextButton* captureBtn = nullptr;

            bool keyPressed(const juce::KeyPress& key) override
            {
                if (captureBtn == nullptr) return false;

                if (key == juce::KeyPress::backspaceKey)
                {
                    // Clear binding
                    shortcuts_.setBinding(capturingSid, juce::KeyPress());
                    captureBtn->setButtonText("(none)");
                }
                else if (key == juce::KeyPress::escapeKey)
                {
                    // Cancel — restore original
                    auto binding = shortcuts_.getBinding(capturingSid);
                    captureBtn->setButtonText(binding.isValid()
                        ? binding.getTextDescriptionWithIcons()
                        : juce::String("(none)"));
                }
                else
                {
                    shortcuts_.setBinding(capturingSid, key);
                    captureBtn->setButtonText(key.getTextDescriptionWithIcons());
                }

                captureBtn = nullptr;
                return true;
            }
        };
    };
};
