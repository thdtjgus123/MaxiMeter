#pragma once

#include <functional>
#include <string>

namespace CrashHandler
{
    using CrashCallback = std::function<void()>;

    /** Use this to install the global exception handler. 
        Pass a callback generic enough to save the project state. 
    */
    void init(CrashCallback saveCallback);

    /** Log a message to the crash log file immediately. */
    void log(const std::string& msg);

    /** Helper to get the path to the crash recovery file. */
    std::string getRecoveryFilePath();
}
