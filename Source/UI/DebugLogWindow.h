#pragma once

/**
 * @file DebugLogWindow.h
 * @brief Debug log window and diagnostic logger for MaxiMeter.
 *
 * Provides:
 *   - DebugLogger:   Singleton ring-buffer that captures diagnostic messages.
 *   - DebugLogWindow: DocumentWindow that displays bridge status, instance
 *                      diagnostics, and a scrollable live log.
 */

#include <JuceHeader.h>
#include "../Canvas/PythonPluginBridge.h"
#include "SkinnedTitleBarLookAndFeel.h"
#include "ThemeManager.h"
#include <deque>
#include <mutex>

//==============================================================================
/// Thread-safe ring-buffer logger.  Call DebugLogger::log() from anywhere;
/// the DebugLogWindow will poll for new entries via a timer.
class DebugLogger
{
public:
    struct Entry
    {
        juce::String timestamp;
        juce::String category;   // "BRIDGE", "SHADER", "RENDER", "INSTANCE", …
        juce::String message;
    };

    static DebugLogger& getInstance()
    {
        static DebugLogger inst;
        return inst;
    }

    /// Append a log entry (thread-safe).
    void log(const juce::String& category, const juce::String& message)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Entry e;
        e.timestamp = juce::Time::getCurrentTime().toString(false, true, true, true);
        e.category  = category;
        e.message   = message;
        entries_.push_back(std::move(e));
        if (entries_.size() > maxEntries_)
            entries_.pop_front();
    }

    /// Snapshot all entries (thread-safe).
    std::vector<Entry> getEntries() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return { entries_.begin(), entries_.end() };
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    int size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return (int)entries_.size();
    }

private:
    DebugLogger() = default;
    mutable std::mutex mutex_;
    std::deque<Entry> entries_;
    static constexpr size_t maxEntries_ = 2000;
};

//==============================================================================
/// Convenience macro — use throughout the codebase.
#define MAXIMETER_LOG(cat, msg)  DebugLogger::getInstance().log(cat, msg)

