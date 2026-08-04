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

bool loadSequenceDefinitions();


namespace {

    std::wstring forwardCloseFileName;  // Full path and name of a user definitions file about to close

    void askUserDefinitionsFile(std::wstring fileName) {

        data.pendingQueryOnClose = false;

        int response = 0;
        TaskDialog(plugin.nppData._nppHandle, 0, L"Compose", L"Load this file as your user definitions file?",
            (L"Do you want to load \"" + fileName + L"\" as your user definitions file now?").data(),
            TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, 0, &response);
        if (response != IDYES) return;

        std::ifstream userfile(fileName);
        if (userfile) {
            auto userrules = nlohmann::json::parse(userfile, 0, false, true);
            if (userrules.is_discarded()) {
                TaskDialog(plugin.nppData._nppHandle, 0, L"Compose", L"Invalid JSON file",
                    (L"\"" + fileName + L"\" is not a valid JSON file.").data(),
                    0, TD_WARNING_ICON, 0);
                return;
            }
        }

        data.userDefinitionsFile = fileName;
        data.userDefinitionsEnabled = true;
        loadSequenceDefinitions();

    }

}


void fileBeforeClose(const NMHDR* nm) {
	if (nm->idFrom != data.pendingUserDefBuffer || !data.pendingQueryOnClose) return;
    size_t fileNameLength = npp(NPPM_GETFULLPATHFROMBUFFERID, nm->idFrom, 0);
    if (fileNameLength <= 0) return;
    forwardCloseFileName.resize(fileNameLength);
    npp(NPPM_GETFULLPATHFROMBUFFERID, nm->idFrom, forwardCloseFileName.data());
}


void fileClosed(const NMHDR* nm) {
	if (nm->idFrom != data.pendingUserDefBuffer) return;
	if (npp(NPPM_GETPOSFROMBUFFERID, nm->idFrom, 0) != -1) /* still open in other view */ return;
    if (data.pendingQueryOnClose) askUserDefinitionsFile(forwardCloseFileName);
	data.pendingUserDefBuffer = 0;
}


void fileSaved(const NMHDR* nm) {

    if (nm->idFrom != data.pendingUserDefBuffer) return;

    std::wstring fileName;
    size_t fileNameLength = npp(NPPM_GETFULLPATHFROMBUFFERID, nm->idFrom, 0);
    if (fileNameLength <= 0) return;
    fileName.resize(fileNameLength);
    npp(NPPM_GETFULLPATHFROMBUFFERID, nm->idFrom, fileName.data());

    askUserDefinitionsFile(fileName);

}
