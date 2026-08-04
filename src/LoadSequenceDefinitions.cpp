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

#include <fstream>
#include "Framework/PluginFramework.h"
#include "Framework/UtilityFramework.h"
#include "Framework/UnicodeFormatTranslation.h"
#include "CommonData.h"

extern NPP::FuncItem menuDefinition[];      // Defined in Plugin.cpp
extern int menuItem_UserDefinitions;        // Defined in Plugin.cpp

namespace {

    bool getRule(const nlohmann::json& j, char32_t& c) {
        if (j.is_string()) {
            auto s = utf8to32(j);
            if (s.length() != 1) 
                return false;
            c = s[0];
            return true;
        }
        if (!j.is_boolean()) 
            return false;
        c = j ? 1 : 0;
        return true;
    }

}

bool loadSequenceDefinitions() {
    auto n = SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINHOMEPATH, 0, 0);
    std::wstring path(n, 0);
    SendMessage(plugin.nppData._nppHandle, NPPM_GETPLUGINHOMEPATH, n + 1, reinterpret_cast<LPARAM>(path.data()));
    path += L"\\Compose\\compose-default.jsonc";
    std::ifstream file(path.data());
    if (!file) return false;
    data.rules = nlohmann::json::parse(file, 0, false, true);
    if (data.userDefinitionsEnabled) {
        std::ifstream userfile(data.userDefinitionsFile);
        if (userfile) {
            auto userrules = nlohmann::json::parse(userfile, 0, false, true);
            if (userrules.is_discarded()) data.userDefinitionsEnabled = false;
            else data.rules.update(userrules);
        }
        else data.userDefinitionsEnabled = false;
    }

    data.combiningRules.clear();

    if (data.rules.contains("implicit combining rules") && data.rules["implicit combining rules"].is_object()) {
        const nlohmann::json j = data.rules["implicit combining rules"];
        bool valid = true;
        for (const auto& [key, array] : j.items()) {
            if (!array.is_array() || array.size() != 4) { valid = false; break; }
            CommonData::CombiningRule& rule = data.combiningRules[utf8to16(key)];
            if (!array[0].is_string() || array[0].empty()) { valid = false; break; }
            if ( !getRule(array[0], rule.one) || !getRule(array[1], rule.two )
              || !getRule(array[2], rule.up ) || !getRule(array[3], rule.down) ) { valid = false; break; }
        }
        if (!valid) data.combiningRules.clear();
    }

    npp(NPPM_SETMENUITEMCHECK, menuDefinition[menuItem_UserDefinitions]._cmdID, data.userDefinitionsEnabled.get());
    return true;
}
