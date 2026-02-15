#include "PythonPluginBridge.h"
#include "../UI/DebugLogWindow.h"

//==============================================================================
PythonPluginBridge& PythonPluginBridge::getInstance()
{
    static PythonPluginBridge instance;
    return instance;
}

PythonPluginBridge::~PythonPluginBridge()
{
    stop();
}

//==============================================================================
bool PythonPluginBridge::start(const juce::File& pluginsDir,
                                const juce::String& pythonExe)
{
    if (running) return true;

    // Remember parameters for restart
    lastPluginsDir_ = pluginsDir;
    lastPythonExe_  = pythonExe;

    // Build the command to launch bridge_runner.py (the proper entry point)
    auto bridgeScript = pluginsDir.getParentDirectory().getChildFile("bridge_runner.py");
    if (!bridgeScript.existsAsFile())
    {
        // Try relative to exe
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                          .getParentDirectory();
        bridgeScript = exeDir.getChildFile("CustomComponents").getChildFile("bridge_runner.py");
    }

    if (!bridgeScript.existsAsFile())
    {
        if (onError) onError("bridge_runner.py not found");
        return false;
    }

#if JUCE_WINDOWS
    // Create anonymous pipes for stdin/stdout
    SECURITY_ATTRIBUTES sa {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdinReadChild  = nullptr;
    HANDLE hStdoutWriteChild = nullptr;

    if (!CreatePipe(&hStdinReadChild, &hStdinWrite, &sa, 0))
    {
        if (onError) onError("Failed to create stdin pipe");
        return false;
    }
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&hStdoutRead, &hStdoutWriteChild, &sa, 0))
    {
        CloseHandle(hStdinReadChild);
        CloseHandle(hStdinWrite); hStdinWrite = nullptr;
        if (onError) onError("Failed to create stdout pipe");
        return false;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // Open NUL device for stderr — Python log output must NOT go to the
    // stdout pipe, otherwise it corrupts the JSON-line IPC protocol.
    HANDLE hStderrNul = CreateFileA(
        "NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    // Launch Python subprocess
    STARTUPINFOA si {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput  = hStdinReadChild;
    si.hStdOutput = hStdoutWriteChild;
    si.hStdError  = hStderrNul ? hStderrNul : hStdoutWriteChild;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi {};
    juce::String cmd = pythonExe + " -u \"" + bridgeScript.getFullPathName() + "\"";
    std::string cmdStr = cmd.toStdString();

    BOOL ok = CreateProcessA(
        nullptr, cmdStr.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr,
        &si, &pi);

    // Close child-side pipe handles (parent doesn't need them)
    CloseHandle(hStdinReadChild);
    CloseHandle(hStdoutWriteChild);
    if (hStderrNul) CloseHandle(hStderrNul);

    if (!ok)
    {
        CloseHandle(hStdinWrite);  hStdinWrite = nullptr;
        CloseHandle(hStdoutRead);  hStdoutRead = nullptr;
        if (onError) onError("Failed to start Python subprocess: " + cmd);
        return false;
    }

    hProcess = pi.hProcess;
    CloseHandle(pi.hThread);
#else
    if (onError) onError("Platform not supported yet");
    return false;
#endif

    running = true;
    MAXIMETER_LOG("BRIDGE", "Python bridge started successfully (pid=" + juce::String((int)(intptr_t)hProcess) + ")");

    // Scan plugins.  Python always auto-scans on startup (bridge_runner.py),
    // so we just need to send scan + list to fetch the manifest list.
    // Use a flag to prevent recursive restart during scan.
    if (!insideScan_)
        scanPlugins();

    return true;
}

void PythonPluginBridge::stop()
{
    if (!running) return;

    // Prevent restart logic from triggering during intentional shutdown
    restartCount_ = kMaxRestarts;

    // Send shutdown message
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "shutdown");
    sendMessage(juce::var(msg.get()));

#if JUCE_WINDOWS
    if (hProcess)
    {
        WaitForSingleObject(hProcess, 2000);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);  hProcess = nullptr;
    }
    if (hStdinWrite)  { CloseHandle(hStdinWrite);  hStdinWrite = nullptr; }
    if (hStdoutRead)  { CloseHandle(hStdoutRead);  hStdoutRead = nullptr; }
#endif

    running = false;
    cachedManifests.clear();
    restartCount_ = 0;  // Reset for next start() cycle
}

