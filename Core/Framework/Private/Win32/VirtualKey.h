#pragma once
#include "../../VirtualKey.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV::Win32 {
    inline VirtualKey getVirtualKey(int key) {
        switch (key) {
        case 0x03:  return VirtualKey::F15;         // VK_F15 (ctrl+break)
        case 0x08:  return VirtualKey::Backspace;   // VK_BACK
        case 0x09:  return VirtualKey::Tab;         // VK_TAB
        case 0x0D:  return VirtualKey::Return;      // VK_RETURN

        //case 0x10:  return VirtualKey::SHIFT;       // VK_SHIFT
        //case 0x11:  return VirtualKey::CONTROL;     // VK_CONTROL
        //case 0x12:  return VirtualKey::OPTION;      // VK_MENU

        case 0x13:  return VirtualKey::F15;         // VK_PAUSE
        case 0x14:  return VirtualKey::Capslock;    // VK_CAPITAL, CAPSLOCK
        case 0x1B:  return VirtualKey::Escape;      // VK_ESCAPE
        case 0x1C:  return VirtualKey::None;        // VK_CONVERT
        case 0x1D:  return VirtualKey::None;        // VK_NONCONVERT
        case 0x1E:  return VirtualKey::None;        // VK_ACCEPT
        case 0x1F:  return VirtualKey::None;        // VK_MODECHANGE
        case 0x20:  return VirtualKey::Space;       // VK_SPACE
        case 0x21:  return VirtualKey::PageUp;      // VK_PRIOR
        case 0x22:  return VirtualKey::PageDown;    // VK_NEXT
        case 0x23:  return VirtualKey::End;         // VK_END
        case 0x24:  return VirtualKey::Home;        // VK_HOME
        case 0x25:  return VirtualKey::Left;        // VK_LEFT
        case 0x26:  return VirtualKey::Up;          // VK_UP
        case 0x27:  return VirtualKey::Right;       // VK_RIGHT
        case 0x28:  return VirtualKey::Down;        // VK_DOWN
        case 0x29:  return VirtualKey::None;        // VK_SELECT
        case 0x2A:  return VirtualKey::None;        // VK_PRINT
        case 0x2B:  return VirtualKey::None;        // VK_EXECUTE
        case 0x2C:  return VirtualKey::F13;         // VK_SNAPSHOT, PRINT SCREEN KEY
        case 0x2D:  return VirtualKey::Insert;      // VK_INSERT
        case 0x2E:  return VirtualKey::Delete;      // VK_DELETE
        case 0x2F:  return VirtualKey::None;        // VK_HELP
        case 0x30:  return VirtualKey::Num0;        // 0
        case 0x31:  return VirtualKey::Num1;        // 1
        case 0x32:  return VirtualKey::Num2;        // 2
        case 0x33:  return VirtualKey::Num3;        // 3
        case 0x34:  return VirtualKey::Num4;        // 4
        case 0x35:  return VirtualKey::Num5;        // 5
        case 0x36:  return VirtualKey::Num6;        // 6
        case 0x37:  return VirtualKey::Num7;        // 7
        case 0x38:  return VirtualKey::Num8;        // 8
        case 0x39:  return VirtualKey::Num9;        // 9

        case 0x41:  return VirtualKey::A;           // A
        case 0x42:  return VirtualKey::B;           // B
        case 0x43:  return VirtualKey::C;           // C
        case 0x44:  return VirtualKey::D;           // D
        case 0x45:  return VirtualKey::E;           // E
        case 0x46:  return VirtualKey::F;           // F
        case 0x47:  return VirtualKey::G;           // G
        case 0x48:  return VirtualKey::H;           // H
        case 0x49:  return VirtualKey::I;           // I
        case 0x4A:  return VirtualKey::J;           // J
        case 0x4B:  return VirtualKey::K;           // K
        case 0x4C:  return VirtualKey::L;           // L
        case 0x4D:  return VirtualKey::M;           // M
        case 0x4E:  return VirtualKey::N;           // N
        case 0x4F:  return VirtualKey::O;           // O
        case 0x50:  return VirtualKey::P;           // P
        case 0x51:  return VirtualKey::Q;           // Q
        case 0x52:  return VirtualKey::R;           // R
        case 0x53:  return VirtualKey::S;           // S
        case 0x54:  return VirtualKey::T;           // T
        case 0x55:  return VirtualKey::U;           // U
        case 0x56:  return VirtualKey::V;           // V
        case 0x57:  return VirtualKey::W;           // W
        case 0x58:  return VirtualKey::X;           // X
        case 0x59:  return VirtualKey::Y;           // Y
        case 0x5A:  return VirtualKey::Z;           // Z
        case 0x5B:  return VirtualKey::LeftCommand; // VK_LWIN
        case 0x5C:  return VirtualKey::RightCommand;// VK_RWIN
        case 0x5D:  return VirtualKey::None;        // VK_APPS
        case 0x5F:  return VirtualKey::None;        // VK_SLEEP
        case 0x60:  return VirtualKey::Pad0;        // VK_NUMPAD0
        case 0x61:  return VirtualKey::Pad1;        // VK_NUMPAD1
        case 0x62:  return VirtualKey::Pad2;        // VK_NUMPAD2
        case 0x63:  return VirtualKey::Pad3;        // VK_NUMPAD3
        case 0x64:  return VirtualKey::Pad4;        // VK_NUMPAD4
        case 0x65:  return VirtualKey::Pad5;        // VK_NUMPAD5
        case 0x66:  return VirtualKey::Pad6;        // VK_NUMPAD6
        case 0x67:  return VirtualKey::Pad7;        // VK_NUMPAD7
        case 0x68:  return VirtualKey::Pad8;        // VK_NUMPAD8
        case 0x69:  return VirtualKey::Pad9;        // VK_NUMPAD9
        case 0x6A:  return VirtualKey::PadAsterisk; // VK_MULTIPLY
        case 0x6B:  return VirtualKey::PadPlus;     // VK_ADD
        case 0x6C:  return VirtualKey::None;        // VK_SEPARATOR
        case 0x6D:  return VirtualKey::PadMinus;    // VK_SUBTRACT
        case 0x6E:  return VirtualKey::PadPeriod;   // VK_DECIMAL
        case 0x6F:  return VirtualKey::PadSlash;    // VK_DIVIDE
        case 0x70:  return VirtualKey::F1;          // VK_F1
        case 0x71:  return VirtualKey::F2;          // VK_F2
        case 0x72:  return VirtualKey::F3;          // VK_F3
        case 0x73:  return VirtualKey::F4;          // VK_F4
        case 0x74:  return VirtualKey::F5;          // VK_F5
        case 0x75:  return VirtualKey::F6;          // VK_F6
        case 0x76:  return VirtualKey::F7;          // VK_F7
        case 0x77:  return VirtualKey::F8;          // VK_F8
        case 0x78:  return VirtualKey::F9;          // VK_F9
        case 0x79:  return VirtualKey::F10;         // VK_F10
        case 0x7A:  return VirtualKey::F11;         // VK_F11
        case 0x7B:  return VirtualKey::F12;         // VK_F12
        case 0x7C:  return VirtualKey::F13;         // VK_F13
        case 0x7D:  return VirtualKey::F14;         // VK_F14
        case 0x7E:  return VirtualKey::F15;         // VK_F15
        case 0x7F:  return VirtualKey::F16;         // VK_F16
        case 0x80:  return VirtualKey::F17;         // VK_F17
        case 0x81:  return VirtualKey::F18;         // VK_F18
        case 0x82:  return VirtualKey::F19;         // VK_F19
        case 0x83:  return VirtualKey::F20;         // VK_F20
        case 0x84:  return VirtualKey::None;        // VK_F21
        case 0x85:  return VirtualKey::None;        // VK_F22
        case 0x86:  return VirtualKey::None;        // VK_F23
        case 0x87:  return VirtualKey::None;        // VK_F24

        case 0x90: return VirtualKey::Numlock;      // VK_NUMLOCK
        case 0x91: return VirtualKey::F14;          // VK_SCROLL, SCROLL LOCK

        case 0xA0: return VirtualKey::LeftShift;    // VK_LSHIFT
        case 0xA1: return VirtualKey::RightShift;   // VK_RSHIFT
        case 0xA2: return VirtualKey::LeftControl;  // VK_LCONTROL
        case 0xA3: return VirtualKey::RightControl; // VK_RCONTROL
        case 0xA4: return VirtualKey::LeftOption;   // VK_LMENU
        case 0xA5: return VirtualKey::RightOption;  // VK_RMENU

        case 0xBA: return VirtualKey::Semicolon;    // VK_OEM_1, ;
        case 0xBB: return VirtualKey::Equal;        // VK_OEM_PLUS, =
        case 0xBC: return VirtualKey::Comma;        // VK_OEM_COMMA, .
        case 0xBD: return VirtualKey::Hyphen;       // VK_OEM_MINUS, -
        case 0xBE: return VirtualKey::Period;       // VK_OEM_PERIOD
        case 0xBF: return VirtualKey::Slash;        // VK_OEM_2, /?
        case 0xC0: return VirtualKey::AccentTilde;  // VK_OEM_3, `~

        case 0xDB: return VirtualKey::OpenBracket;  // VK_OEM_4, [
        case 0xDC: return VirtualKey::Backslash;    // VK_OEM_5, backslash
        case 0xDD: return VirtualKey::CloseBracket; // VK_OEM_6, ]
        case 0xDE: return VirtualKey::Quote;        // VK_OEM_7, '
        case 0xDF: return VirtualKey::None;         // VK_OEM_8

        case 0xE2: return VirtualKey::Backslash;    // VK_OEM_102, backslash for 102-keyboard

        case 0xE5: return VirtualKey::None;         // VK_PROCESSKEY, IME-key

        default: return VirtualKey::None;

        }
        return VirtualKey::None;
    }
}

#endif //#ifdef _WIN32
