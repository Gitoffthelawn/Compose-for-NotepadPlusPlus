// This file is part of Compose for Notepad++,
// Copyright 2025, 2026 by Randy Fellmy <https://www.coises.com/>.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Framework/PluginFramework.h"
#include "Framework/UtilityFramework.h"
#include "CommonData.h"
#include "resource.h"
#include "Shlwapi.h"

void toggleEnabled();  // Defined in ProcessCommands.cpp

namespace {

    LRESULT CALLBACK HotKeySubclass(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
        switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            switch (wParam) {
            case VK_BACK:
            case VK_RETURN:
            case VK_SPACE:
                if (!(GetKeyState(VK_SHIFT) < 0 || GetKeyState(VK_CONTROL) < 0 || (lParam >> 16) & KF_ALTDOWN)) {
                    if (wParam == VK_RETURN) {
                        SendMessage(GetParent(hWnd), WM_NEXTDLGCTL, 0, FALSE);
                        return 0;
                    }
                    break;
                }
                [[fallthrough]];
            case VK_APPS:
            case VK_DELETE:
            case VK_ESCAPE:
            case VK_TAB:
                SendMessage(hWnd, HKM_SETHOTKEY,
                    wParam | (GetKeyState(VK_SHIFT)   < 0  ? HOTKEYF_SHIFT   << 8 : 0)
                           | (GetKeyState(VK_CONTROL) < 0  ? HOTKEYF_CONTROL << 8 : 0)
                           | (HIWORD(lParam) & KF_ALTDOWN  ? HOTKEYF_ALT     << 8 : 0)
                           | (HIWORD(lParam) & KF_EXTENDED ? HOTKEYF_EXT     << 8 : 0), 0);
                return 0;
            case VK_CAPITAL:
            case VK_SCROLL:
            case VK_NUMLOCK:
            {
                // Avoid toggling a locking key by sending an additional up/down sequence
                static bool correctingKeyLock = false;
                correctingKeyLock = !correctingKeyLock;
                if (!correctingKeyLock) break;
                INPUT input[2];
                input[0].type           = input[1].type           = INPUT_KEYBOARD;
                input[0].ki.wVk         = input[1].ki.wVk         = static_cast<WORD>(wParam);
                input[0].ki.wScan       = input[1].ki.wScan       = (lParam & 0xFF0000) >> 16;
                input[0].ki.time        = input[1].ki.time        = 0;
                input[0].ki.dwExtraInfo = input[1].ki.dwExtraInfo = 0;
                input[1].ki.dwFlags = HIWORD(lParam) & KF_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0;
                input[0].ki.dwFlags = KEYEVENTF_KEYUP | input[1].ki.dwFlags;
                SendInput(2, input, sizeof INPUT);
            }
            }
            break;
        }
        return DefSubclassProc(hWnd, msg, wParam, lParam);
    }
    
    INT_PTR CALLBACK composeKeyDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM /* lParam */) {
        switch (uMsg) {
        case WM_DESTROY:
            RemoveWindowSubclass(GetDlgItem(hwndDlg, IDC_SETKEY_COMPOSEKEY), HotKeySubclass, 1);
            return TRUE;
        case WM_INITDIALOG:
        {
            config_rect::show(hwndDlg);  // centers dialog on owner client area
            HWND hk = GetDlgItem(hwndDlg, IDC_SETKEY_COMPOSEKEY);
            SetWindowSubclass(hk, HotKeySubclass, 1, 0);
            SendMessage(hk, HKM_SETHOTKEY, data.composeKey, 0);
            npp(NPPM_DARKMODESUBCLASSANDTHEME, NPP::NppDarkMode::dmfInit, hwndDlg);
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case IDCANCEL:
                EndDialog(hwndDlg, 1);
                return TRUE;
            case IDOK:
                data.composeKey = SendDlgItemMessage(hwndDlg, IDC_SETKEY_COMPOSEKEY, HKM_GETHOTKEY, 0, 0);
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            return FALSE;
        }
        return FALSE;
    }

}

void showComposeKeyDialog() {
    data.bypassCompose = true;
    if (!DialogBox(plugin.dllInstance, MAKEINTRESOURCE(IDD_SETKEY), plugin.nppData._nppHandle, composeKeyDialogProc))
        if (data.enabled != (data.composeKey & 0xff ? true : false)) toggleEnabled();
    data.bypassCompose = false;
}