//==============================================================================
/// The content component shown inside DebugLogWindow.
class DebugLogContent : public juce::Component,
                        private juce::Timer,
                        private ThemeManager::Listener
{
public:
    DebugLogContent()
    {
        // ── Status label ──
        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::topLeft);
        statusLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, 0));

        // ── Log text editor ──
        addAndMakeVisible(logEditor);
        logEditor.setMultiLine(true, false);
        logEditor.setReadOnly(true);
        logEditor.setScrollbarsShown(true);
        logEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, 0));
        applyThemeColours();

        // ── Buttons ──
        addAndMakeVisible(btnClear);
        btnClear.setButtonText("Clear Log");
        btnClear.onClick = [this]
        {
            DebugLogger::getInstance().clear();
            logEditor.clear();
        };

        addAndMakeVisible(btnCopy);
        btnCopy.setButtonText("Copy to Clipboard");
        btnCopy.onClick = [this]
        {
            juce::SystemClipboard::copyTextToClipboard(logEditor.getText());
        };

        addAndMakeVisible(btnRestartBridge);
        btnRestartBridge.setButtonText("Restart Bridge");
        btnRestartBridge.onClick = [this]
        {
            auto& bridge = PythonPluginBridge::getInstance();
            MAXIMETER_LOG("BRIDGE", "Manual restart requested by user");
            bridge.stop();
            // Re-start with saved directory
            auto exeDir = juce::File::getSpecialLocation(
                juce::File::currentExecutableFile).getParentDirectory();
            auto pluginsDir = exeDir.getChildFile("CustomComponents").getChildFile("plugins");
            bridge.start(pluginsDir);
        };

        addAndMakeVisible(btnRescan);
        btnRescan.setButtonText("Re-scan Plugins");
        btnRescan.onClick = [this]
        {
            MAXIMETER_LOG("BRIDGE", "Manual plugin re-scan requested by user");
            PythonPluginBridge::getInstance().scanPlugins();
            refreshStatus();
        };

        addAndMakeVisible(btnTestConnection);
        btnTestConnection.setButtonText("Test Connection");
        btnTestConnection.onClick = [this] { testBridgeConnection(); };

        addAndMakeVisible(autoScrollToggle);
        autoScrollToggle.setButtonText("Auto-scroll");
        autoScrollToggle.setToggleState(true, juce::dontSendNotification);

        // ── Filter combo ──
        addAndMakeVisible(filterLabel);
        filterLabel.setText("Filter:", juce::dontSendNotification);
        filterLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(filterCombo);
        filterCombo.addItem("All", 1);
        filterCombo.addItem("BRIDGE", 2);
        filterCombo.addItem("SHADER", 3);
        filterCombo.addItem("RENDER", 4);
        filterCombo.addItem("INSTANCE", 5);
        filterCombo.addItem("ERROR", 6);
        filterCombo.setSelectedId(1);
        filterCombo.onChange = [this] { refreshLog(); };

        startTimerHz(4);   // Refresh 4 times per second
        refreshStatus();
        refreshLog();
        ThemeManager::getInstance().addListener(this);
    }

    ~DebugLogContent() override
    {
        ThemeManager::getInstance().removeListener(this);
    }

    void paint(juce::Graphics& g) override
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        g.fillAll(pal.panelBg);
    }

    void themeChanged(AppTheme) override
    {
        applyThemeColours();
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);

        // Top row: buttons
        auto btnRow = area.removeFromTop(28);
        btnRestartBridge.setBounds(btnRow.removeFromLeft(110));
        btnRow.removeFromLeft(4);
        btnRescan.setBounds(btnRow.removeFromLeft(110));
        btnRow.removeFromLeft(4);
        btnTestConnection.setBounds(btnRow.removeFromLeft(120));
        btnRow.removeFromLeft(12);
        btnClear.setBounds(btnRow.removeFromLeft(80));
        btnRow.removeFromLeft(4);
        btnCopy.setBounds(btnRow.removeFromLeft(120));
        btnRow.removeFromLeft(12);
        autoScrollToggle.setBounds(btnRow.removeFromLeft(100));

        area.removeFromTop(6);

        // Filter row
        auto filterRow = area.removeFromTop(24);
        filterLabel.setBounds(filterRow.removeFromLeft(45));
        filterRow.removeFromLeft(4);
        filterCombo.setBounds(filterRow.removeFromLeft(120));

        area.removeFromTop(6);

        // Status area
        statusLabel.setBounds(area.removeFromTop(100));

        area.removeFromTop(4);

        // Log editor fills rest
        logEditor.setBounds(area);
    }

    void refreshStatus()
    {
        auto& bridge = PythonPluginBridge::getInstance();
        juce::String s;

        s << "=== Python Bridge Status ===" << juce::newLine;
        s << "  Running:         " << (bridge.isRunning() ? "YES" : "NO") << juce::newLine;

        auto manifests = bridge.getAvailablePlugins();
        s << "  Plugins found:   " << (int)manifests.size() << juce::newLine;
        for (auto& m : manifests)
            s << "    - " << m.name << "  [" << m.id << "]  src=" << m.sourceFile << juce::newLine;

        s << "  Log entries:     " << DebugLogger::getInstance().size() << juce::newLine;

        statusLabel.setText(s, juce::dontSendNotification);
    }

    void refreshLog()
    {
        auto entries = DebugLogger::getInstance().getEntries();
        auto filterIdx = filterCombo.getSelectedId();
        juce::String filterCat;
        switch (filterIdx)
        {
            case 2: filterCat = "BRIDGE";   break;
            case 3: filterCat = "SHADER";   break;
            case 4: filterCat = "RENDER";   break;
            case 5: filterCat = "INSTANCE"; break;
            case 6: filterCat = "ERROR";    break;
            default: break; // show all
        }

        juce::String text;
        for (auto& e : entries)
        {
            if (filterCat.isNotEmpty() && e.category != filterCat)
                continue;
            text << "[" << e.timestamp << "] [" << e.category << "] " << e.message << juce::newLine;
        }

        if (text != lastLogText_)
        {
            lastLogText_ = text;
            logEditor.setText(text, false);
            if (autoScrollToggle.getToggleState())
                logEditor.moveCaretToEnd();
        }
    }

    void testBridgeConnection()
    {
        auto& bridge = PythonPluginBridge::getInstance();
        MAXIMETER_LOG("BRIDGE", "--- Connection Test Start ---");
        MAXIMETER_LOG("BRIDGE", "isRunning() = " + juce::String(bridge.isRunning() ? "true" : "false"));

        auto manifests = bridge.getAvailablePlugins();
        MAXIMETER_LOG("BRIDGE", "Cached manifests: " + juce::String((int)manifests.size()));

        if (!bridge.isRunning())
        {
            MAXIMETER_LOG("ERROR", "Bridge is NOT running — plugins will show black");
            refreshStatus();
            return;
        }

        // Try a scan to test communication
        MAXIMETER_LOG("BRIDGE", "Sending scan command...");
        bridge.scanPlugins();
        auto updated = bridge.getAvailablePlugins();
        MAXIMETER_LOG("BRIDGE", "After scan: " + juce::String((int)updated.size()) + " plugins found");

        if (updated.empty())
            MAXIMETER_LOG("ERROR", "No plugins found after scan — check CustomComponents/plugins/ directory");
        else
        {
            for (auto& m : updated)
                MAXIMETER_LOG("BRIDGE", "  Plugin: " + m.name + " [" + m.id + "]");
        }

        MAXIMETER_LOG("BRIDGE", "--- Connection Test End ---");
        refreshStatus();
    }

private:
    void timerCallback() override
    {
        refreshStatus();
        refreshLog();
    }

    void applyThemeColours()
    {
        auto& pal = ThemeManager::getInstance().getPalette();
        logEditor.setColour(juce::TextEditor::backgroundColourId, pal.windowBg);
        logEditor.setColour(juce::TextEditor::textColourId, pal.bodyText);
        statusLabel.setColour(juce::Label::textColourId, pal.bodyText);
        filterLabel.setColour(juce::Label::textColourId, pal.bodyText);
    }

    juce::Label statusLabel;
    juce::TextEditor logEditor;
    juce::TextButton btnClear, btnCopy, btnRestartBridge, btnRescan, btnTestConnection;
    juce::ToggleButton autoScrollToggle;
    juce::Label filterLabel;
    juce::ComboBox filterCombo;
    juce::String lastLogText_;
};

//==============================================================================
/// Top-level window that hosts the debug log content.
class DebugLogWindow : public juce::DocumentWindow
{
public:
    DebugLogWindow()
        : DocumentWindow("MaxiMeter Debug Log",
                          ThemeManager::getInstance().getPalette().windowBg,
                          DocumentWindow::closeButton)
    {
        setLookAndFeel(&titleBarLnf_);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(32);
        setContentOwned(new DebugLogContent(), false);
        setResizable(true, false);
        centreWithSize(900, 600);
    }

    ~DebugLogWindow() override
    {
        setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
    }

private:
    SkinnedTitleBarLookAndFeel titleBarLnf_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugLogWindow)
};