bool PythonPluginBridge::isRunning() const
{
#if JUCE_WINDOWS
    if (!running || !hProcess) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode))
        return exitCode == STILL_ACTIVE;
    return false;
#else
    return running;
#endif
}

//==============================================================================
bool PythonPluginBridge::tryRestart()
{
    // Guard against infinite restart loops
    if (restartCount_ >= kMaxRestarts)
    {
        DBG("PythonPluginBridge: max restart attempts (" + juce::String(kMaxRestarts)
            + ") reached — giving up");
        if (onError) onError("Python bridge restart failed after "
                             + juce::String(kMaxRestarts) + " attempts");
        return false;
    }

    if (!lastPluginsDir_.exists())
    {
        DBG("PythonPluginBridge: no plugins directory saved — cannot restart");
        return false;
    }

    restartCount_++;
    DBG("PythonPluginBridge: attempting restart #" + juce::String(restartCount_));
    MAXIMETER_LOG("BRIDGE", "Attempting restart #" + juce::String(restartCount_));

    // Clean up old handles (without sending shutdown — process is dead)
#if JUCE_WINDOWS
    if (hProcess)   { CloseHandle(hProcess);   hProcess = nullptr; }
    if (hStdinWrite) { CloseHandle(hStdinWrite); hStdinWrite = nullptr; }
    if (hStdoutRead) { CloseHandle(hStdoutRead); hStdoutRead = nullptr; }
#endif
    running = false;
    cachedManifests.clear();

    // Brief delay before restart to let OS clean up
    juce::Thread::sleep(200);

    // Attempt to start
    if (!start(lastPluginsDir_, lastPythonExe_))
    {
        DBG("PythonPluginBridge: restart failed in start()");
        return false;
    }

    DBG("PythonPluginBridge: restart successful (attempt #" + juce::String(restartCount_) + ")");
    MAXIMETER_LOG("BRIDGE", "Restart successful (attempt #" + juce::String(restartCount_) + ")");
    return true;
}

//==============================================================================
juce::var PythonPluginBridge::sendMessage(const juce::var& msg, DWORD reqTimeout)
{
    juce::ScopedLock sl(pipeLock);

    if (!running)
    {
        // Process not running — try to restart
        if (lastPluginsDir_.exists() && tryRestart())
        {
            // Retry the message after successful restart
        }
        else
            return {};
    }

#if JUCE_WINDOWS
    if (!hStdinWrite || !hStdoutRead) return {};

    // Serialise to JSON line
    auto jsonStr = juce::JSON::toString(msg, true) + "\n";
    auto utf8 = jsonStr.toRawUTF8();
    DWORD len = (DWORD) strlen(utf8);

    DWORD written = 0;
    if (!WriteFile(hStdinWrite, utf8, len, &written, nullptr) || written != len)
    {
        if (onError) onError("Failed to write to Python bridge pipe — attempting restart");
        running = false;
        if (tryRestart())
        {
            // Retry write after restart
            if (!WriteFile(hStdinWrite, utf8, len, &written, nullptr) || written != len)
                return {};
        }
        else
            return {};
    }
    FlushFileBuffers(hStdinWrite);

    // Read response line (blocking, with timeout).
    // IMPORTANT: Only accept lines that start with '{' as valid JSON responses.
    // Any non-JSON output (Python stderr leak, print(), etc.) is silently skipped.
    juce::String responseLine;
    auto startMs = juce::Time::getMillisecondCounter();
    // Use caller-specified timeout, or default 500ms for render, 2000ms otherwise
    const DWORD timeoutMs = (reqTimeout > 0) ? reqTimeout : 500;

    while (true)
    {
        DWORD avail = 0;
        PeekNamedPipe(hStdoutRead, nullptr, 0, nullptr, &avail, nullptr);

        if (avail > 0)
        {
            char buf[4096];
            DWORD bytesRead = 0;
            DWORD toRead = juce::jmin(avail, (DWORD)(sizeof(buf) - 1));
            if (ReadFile(hStdoutRead, buf, toRead, &bytesRead, nullptr) && bytesRead > 0)
            {
                buf[bytesRead] = '\0';
                responseLine += juce::String::fromUTF8(buf, (int) bytesRead);

                // Process all complete lines in the buffer
                while (responseLine.contains("\n"))
                {
                    auto idx = responseLine.indexOf("\n");
                    auto line = responseLine.substring(0, idx).trim();
                    responseLine = responseLine.substring(idx + 1);

                    if (line.isEmpty())
                        continue;

                    // Only accept lines that look like JSON objects
                    if (line[0] != '{')
                    {
                        // Log Python stdout/stderr that isn't JSON (likely tracebacks or print debugging)
                        // This helps diagnose why a plugin failed to load.
                        if (line.isNotEmpty())
                            MAXIMETER_LOG("PY-OUT", line);
                        
                        DBG("PythonBridge: skipping non-JSON line: " + line.substring(0, 80));
                        continue;
                    }

                    auto parsed = juce::JSON::parse(line);
                    if (parsed.isObject())
                    {
                        restartCount_ = 0;  // Reset on successful communication
                        return parsed;
                    }

                    DBG("PythonBridge: JSON parse failed for line: " + line.substring(0, 80));
                }
            }
        }

        if (juce::Time::getMillisecondCounter() - startMs > timeoutMs)
        {
            DBG("PythonBridge: sendMessage timeout (" + juce::String((int)timeoutMs) + "ms)");
            MAXIMETER_LOG("BRIDGE", "sendMessage timeout (" + juce::String((int)timeoutMs) + "ms)");
            // Don't restart on every timeout — the auto-recreate logic in
            // CustomPluginComponent will handle recovery.  Only restart if
            // the process actually died (checked below).
            return {};
        }

        // Check process is still alive
        DWORD exitCode = 0;
        if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            running = false;
            if (onError) onError("Python bridge process exited (code " + juce::String((int)exitCode) + ") — attempting restart");
            tryRestart();
            return {};
        }

        // Yield CPU time without the 15ms Sleep(1) penalty on Windows.
        // SwitchToThread gives up the rest of our timeslice to another ready
        // thread; if none are ready it returns immediately.  This brings the
        // effective polling interval from ~15ms down to ~0–1ms.
        if (!SwitchToThread())
            Sleep(0);
    }
