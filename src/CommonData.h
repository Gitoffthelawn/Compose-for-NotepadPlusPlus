// This file is part of Compose for Notepad++,
// Copyright 2025 by Randy Fellmy <https://www.coises.com/>.

// The source code contained in this file is independent of Notepad++ code.
// It is released under the MIT (Expat) license:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
// associated documentation files (the "Software"), to deal in the Software without restriction, 
// including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial 
// portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT 
// LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include "Framework/ConfigFramework.h"

// Common data structure

inline struct CommonData {

    HHOOK hookCompose   = 0;       // Handle to the hook for processMessages, if successfully installed
    bool  bypassCompose = false;   // Set when Compose key dialog is open, so as not to trap existing compose key

    UINT_PTR     pendingUserDefBuffer = 0;      // Notepad++ BufferID of a user definitions file being edited (0 if none pending)
    bool         pendingQueryOnClose  = false;  // Set if we should ask whether to load pending user definitions file on close

    nlohmann::json rules;

    struct CombiningRule { char32_t one, two, up, down; };  // See ProcessCompose.cpp for explanation.
    std::map<std::wstring, CombiningRule> combiningRules;   // See ProcessCompose.cpp for explanation.

    // Data to be saved in the configuration file

    config<bool>         enabled                = { "ComposeEnabled"        , false    };
    config<WPARAM>       composeKey             = { "ComposeKey"            , VK_INSERT | (HOTKEYF_EXT << 8) };
    config<bool>         userDefinitionsEnabled = { "UserDefinitionsEnabled", false    };
    config<std::wstring> userDefinitionsFile    = { "UserDefinitionsFile"   , L""      };

} data;
