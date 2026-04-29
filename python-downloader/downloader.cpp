/*
 * YouTube Downloader - C++ Win32 Version
 * Crafted by Yusuf Gamal
 *
 * Build command (MinGW x64, from the folder containing main.cpp):
 *   g++ -mwindows -O2 -o YouTubeDownloader.exe main.cpp -lcomctl32 -lole32 -luuid
 *
 * Requires yt-dlp.exe in the same folder as YouTubeDownloader.exe.
 * Download from: https://github.com/yt-dlp/yt-dlp/releases
 * Get "yt-dlp.exe" (the standard build works on Win7+).
 */

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")

// ---------------------------------------------------------------------------
// Control IDs
// ---------------------------------------------------------------------------
#define ID_EDIT_URL      101
#define ID_BTN_DOWNLOAD  102
#define ID_PROGRESSBAR   103
#define ID_LABEL_STATUS  104
#define ID_LABEL_TITLE   105
#define ID_LABEL_URL     106

// Custom messages posted from worker thread to main thread
#define WM_PROGRESS  (WM_USER + 1)   // wParam = percent 0..100
#define WM_STATUS    (WM_USER + 2)   // lParam = new wstring* (caller frees)
#define WM_DONE      (WM_USER + 3)   // wParam=1 success / 0 fail, lParam=wstring*

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
HWND g_hWnd      = NULL;
HWND g_hEditUrl  = NULL;
HWND g_hBtnDl    = NULL;
HWND g_hProgress = NULL;
HWND g_hStatus   = NULL;
BOOL g_downloading = FALSE;

// Catppuccin Mocha palette
static const COLORREF CLR_BG       = RGB(0x1e, 0x1e, 0x2e);
static const COLORREF CLR_FG       = RGB(0xcd, 0xd6, 0xf4);
static const COLORREF CLR_ACCENT   = RGB(0x89, 0xb4, 0xfa);
static const COLORREF CLR_ENTRY_BG = RGB(0x31, 0x32, 0x44);
static const COLORREF CLR_MUTED    = RGB(0xa6, 0xad, 0xc8);

static HBRUSH g_hBrushBg    = NULL;
static HBRUSH g_hBrushEntry = NULL;
static HFONT  g_hFontTitle  = NULL;
static HFONT  g_hFontNormal = NULL;
static HFONT  g_hFontSmall  = NULL;
static HFONT  g_hFontBold   = NULL;

// ---------------------------------------------------------------------------
// Thread parameter struct (heap-allocated, freed by thread)
// ---------------------------------------------------------------------------
struct DownloadParams {
    std::wstring url;
    std::wstring destDir;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::wstring GetExeDir()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? p.substr(0, pos) : L".";
}

static std::wstring BrowseForFolder(HWND hParent)
{
    wchar_t display[MAX_PATH] = {};
    BROWSEINFOW bi  = {};
    bi.hwndOwner    = hParent;
    bi.pszDisplayName = display;
    bi.lpszTitle    = L"Choose destination folder";
    bi.ulFlags      = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";

    wchar_t path[MAX_PATH] = {};
    SHGetPathFromIDListW(pidl, path);

    IMalloc* pm = NULL;
    if (SUCCEEDED(SHGetMalloc(&pm))) { pm->Free(pidl); pm->Release(); }

    return std::wstring(path);
}

