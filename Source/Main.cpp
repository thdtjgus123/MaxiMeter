#include <JuceHeader.h>
#include "UI/MainWindow.h"
#include "UI/SkeuomorphicLookAndFeel.h"
#include <fstream>
#include <csignal>
#include <windows.h>

// Global terminate/abort handler to log crash info
namespace CrashGuard {
    inline void logCrash(const char* reason)
    {
        auto p = juce::File::getSpecialLocation(
                     juce::File::currentApplicationFile)
                     .getParentDirectory()
                     .getChildFile("MaxiMeter_crash.log");
        std::ofstream f(p.getFullPathName().toStdString(), std::ios::app);
        f << "*** CRASH: " << reason << " ***\n";
        f.flush();
    }

    inline void terminateHandler()
    {
        logCrash("std::terminate called");
        if (auto eptr = std::current_exception())
        {
            try { std::rethrow_exception(eptr); }
            catch (const std::exception& e) {
                std::string msg = "  exception: ";
                msg += e.what();
                logCrash(msg.c_str());
            }
            catch (...) {
                logCrash("  unknown exception type");
            }
        }
        std::_Exit(1);
    }

    inline void abortHandler(int)
    {
        logCrash("SIGABRT received");
        std::_Exit(1);
    }

    inline LONG WINAPI sehHandler(EXCEPTION_POINTERS* ep)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "SEH exception code=0x%08lX addr=0x%p",
                 ep->ExceptionRecord->ExceptionCode,
                 ep->ExceptionRecord->ExceptionAddress);
        logCrash(buf);
        std::_Exit(1);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline void install()
    {
        std::set_terminate(terminateHandler);
        std::signal(SIGABRT, abortHandler);
        SetUnhandledExceptionFilter(sehHandler);
    }
}

//==============================================================================
/// MaxiMeter Application — entry point.
class MaxiMeterApplication : public juce::JUCEApplication,
                             public ThemeManager::Listener
{
public:
    MaxiMeterApplication() {}

    const juce::String getApplicationName() override    { return "MaxiMeter"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    //==========================================================================
    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);

        CrashGuard::install();

        // Apply skeuomorphic look-and-feel
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

        ThemeManager::getInstance().addListener(this);

        mainWindow = std::make_unique<MainWindow>("MaxiMeter");
    }

    void shutdown() override
    {
        mainWindow.reset();
        ThemeManager::getInstance().removeListener(this);
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    //==========================================================================
    void themeChanged(AppTheme /*newTheme*/) override
    {
        lookAndFeel.updateFromTheme();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {}

private:
    std::unique_ptr<MainWindow> mainWindow;
    SkeuomorphicLookAndFeel lookAndFeel;
};

//==============================================================================
// Generate the main() function
START_JUCE_APPLICATION(MaxiMeterApplication)
