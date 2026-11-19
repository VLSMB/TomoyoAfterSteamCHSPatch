#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <optional>
#include <cctype>
#include <cwctype>
#include "define.h"
#include "generated/resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

constexpr auto IDC_EDIT_DIR = 1001;
constexpr auto IDC_BTN_BROWSE = 1002;
constexpr auto IDC_BTN_OK = 1003;
constexpr auto IDC_BTN_CANCEL = 1004;
constexpr auto GAME_APPID_STR = L"462990";
constexpr auto PATCH_WINDOW_CLASS = L"VLSMB_PATCH";

static HWND handle_edit = nullptr;
static HFONT handle_font = nullptr;

static std::wstring ReadRegStr(HKEY root, const wchar_t* sub, const wchar_t* val) {
    HKEY h{};
    if (RegOpenKeyExW(root, sub, 0, KEY_READ, &h) != ERROR_SUCCESS) return {};
    wchar_t buf[MAX_PATH * 4]{};
    DWORD sz = sizeof(buf), type = 0;
    LSTATUS st = RegQueryValueExW(h, val, nullptr, &type, (LPBYTE)buf, &sz);
    RegCloseKey(h);
    if (st != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    return buf;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

static std::vector<std::pair<std::string, std::string>> ParseVdf(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> out;
    size_t i = 0, n = text.size();
    auto skip = [&] {
        while (i < n) {
            if (std::isspace((unsigned char)text[i])) ++i;
            else if (text[i] == '/' && i + 1 < n && text[i + 1] == '/')
                while (i < n && text[i] != '\n') ++i;
            else break;
        }
        };
    auto str = [&](std::string& s) -> bool {
        if (i >= n || text[i] != '"') return false;
        ++i; s.clear();
        while (i < n && text[i] != '"') {
            if (text[i] == '\\' && i + 1 < n) {
                char c = text[i + 1];
                s += (c == 'n') ? '\n' : (c == 't') ? '\t' : c;
                i += 2;
            } else s += text[i++];
        }
        if (i >= n) return false;
        ++i; return true;
        };
    while (true) {
        skip();
        if (i >= n) break;
        std::string k, v;
        if (!str(k)) { ++i; continue; }
        skip();
        if (str(v)) out.emplace_back(std::move(k), std::move(v));
    }
    return out;
}

static std::wstring AutoDetectGameDir(const std::wstring& appId) {
    std::wstring steamPath = ReadRegStr(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (steamPath.empty())
        steamPath = ReadRegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (steamPath.empty()) return {};

    std::error_code ec;
    if (!fs::exists(steamPath, ec) || ec) return {};

    std::vector<fs::path> appsRoots;
    appsRoots.push_back(fs::path(steamPath) / L"steamapps");

    std::ifstream lf(fs::path(steamPath) / L"steamapps" / L"libraryfolders.vdf", std::ios::binary);
    if (lf) {
        std::stringstream ss; ss << lf.rdbuf();
        for (auto& [k, v] : ParseVdf(ss.str()))
            if (k == "path")
                appsRoots.push_back(fs::path(Utf8ToWide(v)) / L"steamapps");
    }

    for (auto& root : appsRoots) {
        fs::path acf = root / (L"appmanifest_" + appId + L".acf");
        if (!fs::exists(acf, ec) || ec) continue;

        std::ifstream af(acf, std::ios::binary);
        if (!af) continue;
        std::stringstream ss; ss << af.rdbuf();
        std::wstring installdir;
        for (auto& [k, v] : ParseVdf(ss.str()))
            if (k == "installdir") { installdir = Utf8ToWide(v); break; }
        if (installdir.empty()) continue;

        fs::path gameDir = root / L"common" / installdir;
        if (fs::exists(gameDir, ec) && !ec && fs::is_directory(gameDir, ec) && !ec)
            return gameDir.wstring();
    }
    return {};
}

static std::wstring BrowseFolder(HWND owner) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::wstring result;
    IFileDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD opts = 0;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        pfd->SetTitle(L"请选择《Tomoyo After English Edition》所在的目录");
        if (SUCCEEDED(pfd->Show(owner))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi)) && psi) {
                PWSTR p = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                    result = p;
                    CoTaskMemFree(p);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    return result;
}

static bool ExtractResource(HMODULE hMod, unsigned resId,
    const fs::path& targetPath, std::wstring& err) {
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) {
        err = L"资源不存在，ID=" + std::to_wstring(resId);
        return false;
    }

    DWORD size = SizeofResource(hMod, hRes);
    HGLOBAL hData = LoadResource(hMod, hRes);
    if (!hData || size == 0) {
        err = L"加载资源失败，ID=" + std::to_wstring(resId);
        return false;
    }

    const void* pData = LockResource(hData);
    if (!pData) {
        err = L"锁定资源失败，ID=" + std::to_wstring(resId);
        return false;
    }

    std::error_code ec;
    const fs::path parent = targetPath.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            err = L"创建目录失败: " + parent.wstring()
                + L" (" + std::to_wstring(ec.value()) + L")";
            return false;
        }
    }

    HANDLE hFile = CreateFileW(targetPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        err = L"打开文件失败: " + targetPath.wstring()
            + L" (err=" + std::to_wstring(GetLastError()) + L")";
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, pData, size, &written, nullptr);
    DWORD we = ok ? 0 : GetLastError();
    CloseHandle(hFile);

    if (!ok || written != size) {
        err = L"写入失败: " + targetPath.wstring()
            + L" (err=" + std::to_wstring(we) + L")";
        std::error_code e2;
        fs::remove(targetPath, e2);
        return false;
    }
    return true;
}

