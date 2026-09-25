#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>      // IShellExtInit, IContextMenu, IShellPropSheetExt
#include <olectl.h>      // SELFREG_E_CLASS
#include <strsafe.h>
#include <new>

// {B1E6A9C2-4D3F-4C21-9A77-2F5B8C1E4D0B}
static const GUID CLSID_WinLtfsShellExt =
    { 0xB1E6A9C2, 0x4D3F, 0x4C21, { 0x9A, 0x77, 0x2F, 0x5B, 0x8C, 0x1E, 0x4D, 0x0B } };

static const wchar_t* kFriendlyName = L"WinLtfs LTFS Shell Extension";
static const wchar_t* kHandlerName  = L"WinLtfsShellExt";
static const wchar_t* kApprovedKey  =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

static HINSTANCE g_hInst   = nullptr;
static LONG      g_cDllRef = 0;

enum { CMD_FORMAT = 0, CMD_CHECK = 1, CMD_EJECT = 2, CMD_COUNT = 3 };

static const char* kStNoCartridge = "No Cartridge";
static const char* kStUnsupported = "Unsupported Cartridge";
static const char* kStRepair      = "Cartridge Repair Needed";

static bool ReadDriveMarker(wchar_t letter, char* out, DWORD cbOut)
{
    wchar_t key[64];
    StringCchPrintfW(key, ARRAYSIZE(key), L"Software\\WinLtfs\\Drives\\%c", letter);
    const HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    for (int i = 0; i < 2; ++i)
    {
        HKEY h;
        if (RegOpenKeyExW(roots[i], key, 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS)
            continue;
        DWORD type = 0, cb = cbOut;
        LONG rc = RegQueryValueExA(h, nullptr, nullptr, &type, (LPBYTE)out, &cb);
        RegCloseKey(h);
        if (rc == ERROR_SUCCESS && type == REG_SZ)
        {
            out[(cb < cbOut) ? cb : cbOut - 1] = '\0';   // ensure termination
            return true;
        }
    }
    return false;
}

static bool SignalEngineEject(wchar_t letter)
{
    wchar_t name[64];
    StringCchPrintfW(name, ARRAYSIZE(name), L"Local\\WinLtfs_Eject_%c", letter);
    HANDLE h = OpenEventW(EVENT_MODIFY_STATE, FALSE, name);
    if (!h) return false;
    BOOL ok = SetEvent(h);
    CloseHandle(h);
    return ok != FALSE;
}

class ShellExt : public IShellExtInit, public IContextMenu, public IShellPropSheetExt
{
public:
    ShellExt() : m_cRef(1), m_letter(0) { m_state[0] = '\0'; InterlockedIncrement(&g_cDllRef); }
    ~ShellExt() { InterlockedDecrement(&g_cDllRef); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IShellExtInit)
            *ppv = static_cast<IShellExtInit*>(this);
        else if (riid == IID_IContextMenu)
            *ppv = static_cast<IContextMenu*>(this);
        else if (riid == IID_IShellPropSheetExt)
            *ppv = static_cast<IShellPropSheetExt*>(this);
        else { *ppv = nullptr; return E_NOINTERFACE; }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE, IDataObject* pdtobj, HKEY) override
    {
        if (!pdtobj) return E_INVALIDARG;

        FORMATETC fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg;
        if (FAILED(pdtobj->GetData(&fe, &stg))) return E_INVALIDARG;

        wchar_t path[MAX_PATH] = L"";
        HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
        UINT got = hDrop ? DragQueryFileW(hDrop, 0, path, ARRAYSIZE(path)) : 0;
        if (hDrop) GlobalUnlock(stg.hGlobal);
        ReleaseStgMedium(&stg);
        if (got == 0 || !iswalpha(path[0]) || path[1] != L':')
            return E_FAIL;

        m_letter = (wchar_t)towupper(path[0]);
        if (!ReadDriveMarker(m_letter, m_state, sizeof(m_state)))
            return E_FAIL;   // not a WinLtfs drive -> don't show
        return S_OK;
    }

    STDMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst,
                                  UINT, UINT uFlags) override
    {
        if (uFlags & (CMF_DEFAULTONLY | CMF_VERBSONLY))
            return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);

        const bool formatGray = strcmp(m_state, kStNoCartridge) == 0
                             || strcmp(m_state, kStUnsupported) == 0
                             || strcmp(m_state, kStRepair) == 0;
        const bool checkGray  = strcmp(m_state, kStRepair) != 0;   // enabled only when repair needed
        const bool ejectGray  = strcmp(m_state, kStNoCartridge) == 0;

        UINT pos = indexMenu + 1;
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_SEPARATOR, idCmdFirst + CMD_COUNT, nullptr);
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING | (formatGray ? MF_GRAYED : 0),
                    idCmdFirst + CMD_FORMAT, L"Format LTFS volume...");
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING | (checkGray ? MF_GRAYED : 0),
                    idCmdFirst + CMD_CHECK, L"Check and Repair LTFS volume");
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_STRING | (ejectGray ? MF_GRAYED : 0),
                    idCmdFirst + CMD_EJECT, L"Eject LTFS volume");
        InsertMenuW(hMenu, pos++, MF_BYPOSITION | MF_SEPARATOR, idCmdFirst + CMD_COUNT, nullptr);

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, CMD_COUNT);   // command ids 0..CMD_COUNT-1
    }

    STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici) override
    {
        if (!pici) return E_INVALIDARG;
        if (!IS_INTRESOURCE(pici->lpVerb)) return E_INVALIDARG;   // named verbs TODO
        switch (LOWORD(pici->lpVerb))
        {
        case CMD_EJECT:
            if (!SignalEngineEject(m_letter))
                MessageBoxW(pici->hwnd, L"No running WinLtfs mount is listening for eject on this drive.",
                            kFriendlyName, MB_OK | MB_ICONWARNING);
            return S_OK;
        case CMD_FORMAT:
            MessageBoxW(pici->hwnd, L"Format is not implemented in the shell extension yet.",
                        kFriendlyName, MB_OK | MB_ICONINFORMATION);
            return S_OK;
        case CMD_CHECK:
            MessageBoxW(pici->hwnd, L"Check and Repair is not implemented in the shell extension yet.",
                        kFriendlyName, MB_OK | MB_ICONINFORMATION);
            return S_OK;
        default:
            return E_INVALIDARG;
        }
    }

    STDMETHODIMP GetCommandString(UINT_PTR idCmd, UINT uType, UINT*,
                                  LPSTR pszName, UINT cchMax) override
    {
        PCWSTR help = nullptr, verb = nullptr;
        switch (idCmd)
        {
        case CMD_FORMAT: verb = L"winltfs.format"; help = L"Format this cartridge as an LTFS volume"; break;
        case CMD_CHECK:  verb = L"winltfs.check";  help = L"Check and repair the LTFS volume"; break;
        case CMD_EJECT:  verb = L"winltfs.eject";  help = L"Eject the LTFS cartridge from the tape drive"; break;
        default:         return E_INVALIDARG;
        }
        switch (uType)
        {
        case GCS_VERBW:     return StringCchCopyW((PWSTR)pszName, cchMax, verb);
        case GCS_HELPTEXTW: return StringCchCopyW((PWSTR)pszName, cchMax, help);
        default:            return E_INVALIDARG;   // ANSI variants unused
        }
    }

    // TODO: add the "LTFS" cartridge-info page here.
    STDMETHODIMP AddPages(LPFNADDPROPSHEETPAGE, LPARAM) override { return S_OK; }
    STDMETHODIMP ReplacePage(UINT, LPFNADDPROPSHEETPAGE, LPARAM) override { return E_NOTIMPL; }

