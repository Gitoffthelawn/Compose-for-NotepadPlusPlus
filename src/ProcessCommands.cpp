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
#include "Framework/FileDialogBase.h"
#include "CommonData.h"
#include <fstream>


extern NPP::FuncItem menuDefinition[];      // Defined in Plugin.cpp
extern int menuItem_ToggleEnabled;          // Defined in Plugin.cpp
extern int menuItem_UserDefinitions;        // Defined in Plugin.cpp

bool             loadSequenceDefinitions();             // Defined in LoadSequenceDefinitions.cpp
LRESULT CALLBACK processMessages(int, WPARAM, LPARAM);  // Defined in ProcessCompose.cpp
void             showComposeKeyDialog();                // Defined in ComposeKeyDialog.cpp


void toggleEnabled() {
    if (!data.enabled && !(data.composeKey & 0xff)) {
        showComposeKeyDialog();
        return;
    }
    if (data.hookCompose) {
        UnhookWindowsHookEx(data.hookCompose);
        data.hookCompose = 0;
    }
    else data.hookCompose = SetWindowsHookEx(WH_GETMESSAGE, processMessages, 0, GetCurrentThreadId());
    data.enabled = data.hookCompose ? true : false;
    npp(NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_ToggleEnabled]._cmdID, data.enabled ? 1 : 0);
}


void newUserDefinitionsFile() {
    size_t n = npp(NPPM_GETPLUGINHOMEPATH, 0, 0);
    std::wstring modelPath(n, 0);
    npp(NPPM_GETPLUGINHOMEPATH, n + 1, modelPath.data());
    modelPath += L"\\Compose\\userdefinitions-model.jsonc";
    std::string model;
    {
        std::ifstream modelFile(modelPath);
        if (modelFile) model = { std::istreambuf_iterator<char>(modelFile), std::istreambuf_iterator<char>{} };
    }
    npp(NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
    UINT_PTR bid = npp(NPPM_GETCURRENTBUFFERID, 0, 0);
    if (npp(NPPM_SETBUFFERENCODING, bid, 1)) /* If this fails, we might not have an empty buffer */ {
        npp(NPPM_SETBUFFERLANGTYPE, bid, NPP::L_JSON5);
        plugin.getScintillaPointers();
        sci.SetTargetRange(0, 0);
        sci.ReplaceTarget(model.data());
        data.pendingUserDefBuffer = bid;
        data.pendingQueryOnClose = false;
    }
}


void selectUserDefinitionsFile() {

    struct FOD : OpenDialogBase {

        STDMETHODIMP OnFileOk(IFileDialog*) override {
            if (GetSelectedControlItem(2) == 22) return S_OK;
            std::wstring filename = GetResultPath();
            std::ifstream userfile(filename);
            if (userfile) {
                auto userrules = nlohmann::json::parse(userfile, 0, false, true);
                if (userrules.is_discarded()) {
                    HWND hw = 0;
                    if (auto polew = QueryInterface<IOleWindow>()) {
                        polew->GetWindow(&hw);
                        polew->Release();
                    }
                    TaskDialog(hw, 0, L"Compose: Select or edit a user definitions file", L"Invalid JSON file",
                               (L"\"" + filename + L"\" is not a valid JSON file. Please choose a different file.").data(),
                               0, TD_ERROR_ICON, 0);
                    return S_FALSE;
                }
            }
            return S_OK;
        }

    } fod;

    fod.SetFileName(data.userDefinitionsFile);
    fod.SetFileTypes(L"JSON Files (*.jsonc; *.json; *.json5)|*.jsonc;*.json;*.json5|All Files (*.*)|*.*");
    fod.SetFileTypeIndex(1);
    fod.SetOptions(FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);
    fod.SetTitle(L"Compose: Select or edit a user definitions file");
    fod.SetDefaultExtension(L"jsonc");

    fod.AddPushButton(1, L"None");
    fod.MakeProminent(1);
    fod.EnableOpenDropDown(2);
    fod.AddControlItem(2, 21, L"Select");
    fod.AddControlItem(2, 22, L"Edit");

    if (fod.Show(plugin.nppData._nppHandle)) {
        std::wstring filename = fod.GetResultPath();
        if (fod.GetSelectedControlItem(2) == 21) {
            data.userDefinitionsFile = filename;
            data.userDefinitionsEnabled = true;
            loadSequenceDefinitions();
        }
        else {
            if (npp(NPPM_DOOPEN, 0, filename.data())) {
                data.pendingUserDefBuffer = npp(NPPM_GETCURRENTBUFFERID, 0, 0);
                data.pendingQueryOnClose  = true;
            }
        }
    }

    else if (fod.lastResult() == 1 && data.userDefinitionsEnabled) {
        data.userDefinitionsEnabled = false;
        loadSequenceDefinitions();
    }

}