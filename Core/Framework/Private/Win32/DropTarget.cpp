#include <string>
#include "../../Unicode.h"
#include "DropTarget.h"

#ifdef _WIN32
#include <Windows.h>

using namespace FV::Win32;

DropTarget::DropTarget(Window* win)
    : refCount(1)
    , target(win)
    , periodicUpdate(FALSE)
    , lastEffectMask(DROPEFFECT_NONE) {
}

DropTarget::~DropTarget() {
}

// *** IUnknown ***
HRESULT DropTarget::QueryInterface(REFIID riid, void** ppv) {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppv = static_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG DropTarget::AddRef() {
    return InterlockedIncrement(&refCount);
}

ULONG DropTarget::Release() {
    LONG cRef = InterlockedDecrement(&refCount);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}

HRESULT DropTarget::DragEnter(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    source.clear();
    dropAllowed = FALSE;
    lastEffectMask = DROPEFFECT_NONE;

    if (target->callback().draggingFeedback) {
        FORMATETC fmtetc = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        if (pDataObject->QueryGetData(&fmtetc) == S_OK) {
            source = filesFromDataObject(pDataObject);
            if (source.empty() == false)
                dropAllowed = TRUE;
        }
    }

    if (dropAllowed) {
        POINT pt2 = { pt.x,  pt.y };
        ScreenToClient((HWND)target->platformHandle(), &pt2);
        pt = { pt2.x, pt2.y };

        lastKeyState = grfKeyState;
        lastPosition = pt2;

        Window::DraggingState state = Window::DraggingEntered;
        Window::DragOperation op = target->callback().draggingFeedback(
            target,
            state,
            Point(float(pt2.x), float(pt2.y)),
            source);
        switch (op) {
        case Window::DragOperationCopy:
            lastEffectMask = DROPEFFECT_COPY;
            break;
        case Window::DragOperationMove:
            lastEffectMask = DROPEFFECT_MOVE;
            break;
        case Window::DragOperationLink:
            lastEffectMask = DROPEFFECT_LINK;
            break;
        default:
            lastEffectMask = DROPEFFECT_NONE;
            break;
        }
        *pdwEffect = (*pdwEffect) & lastEffectMask;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    return S_OK;
}

HRESULT DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (dropAllowed) {
        POINT pt2 = { pt.x,  pt.y };
        ScreenToClient((HWND)target->platformHandle(), &pt2);
        pt = { pt2.x, pt2.y };

        bool update = true;
        if (!periodicUpdate &&
            lastPosition.x == pt2.x &&
            lastPosition.y == pt2.y &&
            lastKeyState == grfKeyState)
            update = false;

        if (update) {
            lastKeyState = grfKeyState;
            lastPosition = pt2;

            Window::DraggingState state = Window::DraggingUpdated;
            Window::DragOperation op = target->callback().draggingFeedback(
                target,
                state,
                Point(float(pt2.x), float(pt2.y)),
                source);
            switch (op) {
            case Window::DragOperationCopy:
                lastEffectMask = DROPEFFECT_COPY;
                break;
            case Window::DragOperationMove:
                lastEffectMask = DROPEFFECT_MOVE;
                break;
            case Window::DragOperationLink:
                lastEffectMask = DROPEFFECT_LINK;
                break;
            default:
                lastEffectMask = DROPEFFECT_NONE;
                break;
            }
        }
        *pdwEffect = (*pdwEffect) & lastEffectMask;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    return S_OK;
}

HRESULT DropTarget::DragLeave() {
    if (dropAllowed) {
        Window::DraggingState state = Window::DraggingExited;
        Window::DragOperation op = target->callback().draggingFeedback(
            target,
            state,
            Point(float(lastPosition.x), float(lastPosition.y)),
            source);
    }
    source.clear();
    source.shrink_to_fit();
    lastEffectMask = DROPEFFECT_NONE;
    return S_OK;
}

HRESULT DropTarget::Drop(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    if (dropAllowed) {
        POINT pt2 = { pt.x, pt.y };
        ScreenToClient((HWND)target->platformHandle(), &pt2);
        pt = { pt2.x, pt2.y };

        Window::DraggingState state = Window::DraggingDropped;
        Window::DragOperation op = target->callback().draggingFeedback(
            target,
            state,
            Point(float(pt2.x), float(pt2.y)),
            source);
        switch (op) {
        case Window::DragOperationCopy:
            lastEffectMask = DROPEFFECT_COPY;
            break;
        case Window::DragOperationMove:
            lastEffectMask = DROPEFFECT_MOVE;
            break;
        case Window::DragOperationLink:
            lastEffectMask = DROPEFFECT_LINK;
            break;
        default:
            lastEffectMask = DROPEFFECT_NONE;
            break;
        }
        *pdwEffect = (*pdwEffect) & DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    source.clear();
    source.shrink_to_fit();
    lastEffectMask = DROPEFFECT_NONE;
    return S_OK;
}

std::vector<std::u8string> DropTarget::filesFromDataObject(IDataObject* pDataObject) {
    std::vector<std::u8string> filenames;

    FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stgm;
    if (SUCCEEDED(pDataObject->GetData(&fmte, &stgm))) {
        HDROP hdrop = reinterpret_cast<HDROP>(stgm.hGlobal);
        UINT numFiles = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < numFiles; ++i) {
            UINT len = DragQueryFileW(hdrop, i, NULL, 0);
            LPWSTR buff = (LPWSTR)malloc(sizeof(WCHAR) * (len + 2));
            UINT r = DragQueryFileW(hdrop, i, buff, len + 1);
            buff[r] = 0;

            filenames.push_back(u8string(buff));

            free(buff);
        }
        ReleaseStgMedium(&stgm);
    }

    return filenames;
}
#endif //#ifdef _WIN32
