// Tiny native launcher (no Qt): unpacks the setup UI from the appended
// bootstrap zip into %TEMP%, runs it, then cleans up.

#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kTrailerSize = 72;

std::wstring modulePath()
{
    wchar_t buffer[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
    return buffer;
}

bool readTrailer(const std::wstring &exe, unsigned long long *bootOffset,
                 unsigned long long *bootSize)
{
    HANDLE file = CreateFileW(exe.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size = {};
    GetFileSizeEx(file, &size);
    LARGE_INTEGER position = {};
    position.QuadPart = size.QuadPart - kTrailerSize;
    SetFilePointerEx(file, position, nullptr, FILE_BEGIN);

    unsigned char trailer[kTrailerSize] = {};
    DWORD read = 0;
    const bool ok = ReadFile(file, trailer, kTrailerSize, &read, nullptr)
            && read == kTrailerSize && memcmp(trailer, "VCSP1000", 8) == 0;
    CloseHandle(file);
    if (!ok) return false;

    auto be64 = [](const unsigned char *p) -> unsigned long long {
        return (static_cast<unsigned long long>(p[0]) << 56)
                | (static_cast<unsigned long long>(p[1]) << 48)
                | (static_cast<unsigned long long>(p[2]) << 40)
                | (static_cast<unsigned long long>(p[3]) << 32)
                | (static_cast<unsigned long long>(p[4]) << 24)
                | (static_cast<unsigned long long>(p[5]) << 16)
                | (static_cast<unsigned long long>(p[6]) << 8)
                | static_cast<unsigned long long>(p[7]);
    };
    *bootOffset = be64(trailer + 8);
    *bootSize = be64(trailer + 16);
    return true;
}

bool extractRange(const std::wstring &exe, unsigned long long offset, unsigned long long length,
                  const std::wstring &destination)
{
    HANDLE in = CreateFileW(exe.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, 0, nullptr);
    if (in == INVALID_HANDLE_VALUE) return false;
    HANDLE out = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        CloseHandle(in);
        return false;
    }

    LARGE_INTEGER position = {};
    position.QuadPart = static_cast<LONGLONG>(offset);
    SetFilePointerEx(in, position, nullptr, FILE_BEGIN);

    std::vector<char> buffer(1 << 20);
    unsigned long long remaining = length;
    bool ok = true;
    while (remaining > 0) {
        const DWORD chunk = static_cast<DWORD>(
            (std::min)(remaining, static_cast<unsigned long long>(buffer.size())));
        DWORD read = 0;
        if (!ReadFile(in, buffer.data(), chunk, &read, nullptr) || read == 0) {
            ok = false;
            break;
        }
        DWORD written = 0;
        if (!WriteFile(out, buffer.data(), read, &written, nullptr) || written != read) {
            ok = false;
            break;
        }
        remaining -= read;
    }
    CloseHandle(in);
    CloseHandle(out);
    return ok;
}

bool runAndWait(const std::wstring &commandLine, const std::wstring &workingDir, DWORD *exitCode)
{
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::wstring mutableCommand = commandLine;
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, workingDir.c_str(), &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (exitCode) GetExitCodeProcess(pi.hProcess, exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
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

std::wstring quote(const std::wstring &text)
{
    return L"\"" + text + L"\"";
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    const std::wstring self = modulePath();
    unsigned long long bootOffset = 0;
    unsigned long long bootSize = 0;
    if (!readTrailer(self, &bootOffset, &bootSize)) {
        MessageBoxW(nullptr, L"This installer is damaged (missing payload).",
                    L"VideoCut Studio Setup", MB_ICONERROR | MB_OK);
        return 1;
    }

    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);
    const std::wstring tempDir = std::wstring(tempPath) + L"VideoCutSetup_"
            + std::to_wstring(GetTickCount64());
    CreateDirectoryW(tempDir.c_str(), nullptr);

    const std::wstring zip = tempDir + L"\\boot.zip";
    if (!extractRange(self, bootOffset, bootSize, zip)) {
        MessageBoxW(nullptr, L"Could not unpack the installer.", L"VideoCut Studio Setup",
                    MB_ICONERROR | MB_OK);
        deleteTree(tempDir);
        return 1;
    }

    const std::wstring tarCommand = L"tar.exe -xf " + quote(zip) + L" -C " + quote(tempDir);
    DWORD tarExit = 1;
    if (!runAndWait(L"C:\\Windows\\System32\\cmd.exe /c " + tarCommand, tempDir, &tarExit)
        || tarExit != 0) {
        MessageBoxW(nullptr, L"Could not unpack the installer (tar failed).",
                    L"VideoCut Studio Setup", MB_ICONERROR | MB_OK);
        deleteTree(tempDir);
        return 1;
    }

    // Forward the command line; tell the UI where the real payload lives.
    const std::wstring args = GetCommandLineW();
    const size_t firstSpace = args.find(L' ');
    const std::wstring extra = firstSpace == std::wstring::npos ? L"" : args.substr(firstSpace);

    const std::wstring ui = tempDir + L"\\VideoCutSetup.exe";
    const std::wstring command = quote(ui) + L" --payload " + quote(self) + extra;
    DWORD uiExit = 1;
    runAndWait(command, tempDir, &uiExit);

    deleteTree(tempDir);
    return static_cast<int>(uiExit);
}
