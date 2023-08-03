#include "Application.h"
#include "Window.h"
#include "../../Logger.h"

#ifdef _WIN32
#include <Windows.h>


namespace FV::Win32
{
    HHOOK keyboardHook = nullptr;
    bool disableWindowKey = true;
    uint64_t numActiveWindows = 0;

    LRESULT keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        bool hook = disableWindowKey && numActiveWindows > 0;
        if (nCode == HC_ACTION && hook)
        {
            KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
            if (pkbhs->vkCode == VK_LWIN || pkbhs->vkCode == VK_RWIN)
            {
                static BYTE keyState[256];
                // To use window-key as regular key, update keyState.
                if (wParam == WM_KEYDOWN)
                {
                    GetKeyboardState(keyState);
                    keyState[pkbhs->vkCode] = 0x80;
                    SetKeyboardState(keyState);
                }
                else if (wParam == WM_KEYUP)
                {
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

    int runApplication(FV::Application* app)
    {
        std::scoped_lock guard(mainLoopLock);

        mainThreadID = ::GetCurrentThreadId();

        if (::IsDebuggerPresent() == false)
        {
            if (keyboardHook)
            {
                Log::error("Keyboard hook state invalid. (already installed?)");
                ::UnhookWindowsHookEx(keyboardHook);
                keyboardHook = nullptr;
            }

            bool installHook = false;

            if (installHook)
            {
                keyboardHook = ::SetWindowsHookExW(WH_KEYBOARD_LL,
                                                   keyboardHookProc,
                                                   GetModuleHandleW(0), 0);
                if (keyboardHook == nullptr)
                {
                    Log::error("SetWindowsHookEx Failed.");
                }
            }
        }

        // Setup process DPI
        if (::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        {
            Log::info("Windows DPI-Awareness: DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2");
        }
        else if (::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE))
        {
            Log::info("Windows DPI-Awareness: DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE");
        }
        else
        {
            Log::warning("Windows DPI-Awareness not set, please check application manifest.");
        }

        terminateRequested = false;
        exitCode = 0;

        if (app)
            app->initialize();

        UINT_PTR timerID = 0;
        MSG	msg;
        BOOL ret;

        PostMessageW(NULL, WM_NULL, 0, 0); // To process first enqueued events.
        while ((ret = GetMessageW(&msg, nullptr, 0, 0)) != 0)
        {
            if (ret == -1)
            {
            }
            else
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
            if (terminateRequested == false)
            {
                std::unique_lock lock(mainLoopQueueLock);
                while (terminateRequested == false && mainLoopQueue.empty() == false)
                {
                    std::function<void()> fn = mainLoopQueue.front();
                    mainLoopQueue.erase(mainLoopQueue.begin());
                    lock.unlock();
                    fn();
                    lock.lock();
                }
                lock.unlock();

                if (terminateRequested == false)
                {
                }
                else
                {
                    PostQuitMessage(0);
                }
            }
        }
        if (timerID)
            ::KillTimer(nullptr, timerID);

        if (app)
            app->finalize();

        if (keyboardHook)
            ::UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;

        mainThreadID = 0;

        return exitCode;
    }

    void terminateApplication(int code)
    {
        postOperation([=]{
            terminateRequested = true;
            exitCode = code;
        });
    }

    void postOperation(std::function<void()> fn)
    {
        std::unique_lock lock(mainLoopQueueLock);
        mainLoopQueue.push_back(fn);
        lock.unlock();

        if (mainThreadID)
        {
            PostThreadMessageW(mainThreadID, WM_NULL, 0, 0);
        }
    }
}
#endif //#ifdef _WIN32
