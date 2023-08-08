#include <cmath>
#include <algorithm>
#include "../../Logger.h"
#include "../../Unicode.h"
#include "Window.h"
#include "VirtualKey.h"
#include "DropTarget.h"
#include "Application.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV::Win32 {
    constexpr wchar_t FV_WindowClass[] = L"FVWindowClass";
    // timer id, interval
    constexpr UINT_PTR TimerID_UpdateKeyboardMouse = 10;
    constexpr UINT UpdateKeyboardMouseInterval = 10;
    // window messages
    constexpr UINT FV_WM_SHOWCURSOR = (WM_USER + 0x1100);
    constexpr UINT FV_WM_UPDATEMOUSECAPTURE = (WM_USER + 0x1110);

    extern uint64_t numActiveWindows;
}

using namespace FV::Win32;

namespace 
{
    float dpiScaleForWindow(HWND hWnd)
    {
        if (UINT dpi = ::GetDpiForWindow(hWnd); dpi)
        {
            return float(dpi) / 96.0f;
        }
        return 1.0f;
    }

    std::u8string win32ErrorString(DWORD code)
    {
        LPVOID lpMsgBuf;
        ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, code,
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, nullptr);

        std::wstring ret = (const wchar_t*)lpMsgBuf;
        ::LocalFree(lpMsgBuf);
        return FV::toUTF8(ret);
    }
}

Window::Window(const std::u8string& title, Style s, const WindowCallback& cb)
    : FV::Window(cb)
    , hWnd(nullptr)
    , dropTarget(nullptr)
    , name(title)
    , style(s)
    , bounds(Rect::zero)
    , frame(Rect::zero)
    , scaleFactor(1.0f)
    , activated(false)
    , visible(false)
    , minimized(false)
    , resizing(false)
    , autoResize(false)
    , textCompositionMode(false)
    , mouseLocked(false)
    , mousePos(Point::zero)
    , lockedMousePos(Point::zero)
    , keyboardID(0)
    , mouseID(0)
    , mouseButtonDown{ 0 }
{
    ::OleInitialize(nullptr);

    static const WNDCLASSW	wc = {
        CS_OWNDC,
        (WNDPROC)windowProc,
        0,
        0,
        ::GetModuleHandleW(nullptr),
        LoadIconW(nullptr, IDI_APPLICATION),
        LoadCursorW(nullptr, IDC_ARROW),
        nullptr,
        nullptr,
        FV_WindowClass
    };
    static ATOM a = RegisterClassW(&wc);

    if (a == 0)
        FVASSERT_THROW("Failed to register WndClass");

    DWORD dwStyle = 0;
    if (style & Window::StyleTitle)
        dwStyle |= WS_CAPTION;
    if (style & Window::StyleCloseButton)
        dwStyle |= WS_SYSMENU;
    if (style & Window::StyleMinimizeButton)
        dwStyle |= WS_MINIMIZEBOX;
    if (style & Window::StyleMaximizeButton)
        dwStyle |= WS_MAXIMIZEBOX;
    if (style & Window::StyleResizableBorder)
        dwStyle |= WS_THICKFRAME;

    DWORD dwStyleEx = 0;

    hWnd = CreateWindowExW(dwStyleEx, FV_WindowClass,
                           (const wchar_t*)toUTF16(name).c_str(), dwStyle,
                           CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                           nullptr, nullptr, GetModuleHandleW(nullptr), 0);

    if (hWnd == nullptr)
    {
        Log::error("CreateWindow failed.\n");
        throw std::runtime_error("CreateWindow failed!");
    }

    ::SetLastError(0);

    if (!::SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)this))
    {
        DWORD err = ::GetLastError();
        if (err)
        {
            // error!
            Log::error(std::format(
                "SetWindowLongPtr failed with error {:d}, {}",
                err, win32ErrorString(err)));

            ::DestroyWindow(hWnd);
            hWnd = nullptr;

            throw std::runtime_error("SetWindowLongPtr failed!");
        }
    }

    if (style & Window::StyleAcceptFileDrop)
    {
        DropTarget* dropTarget = new DropTarget(this);
        HRESULT err = RegisterDragDrop(hWnd, dropTarget);
        if (err == S_OK)
        {
            this->dropTarget = dropTarget;
        }
        else
        {
            delete dropTarget;

            Log::error(std::format(
                "RegisterDragDrop failed: {}", win32ErrorString(err)));

            throw std::runtime_error("RegisterDragDrop failed");
        }
    }
    if (style & Window::StyleAutoResize)
        this->autoResize = true;

    RECT rc1, rc2;
    ::GetClientRect(hWnd, &rc1);
    ::GetWindowRect(hWnd, &rc2);

    float scaleFactor = dpiScaleForWindow(hWnd);
    this->scaleFactor = scaleFactor;

    float invScale = 1.0f / scaleFactor;
    this->bounds = Rect(rc1.left,
                        rc1.top,
                        float(rc1.right - rc1.left) * invScale,
                        float(rc1.bottom - rc1.top) * invScale);
    this->frame = Rect(rc2.left, rc2.top, rc2.right - rc2.left, rc2.bottom - rc2.top);

    ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    ::SetTimer(hWnd, TimerID_UpdateKeyboardMouse, UpdateKeyboardMouseInterval, nullptr);
}