#else
    return {};
#endif
}

//==============================================================================
void PythonPluginBridge::scanPlugins()
{
    insideScan_ = true;

    // First scan to load plugins (longer timeout — Python may be starting up)
    juce::DynamicObject::Ptr scanMsg = new juce::DynamicObject();
    scanMsg->setProperty("type", "scan");
    sendMessage(juce::var(scanMsg.get()), 3000);

    // Then request full manifest list (which includes default_size, author, etc.)
    juce::DynamicObject::Ptr listMsg = new juce::DynamicObject();
    listMsg->setProperty("type", "list");
    auto result = sendMessage(juce::var(listMsg.get()), 3000);

    insideScan_ = false;

    if (!result.isObject()) return;

    auto* resultObj = result.getDynamicObject();
    if (!resultObj) return;

    cachedManifests.clear();

    auto manifests = resultObj->getProperty("manifests");
    if (auto* arr = manifests.getArray())
    {
        for (auto& p : *arr)
            cachedManifests.push_back(parseManifest(p));
    }

    MAXIMETER_LOG("BRIDGE", "scanPlugins complete: " + juce::String((int)cachedManifests.size()) + " plugins found");
    for (auto& m : cachedManifests)
        MAXIMETER_LOG("BRIDGE", "  Plugin: " + m.name + " [" + m.id + "]  src=" + m.sourceFile);
}

std::vector<CustomPluginManifest> PythonPluginBridge::getAvailablePlugins() const
{
    return cachedManifests;
}