static bool ApplyPatch(const fs::path& gameDir, std::wstring& msg) {
    std::error_code ec;
    if (!fs::exists(gameDir, ec) || ec || !fs::is_directory(gameDir, ec) || ec) {
        msg = L"游戏目录不存在: " + gameDir.wstring();
        return false;
    }

    HMODULE hMod = GetModuleHandleW(nullptr);  // 资源在本 exe 内
    int okCount = 0;

    for (auto& [relPath, resId] : PATCH_FILES) {
        fs::path target = gameDir / fs::path(relPath);

        std::wstring err;
        if (!ExtractResource(hMod, resId, target, err)) {
            msg = err;
            return false;
        }
        ++okCount;
    }

    msg = L"补丁应用成功，共 " + std::to_wstring(okCount) + L" 个文件";
    return true;
}

static void OnConfirm(HWND hWnd) {
    wchar_t buf[MAX_PATH * 4]{};
    GetWindowTextW(handle_edit, buf, MAX_PATH * 4);
    std::wstring dir = buf;

    auto trim = [](std::wstring& s) {
        while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
        while (!s.empty() && iswspace(s.back())) s.pop_back();
        };
    trim(dir);

    if (dir.empty()) {
        MessageBoxW(hWnd, L"请先选择或输入游戏所在的目录", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring msg;
    if (!ApplyPatch(dir, msg)) {
        MessageBoxW(hWnd, msg.c_str(), L"失败", MB_OK | MB_ICONERROR);
        return;
    }

    MessageBoxW(hWnd, msg.c_str(), L"成功", MB_OK | MB_ICONINFORMATION);
    DestroyWindow(hWnd);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", L"安装目录：",
                WS_CHILD | WS_VISIBLE,
                15, 20, 80, 24, hWnd, nullptr, nullptr, nullptr);
            handle_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                100, 18, 420, 26, hWnd, (HMENU)IDC_EDIT_DIR, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"浏览...",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                540, 18, 80, 26, hWnd, (HMENU)IDC_BTN_BROWSE, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"确认",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                380, 70, 110, 32, hWnd, (HMENU)IDC_BTN_OK, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"取消",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                510, 70, 110, 32, hWnd, (HMENU)IDC_BTN_CANCEL, nullptr, nullptr);

            HDC hdc = GetDC(nullptr);
            UINT dpi = 96;
            if (hdc) {
                dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
                ReleaseDC(nullptr, hdc);
            }
            dpi = dpi ? dpi : 96;
            handle_font = CreateFontW(-MulDiv(9, (int)dpi, 72), 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                L"Microsoft YaHei UI");
            EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
                    SendMessageW(hChild, WM_SETFONT, (WPARAM)lParam, MAKELPARAM(TRUE, 0));
                    return TRUE;
                }, (LPARAM)handle_font);

            std::wstring autoDir = AutoDetectGameDir(GAME_APPID_STR);
            if (!autoDir.empty())
                SetWindowTextW(handle_edit, autoDir.c_str());
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_BTN_BROWSE: {
                    std::wstring picked = BrowseFolder(hWnd);
                    if (!picked.empty())
                        SetWindowTextW(handle_edit, picked.c_str());
                    break;
                }
                case IDC_BTN_OK:
                    OnConfirm(hWnd);
                    break;
                case IDC_BTN_CANCEL:
                    DestroyWindow(hWnd);
                    break;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;
        case WM_DESTROY:
            if (handle_font) {
                DeleteObject(handle_font);
                handle_font = nullptr;
            }
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    InitCommonControls();

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = PATCH_WINDOW_CLASS;
    RegisterClassW(&wc);

    RECT rc{ 0, 0, 650, 130 };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    HWND hWnd = CreateWindowW(PATCH_WINDOW_CLASS, TEXT(WINDOW_TITLE__),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}