// CleanupCA.cpp - install/uninstall custom actions for PopKiller.
//  * DeleteLeftovers (uninstall only): delete autostart entries + runtime data files
//  * BackupRuntimeData / RestoreRuntimeData: preserve runtime data across a major upgrade,
//    even when the package being replaced has no upgrade guard.
// Avoids <msi.h>; declares the single MSI API used and links msi.lib.
#include <windows.h>
#include <string>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "msi.lib")

typedef unsigned long MSIHANDLE;
extern "C" UINT WINAPI MsiGetPropertyW(MSIHANDLE hInstall, LPCWSTR szName, LPWSTR szValueBuf, LPDWORD pchValueBuf);

static std::wstring Join(const std::wstring& dir, const wchar_t* name)
{
    std::wstring p = dir;
    if (!p.empty() && p.back() != L'\\') p += L'\\';
    p += name;
    return p;
}

static void EnsureDir(const std::wstring& dir) { ::CreateDirectoryW(dir.c_str(), nullptr); }

static void CopyIfExists(const std::wstring& src, const std::wstring& dst)
{
    if (::GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    ::SetFileAttributesW(dst.c_str(), FILE_ATTRIBUTE_NORMAL);
    ::CopyFileW(src.c_str(), dst.c_str(), FALSE);
}

static void DeleteTree(const std::wstring& dir)
{
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(Join(dir, L"*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            std::wstring full = Join(dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) DeleteTree(full);
            else { ::SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL); ::DeleteFileW(full.c_str()); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    ::RemoveDirectoryW(dir.c_str());
}

static void DeleteMask(const std::wstring& dir, const wchar_t* mask)
{
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(Join(dir, mask).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            std::wstring full = Join(dir, fd.cFileName);
            ::SetFileAttributesW(full.c_str(), FILE_ATTRIBUTE_NORMAL);
            ::DeleteFileW(full.c_str());
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void DeleteRunValue(HKEY root, const wchar_t* subkey)
{
    HKEY k = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS)
    {
        RegDeleteValueW(k, L"PopKiller");
        RegCloseKey(k);
    }
}

static void DeleteStartupShortcuts()
{
    const wchar_t* sub = L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\PopKiller.lnk";
    wchar_t base[1024]{};
    if (GetEnvironmentVariableW(L"APPDATA", base, ARRAYSIZE(base)))
        ::DeleteFileW((std::wstring(base) + sub).c_str());
    if (GetEnvironmentVariableW(L"ProgramData", base, ARRAYSIZE(base)))
        ::DeleteFileW((std::wstring(base) + sub).c_str());
}

// Custom action data: "<installDir>|<backupDir>"
static bool GetTwoDirs(MSIHANDLE hInstall, std::wstring& a, std::wstring& b)
{
    wchar_t buf[32768]{};
    DWORD n = ARRAYSIZE(buf);
    if (MsiGetPropertyW(hInstall, L"CustomActionData", buf, &n) != ERROR_SUCCESS) return false;
    std::wstring s = buf;
    auto p = s.find(L'|');
    if (p == std::wstring::npos) return false;
    a = s.substr(0, p);
    b = s.substr(p + 1);
    return !a.empty() && !b.empty();
}

static void CopyStaticML(const std::wstring& from, const std::wstring& to, bool onlyMissing)
{
    EnsureDir(to);
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(Join(from, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring dst = Join(to, fd.cFileName);
        if (onlyMissing && ::GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        CopyIfExists(Join(from, fd.cFileName), dst);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

extern "C" __declspec(dllexport) UINT __stdcall DeleteLeftovers(MSIHANDLE hInstall)
{
    DeleteRunValue(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    DeleteRunValue(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    DeleteStartupShortcuts();

    wchar_t buf[32768]{};
    DWORD n = ARRAYSIZE(buf);
    if (MsiGetPropertyW(hInstall, L"CustomActionData", buf, &n) == ERROR_SUCCESS && n > 0)
    {
        std::wstring dir = buf;
        const wchar_t* files[] = {
            L"rules.json", L"rules.json.bak", L"winui.ini", L"blocklog.txt",
            L"labels.json", L"crash.log", L"autostart_debug.log", L"notifydiag.txt"
        };
        for (auto f : files)
        {
            std::wstring p = Join(dir, f);
            ::SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
            ::DeleteFileW(p.c_str());
        }
        DeleteMask(dir, L"*.dmp");
        DeleteMask(dir, L"*.log");
        DeleteTree(Join(dir, L"StaticML"));
    }
    return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) UINT __stdcall BackupRuntimeData(MSIHANDLE hInstall)
{
    std::wstring dir, bak;
    if (!GetTwoDirs(hInstall, dir, bak)) return ERROR_SUCCESS;
    DeleteTree(bak);
    EnsureDir(bak);
    const wchar_t* files[] = { L"rules.json", L"rules.json.bak", L"labels.json", L"winui.ini" };
    for (auto f : files) CopyIfExists(Join(dir, f), Join(bak, f));
    CopyStaticML(Join(dir, L"StaticML"), Join(bak, L"StaticML"), false);
    return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) UINT __stdcall RestoreRuntimeData(MSIHANDLE hInstall)
{
    std::wstring dir, bak;
    if (!GetTwoDirs(hInstall, dir, bak)) return ERROR_SUCCESS;
    if (::GetFileAttributesW(bak.c_str()) == INVALID_FILE_ATTRIBUTES) return ERROR_SUCCESS;
    EnsureDir(dir);
    const wchar_t* files[] = { L"rules.json", L"rules.json.bak", L"labels.json", L"winui.ini" };
    for (auto f : files)
    {
        std::wstring dst = Join(dir, f);
        if (::GetFileAttributesW(dst.c_str()) == INVALID_FILE_ATTRIBUTES)
            CopyIfExists(Join(bak, f), dst);
    }
    CopyStaticML(Join(bak, L"StaticML"), Join(dir, L"StaticML"), true);
    DeleteTree(bak);
    return ERROR_SUCCESS;
}