//==============================================================================
std::vector<CustomPluginProperty> PythonPluginBridge::createInstance(
    const juce::String& manifestId,
    const juce::String& instanceId)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "create");
    msg->setProperty("manifest_id", manifestId);
    msg->setProperty("instance_id", instanceId);

    auto result = sendMessage(juce::var(msg.get()), 2000);  // 2s timeout for create
    if (!result.isObject())
    {
        DBG("PythonBridge: createInstance failed for " + manifestId + " (no response)");
        MAXIMETER_LOG("ERROR", "createInstance FAILED for " + manifestId + " / " + instanceId + " (timeout or no response)");
        return {};
    }

    auto* resultObj = result.getDynamicObject();
    if (!resultObj) return {};

    if (resultObj->getProperty("type").toString() == "error")
    {
        juce::String errorMsg = resultObj->getProperty("message");
        MAXIMETER_LOG("BRIDGE-ERR", "createInstance error for " + manifestId + ": " + errorMsg);
        return {};
    }

    if (resultObj->getProperty("type").toString() != "created")
        return {};

    MAXIMETER_LOG("INSTANCE", "createInstance OK: " + manifestId + " / " + instanceId);

    std::vector<CustomPluginProperty> props;
    auto propsVar = resultObj->getProperty("properties");
    if (auto* arr = propsVar.getArray())
    {
        for (auto& pv : *arr)
        {
            if (auto* po = pv.getDynamicObject())
            {
                CustomPluginProperty prop;
                prop.key = po->getProperty("key").toString();
                prop.label = po->getProperty("label").toString();
                prop.type = po->getProperty("type").toString();
                prop.defaultVal = po->getProperty("default");
                prop.group = po->getProperty("group").toString();
                prop.minVal = (float)po->getProperty("min");
                prop.maxVal = (float)po->getProperty("max");
                prop.step = (float)po->getProperty("step");
                prop.description = po->getProperty("description").toString();

                // Parse ENUM choices: Python sends [["value","Label"], ...]
                if (auto* choicesArr = po->getProperty("choices").getArray())
                {
                    for (auto& cv : *choicesArr)
                    {
                        if (auto* pair = cv.getArray())
                        {
                            if (pair->size() >= 2)
                                prop.choices.add({ (*pair)[0], (*pair)[1].toString() });
                        }
                    }
                }

                props.push_back(std::move(prop));
            }
        }
    }

    return props;
}

void PythonPluginBridge::destroyInstance(const juce::String& instanceId)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "destroy");
    msg->setProperty("instance_id", instanceId);
    sendMessage(juce::var(msg.get()));
}

//==============================================================================
std::vector<PluginRender::RenderCommand> PythonPluginBridge::renderInstance(
    const juce::String& instanceId,
    int width, int height,
    const juce::String& audioJson)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "render");
    msg->setProperty("instance_id", instanceId);
    msg->setProperty("width", width);
    msg->setProperty("height", height);

    // Parse audio JSON if provided
    if (audioJson.isNotEmpty())
    {
        auto audioVar = juce::JSON::parse(audioJson);
        msg->setProperty("audio", audioVar);
    }

    auto result = sendMessage(juce::var(msg.get()));
    if (!result.isObject()) return {};

    auto* resultObj = result.getDynamicObject();
    if (!resultObj) return {};

    std::vector<PluginRender::RenderCommand> commands;
    auto cmdsVar = resultObj->getProperty("commands");
    if (auto* arr = cmdsVar.getArray())
    {
        commands.reserve(arr->size());
        for (auto& cv : *arr)
            commands.push_back(parseRenderCommand(cv));
    }

    return commands;
}

//==============================================================================
void PythonPluginBridge::setProperty(const juce::String& instanceId,
                                      const juce::String& key,
                                      const juce::var& value)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "set_property");
    msg->setProperty("instance_id", instanceId);
    msg->setProperty("key", key);
    msg->setProperty("value", value);
    sendMessage(juce::var(msg.get()));
}

void PythonPluginBridge::notifyResize(const juce::String& instanceId, int width, int height)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "resize");
    msg->setProperty("instance_id", instanceId);
    msg->setProperty("width", width);
    msg->setProperty("height", height);
    sendMessage(juce::var(msg.get()));
}

//==============================================================================
juce::String PythonPluginBridge::serialiseAll()
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "save");

    auto result = sendMessage(juce::var(msg.get()));
    if (result.isObject())
    {
        auto* obj = result.getDynamicObject();
        if (obj)
            return juce::JSON::toString(obj->getProperty("data"));
    }
    return {};
}

void PythonPluginBridge::deserialiseAll(const juce::String& jsonData)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "load");
    msg->setProperty("data", juce::JSON::parse(jsonData));
    sendMessage(juce::var(msg.get()));
}

//==============================================================================
bool PythonPluginBridge::reloadPlugin(const juce::String& manifestId)
{
    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty("type", "reload");
    msg->setProperty("manifest_id", manifestId);

    auto result = sendMessage(juce::var(msg.get()));
    if (result.isObject())
    {
        auto* obj = result.getDynamicObject();
        if (obj)
            return obj->getProperty("type").toString() == "ok";
    }
    return false;
}

