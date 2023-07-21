#pragma once
#include "../include.h"
#include <functional>
#include "Rect.h"
#include "Vector2.h"
#include "VirtualKey.h"

namespace FV
{
    class Window
    {
    public:
        virtual ~Window() {}

		/// Window Style
		enum Style : uint32_t
		{
			StyleTitle = 1,
			StyleCloseButton = 1 << 1,
			StyleMinimizeButton = 1 << 2,
			StyleMaximizeButton = 1 << 3,
			StyleResizableBorder = 1 << 4,
			StyleAutoResize = 1 << 5,	///< resize on rotate or DPI change, etc.
			StyleGenericWindow = 0xff,	///< includes all but StyleAcceptFileDrop
			StyleAcceptFileDrop = 1 << 8, ///< enables file drag & drop
		};

		/// Mouse, (Multi) Touch, Table Stylus pen event.
		struct MouseEvent
		{
			enum Type
			{
				ButtonDown = 0,
				ButtonUp,
				Move,
				Wheel,
				Pointing,
			};
			enum Device
			{
				GenericMouse = 0,
				Stylus,
				Touch,
				Unknown,
			};
			Type type;
			std::weak_ptr<Window> window;
			Device device;
			int deviceId;
			int buttonId;
			Point location;	///< location in window
			Vector2 delta;
			float pressure;		///< for stylus-pen
			float tilt;			///< radian value of stylus-pen and screen angle. 0 is parallel, PI/2 is perpendicular to the surface.
		};

		/// Keyboard and text event.
		struct KeyboardEvent
		{
			enum Type
			{
				KeyDown = 0,
				KeyUp,
				TextInput,
				TextComposition, ///< text composition in progress
			};
			Type type;
			std::weak_ptr<Window> window;
			int deviceId;
			VirtualKey key;
			std::wstring text;
		};

		/// Window event, reposition, resize etc.
		struct WindowEvent
		{
			enum Type
			{
				WindowCreated = 0,	///< To receive this event, you must start the window event system asynchronously using 'DKEventLoop'.
				WindowClosed,
				WindowHidden,
				WindowShown,
				WindowActivated,
				WindowInactivated,
				WindowMinimized,
				WindowMoved,
				WindowResized,
				WindowUpdate,
			};
			Type type;
			std::weak_ptr<Window> window;
			Rect windowRect;	///< on parent space coords (can be screen space)
			Rect contentRect;	///< on parent space coords (can be screen space)
			float contentScaleFactor;
		};

		using WindowEventHandler = std::function<void(const WindowEvent&)>;
		using KeyboardEventHandler = std::function<void(const KeyboardEvent&)>;
		using MouseEventHandler = std::function<void(const MouseEvent&)>;

		/// drag and drop (files only)
		enum DraggingState
		{
			DraggingEntered = 0,
			DraggingUpdated,
			DraggingExited,
			DraggingDropped,
		};
		/// drag and drop operation
		enum DragOperation
		{
			DragOperationNone = 0,	///< drag & drop not allowed
			DragOperationCopy,		///< Inform the user that a copy operation will be performed.
			DragOperationMove,		///< Inform the user that a move operation will be performed.
			DragOperationLink,		///< Inform the user that a link operation will be performed.
		};

		/// Window Callback
		/// Callback function is required for some events that cannot be processed asynchronously.
		struct WindowCallback
		{
			using DragOperationCallback = std::function<DragOperation(Window*, DraggingState, const Point&, const std::vector<std::string>&)>;
			DragOperationCallback draggingFeedback;
			std::function<Size(Window*)> contentMinSize;
			std::function<Size(Window*)> contentMaxSize;
			std::function<bool(Window*)> closeRequest;
		};

		virtual bool isActivated() const = 0;
		virtual bool isVisible() const = 0;
		virtual Rect contentBounds() const = 0;
		virtual Rect windowFrame() const = 0;
		virtual float contentScaleFactor() const = 0;
		virtual void setOrigin(const Point&) = 0;
		virtual std::string title() const = 0;
		virtual void setTitle(const std::string&) = 0;
		virtual Size resolution() const = 0;

		virtual bool isTextInputEnabled(int deviceID) const = 0;
		virtual void enableTextInput(int deviceID) = 0;
		virtual bool keyState(int deviceID, VirtualKey) = 0;
		virtual void setKeyState(int deviceID, VirtualKey, bool) = 0;
		virtual void resetKeyStates(int deviceID) = 0;

		virtual void show() = 0;
		virtual void hide() = 0;
		virtual void activate() = 0;
		virtual void minimize() = 0;

		virtual void addEventObserver(const void*, WindowEventHandler) = 0;
		virtual void addEventObserver(const void*, MouseEventHandler) = 0;
		virtual void addEventObserver(const void*, KeyboardEventHandler) = 0;
		virtual void removeEventObserver(const void*) = 0;

		virtual void postMouseEvent(const MouseEvent&) = 0;
		virtual void postKeyboardEvent(const KeyboardEvent&) = 0;
		virtual void postWindowEvent(const WindowEvent&) = 0;

		virtual void* platformHandle() const = 0;
	};
}
