#include "CrashHandler.h"
#include <windows.h>
#include <fstream>
#include <JuceHeader.h>
#include <ctime>

namespace CrashHandler
{
    static CrashCallback g_saveCallback;

    std::string getRecoveryFilePath()
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                       .getParentDirectory()
                       .getChildFile("MaxiMeter_Recovery.mmproj");
        return file.getFullPathName().toStdString();
    }

    void log(const std::string& msg)
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                       .getParentDirectory()
                       .getChildFile("MaxiMeter_CrashLog.txt");
        
        std::ofstream f(file.getFullPathName().toStdString(), std::ios::app);
        
        char timeBuf[64];
        std::time_t t = std::time(nullptr);
        std::strftime(timeBuf, sizeof(timeBuf), "[%Y-%m-%d %H:%M:%S] ", std::localtime(&t));
        
        f << timeBuf << msg << "\n";
    }

    LONG WINAPI UnhandledHandler(EXCEPTION_POINTERS* ep)
    {
        log("!!! CRASH DETECTED !!!");
        char buf[128];
        snprintf(buf, sizeof(buf), "Exception Code: 0x%08X at Address: %p", 
                 ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
        log(buf);

        if (g_saveCallback)
        {
            log("Attempting emergency save...");
            try
            {
                g_saveCallback();
                log("Emergency save completed successfully.");
            }
            catch (...)
            {
                log("Failed to save during crash handling.");
            }
        }
        else
        {
            log("No save callback registered.");
        }
        
        log("Terminating process.");

        // Show a message box so the user knows
        MessageBoxA(NULL, "MaxiMeter has crashed.\n\nA recovery file 'MaxiMeter_Recovery.mmproj' has been attempted.", "MaxiMeter Crash", MB_OK | MB_ICONERROR);

        return EXCEPTION_EXECUTE_HANDLER; // Proceed to terminate
    }

    void init(CrashCallback saveCallback)
    {
        g_saveCallback = saveCallback;
        SetUnhandledExceptionFilter(UnhandledHandler);
        log("Crash Handler Installed.");
    }
}
