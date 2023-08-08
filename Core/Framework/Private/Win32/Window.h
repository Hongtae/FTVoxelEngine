#pragma once
#include <memory>
#include <atomic>
#include <bitset>
#include "../../Window.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV::Win32
{
    class Window : public FV::Window, public std::enable_shared_from_this<Window>
    {
    public:
        Window(const std::u8string& name, Style style, const WindowCallback& callback);
        ~Window();

		bool isActivated() const override { return activated; }
		bool isVisible() const override { return visible; }

		Rect contentBounds() const override { return bounds; }
		Rect windowFrame() const override { return frame; }
		float contentScaleFactor() const override { return scaleFactor; }
		Size resolution() const override;
		void setResolution(Size) override;

		Point origin() const override { return windowFrame().origin; }
		void setOrigin(Point) override;
		Size contentSize() const override { return bounds.size; }
		void setContentSize(Size) override;

		void show() override;
		void hide() override;
		void activate() override;
		void minimize() override;

		std::u8string title() const override;
		void setTitle(const std::u8string&) override;

		void showMouse(int deviceID, bool) override;
		bool isMouseVisible(int deviceID) const override;
		void lockMouse(int deviceID, bool) override;
		bool isMouseLocked(int deviceID) const override;
		void setMousePosition(int deviceID, Point) override;
		Point mousePosition(int deviceID) const override;
		
		bool isTextInputEnabled(int deviceID) const override;
		void enableTextInput(int deviceID, bool) override;
		bool keyState(int deviceID, VirtualKey) override;
		void setKeyState(int deviceID, VirtualKey, bool) override;
		void resetKeyStates(int deviceID) override;

		void* platformHandle() const override { return hWnd; }

	private:
		void resetMouse();
		void resetKeyStates();
		void synchronizeKeyStates();
		void synchronizeMouse();
		void destroy();

		static LRESULT windowProc(HWND, UINT, WPARAM, LPARAM);

		class DropTarget* dropTarget;
		HWND hWnd;
		Style style;
		Rect bounds;
		Rect frame;
		float scaleFactor;
		std::u8string name;

		bool activated;
		bool visible;
		bool minimized;
		bool resizing;
		bool autoResize;

		bool textCompositionMode;
		bool mouseLocked;

		Point mousePos;
		Point lockedMousePos;

		int keyboardID;
		int mouseID;

		std::bitset<size_t(VirtualKey::MaxValue)> keyboardStates;

		union {
			uint8_t buttons;
			struct {
				uint8_t button1 : 1;
				uint8_t button2 : 1;
				uint8_t button3 : 1;
				uint8_t button4 : 1;
				uint8_t button5 : 1;
				uint8_t button6 : 1;
				uint8_t button7 : 1;
				uint8_t button8 : 1;
			};
		} mouseButtonDown;
    };
}
#endif //#ifdef _WIN32
