#include "Window.h"
#include "Logger.h"
#include "Private/Win32/Win32Window.h"

using namespace FV;

Window::Window(const WindowCallback& cb)
    : _callback(cb) {
}

Window::~Window() {
}

void Window::addEventObserver(const void* ctxt, WindowEventHandler handler) {
    std::scoped_lock guard(observerLock);
    auto it = eventObservers.try_emplace(ctxt, EventHandlers{});
    it.first->second.windowEventHandler = handler;
}

void Window::addEventObserver(const void* ctxt, MouseEventHandler handler) {
    std::scoped_lock guard(observerLock);
    auto it = eventObservers.try_emplace(ctxt, EventHandlers{});
    it.first->second.mouseEventHandler = handler;
}

void Window::addEventObserver(const void* ctxt, KeyboardEventHandler handler) {
    std::scoped_lock guard(observerLock);
    auto it = eventObservers.try_emplace(ctxt, EventHandlers{});
    it.first->second.keyboardEventHandler = handler;
}

void Window::removeEventObserver(const void* ctxt) {
    std::scoped_lock guard(observerLock);
    eventObservers.erase(ctxt);
}

void Window::postMouseEvent(const MouseEvent& event) {
    std::unique_lock guard(observerLock);
    for (auto& pair : eventObservers)
        if (auto& handler = pair.second.mouseEventHandler; handler)
            handler(event);
}

void Window::postKeyboardEvent(const KeyboardEvent& event) {
    std::unique_lock guard(observerLock);
    for (auto& pair : eventObservers)
        if (auto& handler = pair.second.keyboardEventHandler; handler)
            handler(event);
}

void Window::postWindowEvent(const WindowEvent& event) {
    std::unique_lock guard(observerLock);
    for (auto& pair : eventObservers)
        if (auto& handler = pair.second.windowEventHandler; handler)
            handler(event);
}

std::shared_ptr<Window> Window::makeWindow(const std::u8string& name, Style style, const WindowCallback& cb) {
    try {
#ifdef _WIN32
        return std::make_shared<Win32Window>(name, style, cb);
#endif
    } catch (const std::exception& err) {
        Log::error(std::format("Window creation failed: {}",
                               err.what()));
    }
    return nullptr;
}
