#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>
#include "../../Unicode.h"
#include "Application.h"
#include "Window.h"
#include "Logger.h"

#ifdef _WIN32
#include <Windows.h>
#include <tchar.h>
#include <Sddl.h>
#include <shlobj.h>

namespace FV::Win32 {
    HHOOK keyboardHook = nullptr;
    bool disableWindowKey = true;
    uint64_t numActiveWindows = 0;

    LRESULT keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
        bool hook = disableWindowKey && numActiveWindows > 0;
        if (nCode == HC_ACTION && hook) {
            KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
            if (pkbhs->vkCode == VK_LWIN || pkbhs->vkCode == VK_RWIN) {
                static BYTE keyState[256];
                // To use window-key as regular key, update keyState.
                if (wParam == WM_KEYDOWN) {
                    GetKeyboardState(keyState);
                    keyState[pkbhs->vkCode] = 0x80;
                    SetKeyboardState(keyState);
                } else if (wParam == WM_KEYUP) {
                    GetKeyboardState(keyState);
                    keyState[pkbhs->vkCode] = 0x00;
                    SetKeyboardState(keyState);
                }
                return 1;
            }
        }
        return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
    }

    bool terminateRequested = false;
    int exitCode = 0;
    DWORD mainThreadID = 0;
    std::mutex mainLoopLock;
    std::mutex mainLoopQueueLock;
    std::vector<std::function<void()>> mainLoopQueue;

    int runApplication(FV::Application* app) {
        std::scoped_lock guard(mainLoopLock);
        auto logger = std::make_shared<Win32::Logger>();
        logger->bind(false);

        mainThreadID = ::GetCurrentThreadId();

        if (::IsDebuggerPresent() == false) {
            if (keyboardHook) {
                Log::error("Keyboard hook state invalid. (already installed?)");
                ::UnhookWindowsHookEx(keyboardHook);
                keyboardHook = nullptr;
            }

            bool installHook = false;

            if (installHook) {
                keyboardHook = ::SetWindowsHookExW(WH_KEYBOARD_LL,
                                                   keyboardHookProc,
                                                   GetModuleHandleW(0), 0);
                if (keyboardHook == nullptr) {
                    Log::error("SetWindowsHookEx Failed.");
                }
            }
        }

        // Setup process DPI
        if (::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            Log::info("Windows DPI-Awareness: DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2");
        } else if (::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
            Log::info("Windows DPI-Awareness: DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE");
        } else {
            Log::warning("Windows DPI-Awareness not set, please check application manifest.");
        }

        terminateRequested = false;
        exitCode = 0;

        if (app)
            app->initialize();

        auto timezone = std::chrono::current_zone();
        auto const initializedAt = std::chrono::system_clock::now();
        Log::info(std::format("Application initialized at: {}",
                              timezone->to_local(initializedAt)));

        MSG	msg;
        BOOL ret;
        PostMessageW(NULL, WM_NULL, 0, 0); // To process first enqueued events.
        while ((ret = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
            if (ret == -1) {
            } else {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
            if (terminateRequested == false) {
                std::unique_lock lock(mainLoopQueueLock);
                while (terminateRequested == false && mainLoopQueue.empty() == false) {
                    std::function<void()> fn = mainLoopQueue.front();
                    mainLoopQueue.erase(mainLoopQueue.begin());
                    lock.unlock();
                    fn();
                    lock.lock();
                }
                lock.unlock();

                if (terminateRequested == false) {
                } else {
                    PostQuitMessage(0);
                }
            }
        }

        if (app)
            app->finalize();

        auto const finalizedAt = std::chrono::system_clock::now();
        auto running = finalizedAt - initializedAt;

        Log::info(std::format("Application finalized at: {} ({} seconds)",
                              timezone->to_local(finalizedAt),
                              std::chrono::duration<double>(running).count()));

        if (keyboardHook)
            ::UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;

        logger->unbind();
        mainThreadID = 0;

        return exitCode;
    }

    void terminateApplication(int code) {
        postOperation([=] {
            terminateRequested = true;
            exitCode = code;
                      });
    }

    void postOperation(std::function<void()> fn) {
        std::unique_lock lock(mainLoopQueueLock);
        mainLoopQueue.push_back(fn);
        lock.unlock();

        if (mainThreadID) {
            PostThreadMessageW(mainThreadID, WM_NULL, 0, 0);
        }
    }

    std::vector<std::u8string> commandLineArguments() {
        std::vector<std::u8string> result;
        int argc = 0;
        LPWSTR* args = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
        if (args) {
            result.reserve(argc);
            for (int i = 0; i < argc; ++i) {
                std::wstring warg = args[i];
                result.push_back(u8string(warg));
            }
        }
        return result;
    }

    std::u8string environmentPath(Application::EnvironmentPath aep) {
        WCHAR path[MAX_PATH];
        auto getWindowsFolder = [&path](std::initializer_list<int> folders)->std::u8string {
            ITEMIDLIST* pidl;
            for (int fid : folders) {
                if (SHGetSpecialFolderLocation(NULL, fid | CSIDL_FLAG_CREATE, &pidl) == NOERROR &&
                    SHGetPathFromIDListW(pidl, path)) {
                    return u8string(path);
                }
            }
            return (const char8_t*)"C:\\";
        };

        switch (aep) {
        case Application::EnvironmentPath::SystemRoot:		// system root, (boot volume)
            ::GetWindowsDirectoryW(path, MAX_PATH);
            path[2] = NULL;
            return u8string(path);
            break;
        case Application::EnvironmentPath::AppRoot:			// root directory of executable
        case Application::EnvironmentPath::AppResource:
        case Application::EnvironmentPath::AppExecutable:
            for (DWORD len = ::GetModuleFileNameW(::GetModuleHandle(NULL), path, MAX_PATH); len > 0; len--) {
                if (path[len - 1] == L'\\') {
                    path[len - 1] = L'\0';
                    return u8string(path);
                }
            }
            return (const char8_t*)"C:\\";
            break;
        case Application::EnvironmentPath::AppData:			// application's data
            return getWindowsFolder({ CSIDL_APPDATA, CSIDL_LOCAL_APPDATA, CSIDL_COMMON_APPDATA });
            break;
        case Application::EnvironmentPath::UserHome:		// user's home dir
            return getWindowsFolder({ CSIDL_PROFILE, CSIDL_MYDOCUMENTS, CSIDL_DESKTOPDIRECTORY });
            break;
        case Application::EnvironmentPath::UserDocuments:	// user's documents dir
            return getWindowsFolder({ CSIDL_MYDOCUMENTS, CSIDL_PROFILE, CSIDL_DESKTOPDIRECTORY });
            break;
        case Application::EnvironmentPath::UserPreferences:	// user's setting(config) dir
            return getWindowsFolder({ CSIDL_LOCAL_APPDATA, CSIDL_APPDATA, CSIDL_PROFILE });
            break;
        case Application::EnvironmentPath::UserCache:		// user's cache dir
            return getWindowsFolder({ CSIDL_LOCAL_APPDATA, CSIDL_APPDATA, CSIDL_PROFILE });
            break;
        case Application::EnvironmentPath::UserTemp:		// user's temporary dir
            do {
                DWORD ret = ::GetTempPathW(MAX_PATH, path);
                if (ret > MAX_PATH || ret == 0)
                    return getWindowsFolder({ CSIDL_PROFILE, CSIDL_MYDOCUMENTS, CSIDL_DESKTOPDIRECTORY });
                return u8string(path);
            } while (0);
            break;
        }
        return u8string(path);
    }
}
#endif //#ifdef _WIN32