private:
    LONG    m_cRef;
    wchar_t m_letter;      // drive letter of the right-clicked WinLtfs drive
    char    m_state[64];   // current state label (from the engine's marker)
};

class ClassFactory : public IClassFactory
{
public:
    ClassFactory() : m_cRef(1) { InterlockedIncrement(&g_cDllRef); }
    ~ClassFactory() { InterlockedDecrement(&g_cDllRef); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() override
    {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (c == 0) delete this;
        return c;
    }
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
    {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        ShellExt* p = new (std::nothrow) ShellExt();
        if (!p) return E_OUTOFMEMORY;
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock) InterlockedIncrement(&g_cDllRef); else InterlockedDecrement(&g_cDllRef);
        return S_OK;
    }

private:
    LONG m_cRef;
};

static LONG SetString(HKEY root, PCWSTR subkey, PCWSTR value, PCWSTR data)
{
    HKEY h;
    LONG rc = RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &h, nullptr);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(h, value, 0, REG_SZ, (const BYTE*)data,
                        (DWORD)((wcslen(data) + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return rc;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (rclsid != CLSID_WinLtfsShellExt) return CLASS_E_CLASSNOTAVAILABLE;
    ClassFactory* p = new (std::nothrow) ClassFactory();
    if (!p) return E_OUTOFMEMORY;
    HRESULT hr = p->QueryInterface(riid, ppv);
    p->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void)
{
    return g_cDllRef ? S_FALSE : S_OK;
}

STDAPI DllRegisterServer(void)
{
    wchar_t clsid[64];
    StringFromGUID2(CLSID_WinLtfsShellExt, clsid, ARRAYSIZE(clsid));

    wchar_t module[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, module, ARRAYSIZE(module)) == 0)
        return HRESULT_FROM_WIN32(GetLastError());

    wchar_t key[256];

    StringCchPrintfW(key, ARRAYSIZE(key), L"CLSID\\%s", clsid);
    if (SetString(HKEY_CLASSES_ROOT, key, nullptr, kFriendlyName) != ERROR_SUCCESS)
        return SELFREG_E_CLASS;

    StringCchPrintfW(key, ARRAYSIZE(key), L"CLSID\\%s\\InprocServer32", clsid);
    if (SetString(HKEY_CLASSES_ROOT, key, nullptr, module) != ERROR_SUCCESS)
        return SELFREG_E_CLASS;
    SetString(HKEY_CLASSES_ROOT, key, L"ThreadingModel", L"Apartment");

    StringCchPrintfW(key, ARRAYSIZE(key),
                     L"Drive\\shellex\\ContextMenuHandlers\\%s", kHandlerName);
    SetString(HKEY_CLASSES_ROOT, key, nullptr, clsid);

    StringCchPrintfW(key, ARRAYSIZE(key),
                     L"Drive\\shellex\\PropertySheetHandlers\\%s", kHandlerName);
    SetString(HKEY_CLASSES_ROOT, key, nullptr, clsid);

    // Approved list (required when the machine enforces the shell-ext allowlist).
    SetString(HKEY_LOCAL_MACHINE, kApprovedKey, clsid, kFriendlyName);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    wchar_t clsid[64];
    StringFromGUID2(CLSID_WinLtfsShellExt, clsid, ARRAYSIZE(clsid));

    wchar_t key[256];

    StringCchPrintfW(key, ARRAYSIZE(key),
                     L"Drive\\shellex\\ContextMenuHandlers\\%s", kHandlerName);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, key);

    StringCchPrintfW(key, ARRAYSIZE(key),
                     L"Drive\\shellex\\PropertySheetHandlers\\%s", kHandlerName);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, key);

    StringCchPrintfW(key, ARRAYSIZE(key), L"CLSID\\%s\\InprocServer32", clsid);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    StringCchPrintfW(key, ARRAYSIZE(key), L"CLSID\\%s", clsid);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, key);

    { // remove the Approved value
        HKEY h;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kApprovedKey, 0, KEY_SET_VALUE, &h) == ERROR_SUCCESS)
        {
            RegDeleteValueW(h, clsid);
            RegCloseKey(h);
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hInst;
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}
