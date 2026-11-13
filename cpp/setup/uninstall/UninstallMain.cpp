// Minimal Win32 uninstaller (no Qt, no dependencies).
// Removes shortcuts, registry entries and the install folder, then deletes
// itself from %TEMP%.

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>

namespace {

std::wstring modulePath()
{
    wchar_t buffer[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
    return buffer;
}

std::wstring parentDir(const std::wstring &path)
{
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(0, slash);
}

bool hasArg(const std::wstring &flag)
{
    int count = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &count);
    bool found = false;
    for (int i = 1; i < count; ++i)
        if (flag == argv[i]) found = true;
    LocalFree(argv);
    return found;
}

std::wstring argValue(int index)
{
    int count = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &count);
    std::wstring value;
    if (index < count) value = argv[index];
    LocalFree(argv);
    return value;
}

void deleteTree(const std::wstring &dir)
{
    std::wstring buffer = dir;
    buffer.push_back(L'\0');
    buffer.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = buffer.c_str();
    op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    SHFileOperationW(&op);
}

void deleteShortcuts()
{
    wchar_t appData[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
        std::wstring link = std::wstring(appData)
                + L"\\Microsoft\\Windows\\Start Menu\\Programs\\VideoCut Studio.lnk";
        DeleteFileW(link.c_str());
    }
    wchar_t profile[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH)) {
        std::wstring link = std::wstring(profile) + L"\\Desktop\\VideoCut Studio.lnk";
        DeleteFileW(link.c_str());
    }
}

void deleteRegistry()
{
    RegDeleteTreeW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VideoCutStudio");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\VideoCutStudio.Project");
    RegDeleteTreeW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\VideoCutStudio.exe");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\VideoCut\\VideoCutStudio");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\VideoCut");

    // Only drop the .vcs association if it still points at us.
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.vcs", 0, KEY_READ | KEY_WRITE, &key)
        == ERROR_SUCCESS) {
        wchar_t value[256] = {};
        DWORD size = sizeof(value);
        DWORD type = 0;
        if (RegQueryValueExW(key, L"", nullptr, &type, reinterpret_cast<LPBYTE>(value), &size)
            == ERROR_SUCCESS
            && std::wstring(value) == L"VideoCutStudio.Project") {
            RegDeleteValueW(key, L"");
        }
        RegCloseKey(key);
    }
}

void selfDelete(const std::wstring &path)
{
    std::wstring command = L"/c ping 127.0.0.1 -n 3 > nul & del /f /q \"";
    command += path;
    command += L"\"";
    ShellExecuteW(nullptr, L"open", L"cmd.exe", command.c_str(), nullptr, SW_HIDE);
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const bool silent = hasArg(L"/S") || hasArg(L"/silent");
    const bool fromTemp = hasArg(L"--from-temp");
    const std::wstring self = modulePath();

    std::wstring installDir = fromTemp ? argValue(2) : parentDir(self);
    if (installDir.empty()) return 1;

    if (!fromTemp) {
        if (!silent) {
            const int answer = MessageBoxW(
                nullptr,
                L"Remove VideoCut Studio from this computer?\n\n"
                L"Your projects and exported videos will not be deleted.",
                L"Uninstall VideoCut Studio", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
            if (answer != IDYES) return 0;
        }

        // Copy to %TEMP% so the install folder can be removed completely.
        wchar_t temp[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, temp);
        std::wstring copy = std::wstring(temp) + L"VideoCutUninstall.exe";
        CopyFileW(self.c_str(), copy.c_str(), FALSE);

        std::wstring command = L"\"" + copy + L"\" --from-temp \"" + installDir + L"\"";
        if (silent) command += L" /S";

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};
        std::wstring mutableCommand = command;
        if (CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                           nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        return 0;
    }

    deleteShortcuts();
    deleteRegistry();
    deleteTree(installDir);

    if (!silent) {
        MessageBoxW(nullptr, L"VideoCut Studio has been removed.",
                    L"Uninstall complete", MB_ICONINFORMATION | MB_OK);
    }
    selfDelete(self);
    return 0;
}
