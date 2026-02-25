#pragma once
#include <JuceHeader.h>

class LogWindow : public juce::DocumentWindow
{
public:
    LogWindow() : DocumentWindow("Application Logs",
        juce::Colours::black,
        DocumentWindow::closeButton | DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(&logComponent, true);
        setResizable(true, false);
        setAlwaysOnTop(true);
        centreWithSize(600, 400);
        // setVisible(true); // Don't show by default
    }
    
    void closeButtonPressed() override { setVisible(false); }
    
    void postMessage(const juce::String& msg) {
        logComponent.logMessage(msg);
    }

private:
    struct LogComponent : public juce::Component, public juce::Logger
    {
        juce::TextEditor editor;
        LogComponent()
        {
            editor.setMultiLine(true);
            editor.setReadOnly(true);
            editor.setScrollbarsShown(true);
            editor.setFont({ "Consolas", 14.f, juce::Font::plain });
            editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
            editor.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
            addAndMakeVisible(editor);
            
            // Register as the global logger
            juce::Logger::setCurrentLogger(this);
        }
        
        ~LogComponent() override { juce::Logger::setCurrentLogger(nullptr); }

        void resized() override { editor.setBounds(getLocalBounds()); }

        void logMessage(const juce::String& message) override
        {
             // Use callAsync to be thread-safe — guard with SafePointer to avoid
             // use-after-free if LogComponent is destroyed before the lambda fires.
             juce::Component::SafePointer<LogComponent> safeThis(this);
             juce::MessageManager::callAsync([safeThis, msg = message]() {
                if (safeThis == nullptr) return;

                // Auto-show the window on log usage
                if (auto* w = safeThis->findParentComponentOfClass<LogWindow>())
                    w->setVisible(true);
                    
                safeThis->editor.moveCaretToEnd();
                safeThis->editor.insertTextAtCaret(msg + "\n");
             });
        }
    } logComponent;
};
