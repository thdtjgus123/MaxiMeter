#include "CrashHandler.h"
#include <windows.h>
#include <dbghelp.h>
#include <fstream>
#include <JuceHeader.h>
#include <ctime>

#pragma comment(lib, "dbghelp.lib")

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

    static void logStackTrace(CONTEXT* ctx)
    {
        HANDLE process = GetCurrentProcess();
        HANDLE thread  = GetCurrentThread();
        SymInitialize(process, NULL, TRUE);

        STACKFRAME64 frame = {};
        frame.AddrPC.Offset    = ctx->Rip;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Rbp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Rsp;
        frame.AddrStack.Mode   = AddrModeFlat;

        log("Stack trace:");
        for (int i = 0; i < 48; ++i)
        {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread,
                             &frame, ctx, NULL,
                             SymFunctionTableAccess64, SymGetModuleBase64, NULL))
                break;

            DWORD64 addr = frame.AddrPC.Offset;
            if (addr == 0) break;

            char symbolBuf[sizeof(SYMBOL_INFO) + 256];
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuf);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen   = 255;

            DWORD64 displacement = 0;
            char line[512];

            // Try to get module name for RVA
            DWORD64 moduleBase = SymGetModuleBase64(process, addr);

            if (SymFromAddr(process, addr, &displacement, symbol))
            {
                // Try to get source file + line number
                IMAGEHLP_LINE64 lineInfo = {};
                lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                DWORD lineDisp = 0;
                if (SymGetLineFromAddr64(process, addr, &lineDisp, &lineInfo))
                {
                    snprintf(line, sizeof(line), "  [%2d] %s+0x%llX (%s:%lu)",
                             i, symbol->Name, (unsigned long long)displacement,
                             lineInfo.FileName, lineInfo.LineNumber);
                }
                else
                {
                    snprintf(line, sizeof(line), "  [%2d] %s+0x%llX (RVA 0x%llX)",
                             i, symbol->Name, (unsigned long long)displacement,
                             (unsigned long long)(addr - moduleBase));
                }
            }
            else
            {
                snprintf(line, sizeof(line), "  [%2d] 0x%p (RVA 0x%llX)",
                         i, (void*)addr,
                         (unsigned long long)(moduleBase ? addr - moduleBase : addr));
            }
            log(line);
        }

        SymCleanup(process);
    }

    LONG WINAPI UnhandledHandler(EXCEPTION_POINTERS* ep)
    {
        log("!!! CRASH DETECTED !!!");
        char buf[128];
        snprintf(buf, sizeof(buf), "Exception Code: 0x%08X at Address: %p", 
                 ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
        log(buf);

        // Capture stack trace with symbol names
        if (ep->ContextRecord)
            logStackTrace(ep->ContextRecord);

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
