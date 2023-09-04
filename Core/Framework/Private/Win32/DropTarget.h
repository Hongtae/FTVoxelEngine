#pragma once
#include <vector>
#include <string>
#include "Window.h"

#ifdef _WIN32
#include <Windows.h>

namespace FV::Win32 {
    class DropTarget : public IDropTarget {
    public:
        DropTarget(Window* target);
        ~DropTarget();

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
        Window* target;
    };
}
#endif //#ifdef _WIN32