static void CreateResources()
{
    g_hBrushBg    = CreateSolidBrush(CLR_BG);
    g_hBrushEntry = CreateSolidBrush(CLR_ENTRY_BG);

    g_hFontTitle  = CreateFontW(-20,0,0,0,FW_BOLD,  FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
    g_hFontNormal = CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
    g_hFontSmall  = CreateFontW(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
    g_hFontBold   = CreateFontW(-14,0,0,0,FW_BOLD,  FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
}

static void DestroyResources()
{
    DeleteObject(g_hBrushBg);    DeleteObject(g_hBrushEntry);
    DeleteObject(g_hFontTitle);  DeleteObject(g_hFontNormal);
    DeleteObject(g_hFontSmall);  DeleteObject(g_hFontBold);
}

static void CenterWindow(HWND hWnd)
{
    RECT rc; GetWindowRect(hWnd, &rc);
    int w=rc.right-rc.left, h=rc.bottom-rc.top;
    SetWindowPos(hWnd,NULL,
        (GetSystemMetrics(SM_CXSCREEN)-w)/2,
        (GetSystemMetrics(SM_CYSCREEN)-h)/2,
        0,0, SWP_NOSIZE|SWP_NOZORDER);
}

// ---------------------------------------------------------------------------
// Worker thread  (plain Win32 CreateThread - no std::thread, no pthread)
// ---------------------------------------------------------------------------
static DWORD WINAPI DownloadThread(LPVOID lpParam)
{
    DownloadParams* p = (DownloadParams*)lpParam;
    std::wstring url     = p->url;
    std::wstring destDir = p->destDir;
    delete p;

    std::wstring ytdlp = GetExeDir() + L"\\yt-dlp.exe";

    std::wstring cmd =
        L"\"" + ytdlp + L"\""
        L" --newline"
        L" --no-part"
        L" --format best"
        L" --no-playlist"
        L" -o \"" + destDir + L"\\%(title)s.%(ext)s\""
        L" \"" + url + L"\"";

    // Create pipe for stdout+stderr
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hR = NULL, hW = NULL;
    if (!CreatePipe(&hR, &hW, &sa, 0)) {
        PostMessageW(g_hWnd, WM_DONE, 0,
            (LPARAM)new std::wstring(L"Failed to create pipe."));
        return 0;
    }
    SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow= SW_HIDE;
    si.hStdOutput = hW;
    si.hStdError  = hW;

    PROCESS_INFORMATION pi = {};

    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    if (!CreateProcessW(NULL, buf.data(), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        CloseHandle(hR); CloseHandle(hW);
        PostMessageW(g_hWnd, WM_DONE, 0,
            (LPARAM)new std::wstring(
                L"Could not launch yt-dlp.exe\n"
                L"Make sure yt-dlp.exe is in the same folder as this program."));
        return 0;
    }
    CloseHandle(hW); // close write-end in parent

    // Read yt-dlp output line by line
    std::string accum;
    char rbuf[512];
    DWORD nRead = 0;

    while (ReadFile(hR, rbuf, sizeof(rbuf)-1, &nRead, NULL) && nRead > 0)
    {
        rbuf[nRead] = '\0';
        accum += rbuf;

        size_t pos;
        while ((pos = accum.find('\n')) != std::string::npos)
        {
            std::string line = accum.substr(0, pos);
            accum.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.find("[download]") != std::string::npos)
            {
                // Parse percentage
                size_t pct = line.find('%');
                if (pct != std::string::npos) {
                    size_t ns = pct;
                    while (ns > 0 && (isdigit((unsigned char)line[ns-1]) || line[ns-1]=='.'))
                        --ns;
                    try {
                        int ipct = (int)std::stod(line.substr(ns, pct-ns));
                        PostMessageW(g_hWnd, WM_PROGRESS, (WPARAM)ipct, 0);
                    } catch(...) {}
                }

                // Send line as status
                int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, NULL, 0);
                if (wlen > 0) {
                    std::wstring* ws = new std::wstring(wlen, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, &(*ws)[0], wlen);
                    ws->resize(wcslen(ws->c_str()));
                    PostMessageW(g_hWnd, WM_STATUS, 0, (LPARAM)ws);
                }
            }
        }
    }
    CloseHandle(hR);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode == 0) {
        PostMessageW(g_hWnd, WM_PROGRESS, 100, 0);
        PostMessageW(g_hWnd, WM_DONE, 1,
            (LPARAM)new std::wstring(L"Download complete!"));
    } else {
        PostMessageW(g_hWnd, WM_DONE, 0,
            (LPARAM)new std::wstring(L"Download failed. Check the URL and try again."));
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Window Procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateResources();
        const int W = 460, x = 18;
        int y = 16;

        // Title
        HWND h = CreateWindowW(L"STATIC", L"YouTube Downloader",
            WS_CHILD|WS_VISIBLE|SS_CENTER,
            0, y, W, 28, hWnd, (HMENU)ID_LABEL_TITLE, NULL, NULL);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

        // "Video URL" label
        y += 36;
        h = CreateWindowW(L"STATIC", L"Video URL",
            WS_CHILD|WS_VISIBLE,
            x, y, W-x*2, 18, hWnd, (HMENU)ID_LABEL_URL, NULL, NULL);
        SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        // URL edit
        y += 20;
        g_hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
            x, y, W-x*2, 26, hWnd, (HMENU)ID_EDIT_URL, NULL, NULL);
        SendMessageW(g_hEditUrl, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

        // Download button
        y += 36;
        g_hBtnDl = CreateWindowW(L"BUTTON", L"Download",
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            x, y, 120, 30, hWnd, (HMENU)ID_BTN_DOWNLOAD, NULL, NULL);
        SendMessageW(g_hBtnDl, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

        // Progress bar
        y += 42;
        g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
            WS_CHILD|WS_VISIBLE|PBS_SMOOTH,
            x, y, W-x*2, 16, hWnd, (HMENU)ID_PROGRESSBAR, NULL, NULL);
        SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0,100));
        SendMessageW(g_hProgress, PBM_SETPOS,   0, 0);
        SendMessageW(g_hProgress, PBM_SETBARCOLOR, 0, (LPARAM)CLR_ACCENT);
        SendMessageW(g_hProgress, PBM_SETBKCOLOR,  0, (LPARAM)CLR_ENTRY_BG);

        // Status label
        y += 24;
        g_hStatus = CreateWindowW(L"STATIC",
            L"Paste a YouTube URL and press Download.",
            WS_CHILD|WS_VISIBLE|SS_LEFT,
            x, y, W-x*2, 18, hWnd, (HMENU)ID_LABEL_STATUS, NULL, NULL);
        SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

        CenterWindow(hWnd);
        return 0;
    }

    case WM_ERASEBKGND:
    {
        RECT rc; GetClientRect(hWnd, &rc);
        FillRect((HDC)wParam, &rc, g_hBrushBg);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetBkColor(hdc, CLR_BG);
        int id = GetDlgCtrlID((HWND)lParam);
        SetTextColor(hdc,
            id == ID_LABEL_TITLE  ? CLR_ACCENT :
            id == ID_LABEL_STATUS ? CLR_MUTED  : CLR_FG);
        return (LRESULT)g_hBrushBg;
    }

    case WM_CTLCOLOREDIT:
    {
        SetBkColor((HDC)wParam, CLR_ENTRY_BG);
        SetTextColor((HDC)wParam, CLR_FG);
        return (LRESULT)g_hBrushEntry;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BTN_DOWNLOAD && !g_downloading)
        {
            int len = GetWindowTextLengthW(g_hEditUrl);
            if (len == 0) {
                MessageBoxW(hWnd, L"Please enter a YouTube video URL.",
                            L"No URL", MB_ICONWARNING);
                break;
            }
            std::wstring url(len+1, L'\0');
            GetWindowTextW(g_hEditUrl, &url[0], len+1);
            url.resize(len);

            // Trim
            size_t s = url.find_first_not_of(L" \t\r\n");
            size_t e = url.find_last_not_of(L" \t\r\n");
            if (s == std::wstring::npos) {
                MessageBoxW(hWnd, L"Please enter a YouTube video URL.",
                            L"No URL", MB_ICONWARNING);
                break;
            }
            url = url.substr(s, e-s+1);

            std::wstring dest = BrowseForFolder(hWnd);
            if (dest.empty()) break;

            EnableWindow(g_hBtnDl, FALSE);
            SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);
            SetWindowTextW(g_hStatus, L"Starting download...");

            g_downloading = TRUE;

            // Use Win32 CreateThread — no std::thread, no pthread dependency
            DownloadParams* dp = new DownloadParams{url, dest};
            HANDLE hThread = CreateThread(NULL, 0, DownloadThread, dp, 0, NULL);
            if (hThread) CloseHandle(hThread);
            else {
                delete dp;
                g_downloading = FALSE;
                EnableWindow(g_hBtnDl, TRUE);
                MessageBoxW(hWnd, L"Failed to start download thread.",
                            L"Error", MB_ICONERROR);
            }
        }
        break;
    }

    case WM_PROGRESS:
        SendMessageW(g_hProgress, PBM_SETPOS, wParam, 0);
        break;

    case WM_STATUS:
    {
        std::wstring* pm = (std::wstring*)lParam;
        if (pm) { SetWindowTextW(g_hStatus, pm->c_str()); delete pm; }
        break;
    }

    case WM_DONE:
    {
        std::wstring* pm = (std::wstring*)lParam;
        bool ok = (wParam == 1);

        SendMessageW(g_hProgress, PBM_SETPOS, ok ? 100 : 0, 0);
        SetWindowTextW(g_hStatus, pm ? pm->c_str() : (ok ? L"Done." : L"Failed."));

        if (ok) {
            MessageBoxW(hWnd, L"Video downloaded successfully!", L"Done",
                        MB_ICONINFORMATION);
            SetWindowTextW(g_hEditUrl, L"");
            SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);
            SetWindowTextW(g_hStatus,
                L"Paste a YouTube URL and press Download.");
        } else {
            MessageBoxW(hWnd, pm ? pm->c_str() : L"Unknown error.",
                        L"Download failed", MB_ICONERROR);
        }

        if (pm) delete pm;
        g_downloading = FALSE;
        EnableWindow(g_hBtnDl, TRUE);
        break;
    }

    case WM_DESTROY:
        DestroyResources();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitialize(NULL);

    WNDCLASSEXW wc   = {sizeof(wc)};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = L"YTDownloaderClass";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    RECT rc = {0,0,460,230};
    AdjustWindowRect(&rc, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, FALSE);

    g_hWnd = CreateWindowExW(0,
        L"YTDownloaderClass",
        L"YouTube Downloader  \x2013  Crafted by Yusuf Gamal",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right-rc.left, rc.bottom-rc.top,
        NULL, NULL, hInst, NULL);

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}