//==============================================================================
CustomPluginManifest PythonPluginBridge::parseManifest(const juce::var& v)
{
    CustomPluginManifest m;
    if (auto* obj = v.getDynamicObject())
    {
        m.id            = obj->getProperty("id").toString();
        m.name          = obj->getProperty("name").toString();
        m.category      = obj->getProperty("category").toString();
        m.description   = obj->getProperty("description").toString();
        m.author        = obj->getProperty("author").toString();
        m.version       = obj->getProperty("version").toString();
        m.sourceFile    = obj->getProperty("source_file").toString();

        // Python sends "default_size": [w, h] — parse both formats
        auto sizeVar = obj->getProperty("default_size");
        if (auto* sizeArr = sizeVar.getArray())
        {
            if (sizeArr->size() >= 2)
            {
                m.defaultWidth  = (int)(*sizeArr)[0];
                m.defaultHeight = (int)(*sizeArr)[1];
            }
        }
        else
        {
            m.defaultWidth  = (int)obj->getProperty("default_width");
            m.defaultHeight = (int)obj->getProperty("default_height");
        }

        if (auto* tags = obj->getProperty("tags").getArray())
            for (auto& t : *tags)
                m.tags.add(t.toString());
    }
    return m;
}

PluginRender::RenderCommand PythonPluginBridge::parseRenderCommand(const juce::var& v)
{
    PluginRender::RenderCommand cmd;

    if (auto* obj = v.getDynamicObject())
    {
        auto cmdStr = obj->getProperty("cmd").toString();

        // Map string command type to enum
        static const std::unordered_map<std::string, PluginRender::CmdType> cmdMap = {
            {"clear",              PluginRender::CmdType::Clear},
            {"fill_rect",          PluginRender::CmdType::FillRect},
            {"stroke_rect",        PluginRender::CmdType::StrokeRect},
            {"fill_rounded_rect",  PluginRender::CmdType::FillRoundedRect},
            {"stroke_rounded_rect",PluginRender::CmdType::StrokeRoundedRect},
            {"fill_ellipse",       PluginRender::CmdType::FillEllipse},
            {"stroke_ellipse",     PluginRender::CmdType::StrokeEllipse},
            {"draw_line",          PluginRender::CmdType::DrawLine},
            {"draw_path",          PluginRender::CmdType::DrawPath},
            {"fill_path",          PluginRender::CmdType::FillPath},
            {"draw_text",          PluginRender::CmdType::DrawText},
            {"draw_image",         PluginRender::CmdType::DrawImage},
            {"fill_gradient_rect", PluginRender::CmdType::FillGradientRect},
            {"set_clip",           PluginRender::CmdType::SetClip},
            {"reset_clip",         PluginRender::CmdType::ResetClip},
            {"push_transform",     PluginRender::CmdType::PushTransform},
            {"pop_transform",      PluginRender::CmdType::PopTransform},
            {"set_opacity",        PluginRender::CmdType::SetOpacity},
            {"set_blend_mode",     PluginRender::CmdType::SetBlendMode},
            {"draw_arc",           PluginRender::CmdType::DrawArc},
            {"fill_arc",           PluginRender::CmdType::FillArc},
            {"draw_bezier",        PluginRender::CmdType::DrawBezier},
            {"fill_circle",        PluginRender::CmdType::FillCircle},
            {"stroke_circle",      PluginRender::CmdType::StrokeCircle},
            {"draw_polyline",      PluginRender::CmdType::DrawPolyline},
            {"save_state",         PluginRender::CmdType::SaveState},
            {"restore_state",      PluginRender::CmdType::RestoreState},
            {"draw_shader",        PluginRender::CmdType::DrawShader},
            {"draw_custom_shader", PluginRender::CmdType::DrawCustomShader},
        };

        auto it = cmdMap.find(cmdStr.toStdString());
        if (it != cmdMap.end())
            cmd.type = it->second;
        else
            cmd.type = PluginRender::CmdType::Clear; // fallback

        // Store all properties as params
        juce::DynamicObject::Ptr params = new juce::DynamicObject();
        for (auto& prop : obj->getProperties())
        {
            if (prop.name.toString() != "cmd")
                params->setProperty(prop.name, prop.value);
        }
        cmd.params = juce::var(params.get());
    }

    return cmd;
}

//==============================================================================
juce::String AudioDataSerialiser::buildAudioJson(/* analyser refs */)
{
    // Stub — the actual implementation depends on what analyzers
    // the caller passes. For now, CustomPluginComponent uses SharedMemory
    // or passes audio data inline from MeterFactory::feedMeter.
    return "{}";
}
