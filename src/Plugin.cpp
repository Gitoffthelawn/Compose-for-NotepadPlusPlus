// This file is part of Compose for Notepad++,
// Copyright 2025 by Randy Fellmy <https://www.coises.com/>.

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
using namespace NPP;


// Routines that load and save the configuration file

void loadConfiguration();  // Defined in Configuration.cpp
void saveConfiguration();  // Defined in Configuration.cpp

// Routine to load the sequence definitions

bool loadSequenceDefinitions();  // defined in LoadSequenceDefinitions.cpp

// Routines that process menu commands

void toggleEnabled();               // defined in ProcessCommands.cpp
void showComposeKeyDialog();        // defined in ComposeKeyDialog.cpp
void selectUserDefinitionsFile();   // defined in ProcessCommands.cpp
void newUserDefinitionsFile();      // defined in ProcessCommands.cpp
void showAboutDialog();             // defined in About.cpp

// Routines that process Notepad++ notifications

void fileBeforeClose(const NMHDR*);
void fileClosed(const NMHDR*);
void fileSaved(const NMHDR*);


// Name and define any shortcut keys to be assigned as menu item defaults: Ctrl, Alt, Shift and the virtual key code
//
// Define menu commands:
//     text to appear on menu (ignored for a menu separator line)
//     address of a void, zero-argument function that processes the command (0 for a menu separator line)
//         recommended: use plugin.cmd, per examples, to wrap the function, setting Scintilla pointers and bypassing notifications
//     ignored on call; on return, Notepad++ will fill this in with the menu command ID it assigns
//     whether to show a checkmark beside this item on initial display of the menu
//     0 or default shortcut key (menu accelerator) specified as the address of an NPP::ShortcutKey structure
//
// If you will reference any menu items elsewhere, define mnemonic references for them following the menu definition,
// where it's easy to see and update them if you change the menu; then you can use:
//    extern NPP::FuncItem menuDefinition[];
//    extern int mnemonic;
// in other source files; use the mnemonic to get the ordinal position in the original menu, and:
//    menuDefinition[mnemonic]._cmdID
// to get the menu item identifier assigned by Notepad++.

FuncItem menuDefinition[] = {
    { L"Enabled"                  , []() {plugin.cmd(toggleEnabled            );}, 0, false, 0},
    { L"Compose key..."           , []() {plugin.cmd(showComposeKeyDialog     );}, 0, false, 0},
    { L"---"                      , 0                                            , 0, false, 0},
    { L"User definitions file..." , []() {plugin.cmd(selectUserDefinitionsFile);}, 0, false, 0},
    { L"New user definitions file", []() {plugin.cmd(newUserDefinitionsFile);   }, 0, false, 0},
    { L"Help/About..."            , []() {plugin.cmd(showAboutDialog          );}, 0, false, 0}
};

int menuItem_ToggleEnabled = 0;
int menuItem_UserDefinitions = 3;


// Tell Notepad++ the plugin name

extern "C" __declspec(dllexport) const wchar_t* getName() {
    return L"Compose";
}


// Tell Notepad++ about the plugin menu

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(int *n) {
    loadConfiguration();
    *n = sizeof(menuDefinition) / sizeof(FuncItem);
    return reinterpret_cast<FuncItem*>(&menuDefinition);
}


// Notification processing: each notification desired must be sent to a function that will handle it.

extern "C" __declspec(dllexport) void beNotified(SCNotification *np) {

    if (plugin.bypassNotifications) return;
    plugin.bypassNotifications = true;
    auto*& nmhdr = reinterpret_cast<NMHDR*&>(np);

    if (nmhdr->hwndFrom == plugin.nppData._nppHandle) {
      
        switch (nmhdr->code) {

        case NPPN_BEFORESHUTDOWN:
            plugin.startupOrShutdown = true;
            break;

        case NPPN_CANCELSHUTDOWN:
            plugin.startupOrShutdown = false;
            break;

        case NPPN_FILEBEFORECLOSE:
            fileBeforeClose(nmhdr);
            break;

        case NPPN_FILECLOSED:
            fileClosed(nmhdr);
            break;

        case NPPN_FILESAVED:
            fileSaved(nmhdr);
            break;

        case NPPN_READY:
            plugin.startupOrShutdown = false;
            if (loadSequenceDefinitions() && data.enabled) toggleEnabled();
            npp(NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_UserDefinitions]._cmdID, data.userDefinitionsEnabled ? 1 : 0);
            break;

        case NPPN_SHUTDOWN:
            if (data.hookCompose) UnhookWindowsHookEx(data.hookCompose);
            saveConfiguration();
            break;

        }

    }

    plugin.bypassNotifications = false;

}


// This is rarely used, but a few Notepad++ commands call this routine as part of their processing

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) {return TRUE;}