Window::~Window()
{
    if (hWnd)
    {
        ::KillTimer(hWnd, TimerID_UpdateKeyboardMouse);
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        ::PostMessageW(hWnd, UINT(WM_CLOSE), 0, 0);
    }
    ::OleUninitialize();
}

void Window::destroy()
{
    activated = false;
    if (hWnd)
    {
        if (this->dropTarget)
        {
            RevokeDragDrop(hWnd);
            ULONG ref = this->dropTarget->Release();
            this->dropTarget = nullptr;
            if (ref > 0)
            {
                Log::warning(std::format(
                    "DropTarget for Window:{} in use! ref-count:{:d}", name, ref
                ));
            }
        }

        ::KillTimer(hWnd, TimerID_UpdateKeyboardMouse);

        // set GWLP_USERDATA to 0, to forwarding messages to DefWindowProc.
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        // Post WM_CLOSE to destroy window from DefWindowProc().
        ::PostMessageW(hWnd, WM_CLOSE, 0, 0);

        postWindowEvent({
                WindowEvent::WindowClosed,
                weak_from_this(),
                frame,
                bounds,
                scaleFactor });

        hWnd = nullptr;
    }
}

FV::Size Window::resolution() const
{
    return bounds.size * scaleFactor;
}

void Window::setResolution(Size size)
{
    if (hWnd)
    {
        int w = std::max(int(std::round(size.width)), 1);
        int h = std::max(int(std::round(size.height)), 1);

        DWORD style = ((DWORD)GetWindowLong(hWnd, GWL_STYLE));
        DWORD styleEx = ((DWORD)GetWindowLong(hWnd, GWL_EXSTYLE));
        BOOL menu = ::GetMenu(hWnd) != NULL;

        RECT rc = { 0, 0, w, h };
        if (::AdjustWindowRectEx(&rc, style, menu, styleEx))
        {
            Size size = Size(w, h);
            this->bounds.size = size / scaleFactor;

            w = rc.right - rc.left;
            h = rc.bottom - rc.top;
            ::SetWindowPos(hWnd, HWND_TOP, 0, 0, w, h,
                           SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        }
    }
}

void Window::setOrigin(Point origin)
{
    if (hWnd)
    {
        int x = std::round(origin.x);
        int y = std::round(origin.y);
        ::SetWindowPos(hWnd, HWND_TOP, x, y, 0, 0, 
                       SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }
}

void Window::setContentSize(Size s)
{
    setResolution(s * scaleFactor);
}

void Window::show() 
{
    if (hWnd)
    {
        if (::IsIconic(hWnd))
            ::ShowWindow(hWnd, SW_RESTORE);
        else
            ::ShowWindow(hWnd, SW_SHOWNA);
    }
}

void Window::hide() 
{
    if (hWnd)
        ::ShowWindow(hWnd, SW_HIDE);
}

void Window::activate() 
{
    if (hWnd)
    {
        if (::IsIconic(hWnd))
            ::ShowWindow(hWnd, SW_RESTORE);

        ::ShowWindow(hWnd, SW_SHOW);
        ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ::SetForegroundWindow(hWnd);
    }
}

void Window::minimize() 
{
    if (hWnd)
        ::ShowWindow(hWnd, SW_MINIMIZE);
}

std::u8string Window::title() const 
{
    if (hWnd)
    {
        int len = GetWindowTextLengthW(hWnd);
        if (len > 0) {
            std::wstring title;
            title.resize(len + 1);
            ::GetWindowTextW(hWnd, title.data(), len + 1);
            return toUTF8(title);
        }
        return {};
    }
    return name; 
}

void Window::setTitle(const std::u8string& title)
{
    if (hWnd)
    {
        ::SetWindowTextW(hWnd, (const wchar_t*)toUTF16(title).c_str());
    }
    name = title;
}

void Window::showMouse(int deviceID, bool show)
{
    if (hWnd && deviceID == mouseID)
    {
        WPARAM wParam = show ? WPARAM(1) : WPARAM(0);
        ::PostMessageW(hWnd, FV_WM_SHOWCURSOR, wParam, 0);
    }
}

bool Window::isMouseVisible(int deviceID) const 
{
    if (deviceID == mouseID)
    {
        CURSORINFO info = {};
        if (::GetCursorInfo(&info))
            return info.flags != 0;
    }
    return false;
}

void Window::lockMouse(int deviceID, bool lock) 
{
    if (deviceID == mouseID && hWnd)
    {
        this->mouseLocked = lock;
        this->mousePos = this->mousePosition(deviceID);
        this->lockedMousePos = this->mousePos;

        ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
    }
}

bool Window::isMouseLocked(int deviceID) const 
{
    if (deviceID == mouseID)
        return mouseLocked;
    return false;
}

void Window::setMousePosition(int deviceID, Point pos)
{
    if (hWnd && deviceID == mouseID)
    {
        POINT pt = {
            LONG(std::round(pos.x)),
            LONG(std::round(pos.y))
        };
        ::ClientToScreen(hWnd, &pt);
        Point v = Point(pt.x, pt.y) * scaleFactor;
        ::SetCursorPos(std::round(v.x), std::round(v.y));

        this->mousePos = pos;
    }
}

FV::Point Window::mousePosition(int deviceID) const 
{
    if (hWnd && deviceID == mouseID)
    {
        POINT pt;
        ::GetCursorPos(&pt);
        ::ScreenToClient(hWnd, &pt);
        return Point(pt.x, pt.y) / scaleFactor;
    }
    return Point(-1, -1);
}

void Window::resetKeyStates()
{
    for (int i = 0; i < 256; ++i)
    {
        if (i == VK_CAPITAL)
            continue;

        VirtualKey key = getVirtualKey(i);
        if (key == VirtualKey::None)
            continue;

        std::size_t index = std::size_t(key);

        if (keyboardStates[index])
        {
            postKeyboardEvent({
                KeyboardEvent::KeyUp,
                weak_from_this(), 0, key, u8"" });
        }
    }

    if (keyboardStates[std::size_t(VirtualKey::Capslock)])
    {
        postKeyboardEvent({
            KeyboardEvent::KeyUp,
            weak_from_this(), 0, VirtualKey::Capslock, u8"" });
    }

    BYTE tmp[256];
    ::GetKeyboardState(tmp);	// to empty keyboard queue

    keyboardStates.reset();
}

void Window::resetMouse()
{
    if (hWnd)
    {
        POINT ptMouse;
        ::GetCursorPos(&ptMouse);
        ::ScreenToClient(hWnd, &ptMouse);
        mousePos = Point(ptMouse.x, ptMouse.y) / scaleFactor;
    }
}

bool Window::isTextInputEnabled(int deviceID) const 
{
    if (deviceID == keyboardID)
        return textCompositionMode;
    return false;
}

void Window::enableTextInput(int deviceID, bool enable) 
{
    if (deviceID == keyboardID)
    {
        textCompositionMode = enable;
    }
}

bool Window::keyState(int deviceID, VirtualKey k) 
{
    if (deviceID == keyboardID && k > VirtualKey::None && k < VirtualKey::MaxValue)
    {
        return keyboardStates[std::size_t(k)];
    }
    return false;
}

void Window::setKeyState(int deviceID, VirtualKey k, bool down) 
{
    if (deviceID == keyboardID && k > VirtualKey::None && k < VirtualKey::MaxValue)
    {
        keyboardStates.set(std::size_t(k), down);
    }
}

void Window::resetKeyStates(int deviceID) 
{
    if (deviceID == keyboardID)
    {
        resetKeyStates();
    }
}

void Window::synchronizeKeyStates()
{
    if (activated == false) return;

    BYTE keyStateCurrent[256] = {0};	// key-state buffer
    ::GetKeyboardState(keyStateCurrent);

    for (int i = 0; i < 256; i++)
    {
        if (i == VK_CAPITAL)
            continue;

        VirtualKey key = getVirtualKey(i);
        if (key == VirtualKey::None)
            continue;

        std::size_t index = std::size_t(key);

        if (keyStateCurrent[i] & 0x80)
        {
            if (keyboardStates[index] == false)
            {
                postKeyboardEvent({
                    KeyboardEvent::KeyDown,
                    weak_from_this(), 0, key, u8"" });

                keyboardStates.set(index);
            }
        }
        else
        {
            if (keyboardStates[index])
            {
                postKeyboardEvent({
                    KeyboardEvent::KeyUp,
                    weak_from_this(), 0, key, u8"" });

                keyboardStates.reset(index);
            }
        }
    }

    if (keyStateCurrent[VK_CAPITAL] & 0x01)
    {
        if (keyboardStates[std::size_t(VirtualKey::Capslock)] == false)
        {
            postKeyboardEvent({
                KeyboardEvent::KeyDown,
                weak_from_this(), 0, VirtualKey::Capslock, u8"" });

            keyboardStates.set(std::size_t(VirtualKey::Capslock));
        }
    }
    else
    {
        if (keyboardStates[std::size_t(VirtualKey::Capslock)])
        {
            postKeyboardEvent({
                KeyboardEvent::KeyUp,
                weak_from_this(), 0, VirtualKey::Capslock, u8"" });

            keyboardStates.reset(std::size_t(VirtualKey::Capslock));
        }
    }
}

void Window::synchronizeMouse()
{
    if (this->activated == false) return;

    // check mouse has gone out of window region.
    if (hWnd && GetCapture() != hWnd)
    {
        POINT pt;
        ::GetCursorPos(&pt);
        ::ScreenToClient(hWnd, &pt);

        RECT rc;
        ::GetClientRect(hWnd, &rc);

        if (pt.x < rc.left || pt.x > rc.right || pt.y > rc.bottom || pt.y < rc.top)
        {
            ::PostMessageW(hWnd, UINT(WM_MOUSEMOVE), 0, MAKELPARAM(pt.x, pt.y));
        }
    }
}

LRESULT Window::windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (hWnd)
    {
        Window* window = (Window*)::GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        if (window && window->hWnd == hWnd)
        {
            auto postWindowEvent = [&](WindowEvent::Type type)
            {
                window->postWindowEvent({
                type,
                window->weak_from_this(),
                window->frame,
                window->bounds,
                window->scaleFactor });
            };

            switch (uMsg)
            {
            case WM_ACTIVATE:
                if (wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE)
                {
                    if (window->activated == false)
                    {
                        numActiveWindows += 1;
                        window->activated = true;
                        postWindowEvent(WindowEvent::WindowActivated);
                        window->resetKeyStates();
                        window->resetMouse();  // to prevent mouse cursor popped.
                    }
                }
                else
                {
                    if (window->activated)
                    {
                        numActiveWindows -= 1;
                        window->resetKeyStates();	// release all keys
                        window->resetMouse();
                        window->activated = false;
                        postWindowEvent(WindowEvent::WindowInactivated);
                    }
                }
                return 0;
            case WM_SHOWWINDOW:
                if (wParam)
                {
                    if (window->visible == false)
                    {
                        window->visible = true;
                        window->minimized = false;
                        postWindowEvent(WindowEvent::WindowShown);
                    }
                }
                else
                {
                    if (window->visible)
                    {
                        window->visible = false;
                        postWindowEvent(WindowEvent::WindowHidden);
                    }
                }
                break;
            case WM_ENTERSIZEMOVE:
                window->resizing = true;
                return 0;
            case WM_EXITSIZEMOVE:
                window->resizing = false;
                do {
                    RECT rcClient, rcWindow;
                    ::GetClientRect(hWnd, &rcClient);
                    ::GetWindowRect(hWnd, &rcWindow);
                    bool resized = false;
                    bool moved = false;
                    if ((rcClient.right - rcClient.left) != (LONG)std::round(window->bounds.size.width) ||
                        (rcClient.bottom - rcClient.top) != (LONG)std::round(window->bounds.size.height))
                        resized = true;

                    if (rcWindow.left != (LONG)std::round(window->frame.origin.x) ||
                        rcWindow.top != (LONG)std::round(window->frame.origin.y))
                        moved = true;

                    if (resized || moved)
                    {
                        window->frame = Rect(rcWindow.left, rcWindow.top, rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top);
                        window->bounds = Rect(rcClient.left, rcClient.top, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
                        if (resized)
                            postWindowEvent(WindowEvent::WindowResized);
                        if (moved)
                            postWindowEvent(WindowEvent::WindowMoved);
                    }
                } while (0);
                return 0;
            case WM_SIZE:
                if (wParam == SIZE_MAXHIDE)
                {
                    if (window->visible)
                    {
                        window->visible = false;
                        postWindowEvent(WindowEvent::WindowHidden);
                    }
                }
                else if (wParam == SIZE_MINIMIZED)
                {
                    if (window->minimized == false)
                    {
                        window->minimized = true;
                        postWindowEvent(WindowEvent::WindowMinimized);
                    }
                }
                else
                {
                    if (window->minimized || window->visible == false)
                    {
                        window->minimized = false;
                        window->visible = true;
                        postWindowEvent(WindowEvent::WindowShown);
                    }
                    else
                    {
                        Size size = Size(LOWORD(lParam), HIWORD(lParam));
                        window->bounds.size = size / window->scaleFactor;

                        RECT rc;
                        ::GetWindowRect(hWnd, &rc);
                        window->frame = Rect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
                        postWindowEvent(WindowEvent::WindowResized);
                    }
                }
                return 0;
            case WM_MOVE:
                if (window->resizing == false)
                {
                    int x = (int)(short)LOWORD(lParam);   // horizontal position 
                    int y = (int)(short)HIWORD(lParam);   // vertical position 
                    window->frame.origin = Point(x, y);
                    postWindowEvent(WindowEvent::WindowMoved);
                }
                return 0;
            case WM_DPICHANGED:
                do {
                    // Note: xDPI, yDPI are identical for Windows apps
                    int xDPI = HIWORD(wParam);
                    int yDPI = HIWORD(wParam);
                    RECT* suggestedWindowRect = (RECT*)lParam;

                    window->scaleFactor = double(xDPI) / 96.0;

                    if (window->autoResize)
                    {
                        ::SetWindowPos(hWnd,
                                       NULL,
                                       suggestedWindowRect->left,
                                       suggestedWindowRect->top,
                                       suggestedWindowRect->right - suggestedWindowRect->left,
                                       suggestedWindowRect->bottom - suggestedWindowRect->top,
                                       SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                    else
                    {
                        RECT clientRect;
                        RECT windowRect;
                        ::GetClientRect(hWnd, &clientRect);
                        ::GetWindowRect(hWnd, &windowRect);

                        window->frame = Rect(windowRect.left,
                                             windowRect.top,
                                             windowRect.right - windowRect.left,
                                             windowRect.bottom - windowRect.top);

                        float invScale = 1.0 / window->scaleFactor;
                        window->bounds = Rect(clientRect.left,
                                              clientRect.top,
                                              float(clientRect.right - clientRect.left) * invScale,
                                              float(clientRect.bottom - clientRect.top) * invScale);

                        postWindowEvent(WindowEvent::WindowResized);
                    }
                } while (0);
                return 0;
            case WM_GETMINMAXINFO:
                do {
                    const WindowCallback& cb = window->callback();

                    DWORD style = ((DWORD)GetWindowLong(hWnd, GWL_STYLE));
                    DWORD styleEx = ((DWORD)GetWindowLong(hWnd, GWL_EXSTYLE));
                    BOOL menu = ::GetMenu(hWnd) != NULL;

                    Size s = { 1, 1 };
                    if (cb.contentMinSize)
                        s = cb.contentMinSize(window);
                    LONG w = std::round(s.width);
                    LONG h = std::round(s.height);
                    RECT rc = { 0, 0, std::max(w, 1L), std::max(h, 1L) };

                    if (::AdjustWindowRectEx(&rc, style, menu, styleEx))
                    {
                        MINMAXINFO* mm = (MINMAXINFO*)lParam;
                        mm->ptMinTrackSize.x = rc.right - rc.left;
                        mm->ptMinTrackSize.y = rc.bottom - rc.top;
                    }
                    if (cb.contentMaxSize)
                    {
                        Size s = cb.contentMaxSize(window);
                        LONG w = std::round(s.width);
                        LONG h = std::round(s.height);
                        RECT rc = { 0, 0, std::max(w, 1L), std::max(h, 1L) };

                        if (::AdjustWindowRectEx(&rc, style, menu, styleEx))
                        {
                            MINMAXINFO* mm = (MINMAXINFO*)lParam;
                            if (s.width > 0)
                                mm->ptMaxTrackSize.x = rc.right - rc.left;
                            if (s.height > 0)
                                mm->ptMaxTrackSize.y = rc.bottom - rc.top;
                        }
                    }
                } while (0);
                return 0;
            case WM_TIMER:
                if (wParam == TimerID_UpdateKeyboardMouse)
                {
                    window->synchronizeKeyStates();
                    window->synchronizeMouse();
                    return 0;
                }
                break;
            case WM_MOUSEMOVE:
                if (window->activated)
                {
                    POINTS pt = MAKEPOINTS(lParam);
                    float scaleFactor = window->scaleFactor;
                    LONG oldPtX = LONG(std::round(window->mousePos.x * scaleFactor));
                    LONG oldPtY = LONG(std::round(window->mousePos.y * scaleFactor));
                    if (pt.x != oldPtX || pt.y != oldPtY)
                    {
                        Point delta = (Point(pt.x, pt.y) - window->mousePos) / scaleFactor;

                        bool broadcast = true;
                        if (window->mouseLocked)
                        {
                            LONG lockedX = LONG(std::round(window->lockedMousePos.x * scaleFactor));
                            LONG lockedY = LONG(std::round(window->lockedMousePos.y * scaleFactor));
                            if (pt.x == lockedX && pt.y == lockedY)
                            {
                                broadcast = false;
                            }
                            else
                            {
                                window->setMousePosition(window->mouseID, window->mousePos);
                                window->lockedMousePos = window->mousePosition(window->mouseID);
                            }
                        }
                        else
                        {
                            window->mousePos = Point(pt.x, pt.y) / scaleFactor;
                        }

                        if (broadcast)
                        {
                            window->postMouseEvent({
                                MouseEvent::Move,
                                window->weak_from_this(),
                                MouseEvent::GenericMouse,
                                window->mouseID,
                                0, /* buttonID */
                                window->mousePos,
                                delta,
                                0, 0 });
                        }
                    }
                }
                return 0;
            case WM_LBUTTONDOWN:
                do {
                    window->mouseButtonDown.button1 = true;
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    window->postMouseEvent({
                        MouseEvent::ButtonDown,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        0,
                        pos,
                        Point::zero,
                        0, 0 });
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return 0;
            case WM_LBUTTONUP:
                do {
                    window->mouseButtonDown.button1 = false;
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    window->postMouseEvent({
                        MouseEvent::ButtonUp,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        0,
                        pos,
                        Point::zero,
                        0, 0 });
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return 0;
            case WM_RBUTTONDOWN:
                do {
                    window->mouseButtonDown.button2 = true;
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    window->postMouseEvent({
                        MouseEvent::ButtonDown,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        1,
                        pos,
                        Point::zero,
                        0, 0 });
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return 0;
            case WM_RBUTTONUP:
                do {
                    window->mouseButtonDown.button2 = false;
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    window->postMouseEvent({
                        MouseEvent::ButtonUp,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        1,
                        pos,
                        Point::zero,
                        0,0 });
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return 0;
            case WM_MBUTTONDOWN:
                do {
                    window->mouseButtonDown.button3 = true;
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    window->postMouseEvent({
                        MouseEvent::ButtonDown,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        2,
                        pos,
                        Point::zero,
                        0, 0 });
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return 0;
            case WM_MBUTTONUP:
                do {
                    window->mouseButtonDown.button3 = false;
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    window->postMouseEvent({
                        MouseEvent::ButtonUp,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        2,
                        pos,
                        Point::zero,
                        0, 0 });
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return 0;
            case WM_XBUTTONDOWN:
                do {
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    WORD button = GET_XBUTTON_WPARAM(wParam);
                    if (button == XBUTTON1)
                    {
                        window->mouseButtonDown.button4 = true;
                        window->postMouseEvent({
                            MouseEvent::ButtonDown,
                            window->weak_from_this(),
                            MouseEvent::GenericMouse,
                            window->mouseID,
                            3,
                            pos,
                            Point::zero,
                            0,
                            0 });
                    }
                    else if (button == XBUTTON2)
                    {
                        window->mouseButtonDown.button5 = true;
                        window->postMouseEvent({
                            MouseEvent::ButtonDown,
                            window->weak_from_this(),
                            MouseEvent::GenericMouse,
                            window->mouseID,
                            4,
                            pos,
                            Point::zero,
                            0, 0 });
                    }
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return TRUE;
            case WM_XBUTTONUP:
                do {
                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;
                    WORD button = GET_XBUTTON_WPARAM(wParam);
                    if (button == XBUTTON1)
                    {
                        window->mouseButtonDown.button4 = false;
                        window->postMouseEvent({
                            MouseEvent::ButtonUp,
                            window->weak_from_this(),
                            MouseEvent::GenericMouse,
                            window->mouseID,
                            3,
                            pos,
                            Point::zero,
                            0, 0 });
                    }
                    else if (button == XBUTTON2)
                    {
                        window->mouseButtonDown.button5 = false;
                        window->postMouseEvent({
                            MouseEvent::ButtonUp,
                            window->weak_from_this(),
                            MouseEvent::GenericMouse,
                            window->mouseID,
                            4,
                            pos,
                            Point::zero,
                            0, 0 });
                    }
                    ::PostMessageW(hWnd, FV_WM_UPDATEMOUSECAPTURE, 0, 0);
                } while (0);
                return TRUE;
            case WM_MOUSEWHEEL:
                do {
                    POINT origin = { 0, 0 };
                    ::ClientToScreen(hWnd, &origin);

                    POINTS pts = MAKEPOINTS(lParam);
                    Point pos = Point(pts.x, pts.y) / window->scaleFactor;

                    int deltaY = GET_WHEEL_DELTA_WPARAM(wParam);
                    float deltaYScaled = float(deltaY) / window->scaleFactor;

                    window->postMouseEvent({
                        MouseEvent::Wheel,
                        window->weak_from_this(),
                        MouseEvent::GenericMouse,
                        window->mouseID,
                        2,
                        pos,
                        Point(0, deltaYScaled),
                        0, 0 });
                } while (0);
                return 0;
            case WM_CHAR:
                window->synchronizeKeyStates(); // synchronize key states
                if (window->textCompositionMode)
                {
                    wchar_t c[2] = { (wchar_t)wParam };
                    window->postKeyboardEvent({
                        KeyboardEvent::TextInput,
                        window->weak_from_this(),
                        window->keyboardID, 
                        VirtualKey::None,
                        toUTF8(c) });
                }
                return 0;
            case WM_IME_STARTCOMPOSITION:
            case WM_IME_ENDCOMPOSITION:
                return 0;
            case WM_IME_COMPOSITION:
                window->synchronizeKeyStates();

                if (lParam & GCS_RESULTSTR) // composition finished.
                {
                    // Result characters will be received via WM_CHAR,
                    // reset input-candidate characters here.
                    window->postKeyboardEvent({
                        KeyboardEvent::TextComposition, 
                        window->weak_from_this(),
                        window->keyboardID,
                        VirtualKey::None, u8""});
                }
                if (lParam & GCS_COMPSTR) // composition in progress.
                {
                    HIMC hIMC = ImmGetContext(hWnd);
                    if (hIMC)
                    {
                        if (window->textCompositionMode)
                        {
                            LONG textLength = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, 0, 0);
                            if (textLength)
                            {
                                unsigned char* tmp = (unsigned char*)malloc(textLength + 4);
                                memset(tmp, 0, textLength + 4);

                                ImmGetCompositionStringW(hIMC, GCS_COMPSTR, tmp, textLength + 2);
                                std::wstring compositionText = (const wchar_t*)tmp;
                                free(tmp);

                                window->postKeyboardEvent({
                                    KeyboardEvent::TextComposition,
                                    window->weak_from_this(),
                                    window->keyboardID,
                                    VirtualKey::None, toUTF8(compositionText) });
                            }
                            else // composition character's length become 0. (erased)
                            {
                                window->postKeyboardEvent({
                                    KeyboardEvent::TextComposition,
                                    window->weak_from_this(),
                                    window->keyboardID,
                                    VirtualKey::None, u8""});
                            }
                        }
                        else // not text-input mode.
                        {
                            ImmNotifyIME(hIMC, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
                        }
                        ImmReleaseContext(hWnd, hIMC);
                    }
                }
                break;
            case WM_PAINT:
                if (window->resizing == false)
                {
                    postWindowEvent(WindowEvent::WindowUpdate);
                }
                break;
            case WM_CLOSE:
                do {
                    bool close = true;
                    const WindowCallback& cb = window->callback();
                    if (cb.closeRequest)
                        close = cb.closeRequest(window);

                    if (close)
                        window->destroy();
                } while (0);
                return 0;
            case WM_COMMAND:
                break;
            case WM_SYSCOMMAND:
                switch (wParam)
                {
                case SC_CONTEXTHELP:	// help menu
                case SC_KEYMENU:		// alt-key
                case SC_HOTKEY:			// hotkey
                    return 0;
                    break;
                }
                break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return 0;				// block ALT-key
            case WM_KEYDOWN:
            case WM_KEYUP:
                return 0;
            case FV_WM_SHOWCURSOR:
                if (true) {
                    // If we need to control mouse position from other thread,
                    // we should call AttachThreadInput() to synchronize threads.
                    // but we are not going to control position, but control visibility
                    // only, we can use window message.
                    if ((BOOL)wParam)
                        while (::ShowCursor(TRUE) < 0);
                    else
                        while (::ShowCursor(FALSE) >= 0);
                }
                return 0;
            case FV_WM_UPDATEMOUSECAPTURE:
                if (::GetCapture() == hWnd)
                {
                    if (window->mouseButtonDown.buttons == 0 && !window->mouseLocked)
                        ::ReleaseCapture();
                }
                else
                {
                    if (window->mouseButtonDown.buttons != 0 || window->mouseLocked)
                        ::SetCapture(hWnd);
                }
                return 0;
            default:
                break;
            }
        }
    }
    return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
#endif //#ifdef _WIN32
