#pragma once
#include <vector>
#include <string>
#include "Win32Window.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV {
    class Win32DropTarget : public IDropTarget {
    public:
        Win32DropTarget(Win32Window* target);
        ~Win32DropTarget();

        // *** IUnknown ***
        STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        // *** IDropTarget ***
        STDMETHODIMP DragEnter(IDataObject* pdto, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect);
        STDMETHODIMP DragOver(DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect);
        STDMETHODIMP DragLeave();
        STDMETHODIMP Drop(IDataObject* pdto, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect);

        std::vector<std::u8string> filesFromDataObject(IDataObject*);

    private:
        POINT lastPosition;
        DWORD lastKeyState;
        DWORD lastEffectMask;
        BOOL dropAllowed;
        BOOL periodicUpdate;
        LONG refCount;
        std::vector<std::u8string> source;
        Win32Window* target;
    };
}
#endif //#ifdef _WIN32
