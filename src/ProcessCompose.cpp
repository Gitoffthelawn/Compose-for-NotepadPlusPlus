// This file is part of Compose for Notepad++,
// Copyright 2025, 2026 by Randy Fellmy <https://www.coises.com/>.

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

#include "Framework/UnicodeFormatTranslation.h"
#include "CommonData.h"

// CombiningRule and combiningRules are defined in CommonData.cpp:
// 
//     struct CombiningRule { char32_t one, two, up, down; };
//     std::map<std::wstring, CombiningRule> combiningRules;
// 
// LoadSequenceDefinitions fills in combiningRules from the sequence definition file(s).
// 
// A CombiningRule defines the interpretation of keys that represent combining marks (accents).
// Each rule contains the char32_t results for a single accent, a double, an up and a down version.
// A zero means the combination (double, up and/or down) is not valid; a 1 for up or down means use the single accent value.
// 
// combiningRules is a std::map from std::wstrings that represent combining marks to the CombiningRule structures that define them.


namespace {

    std::string  composeSequence;            // compose sequence so far (encoding is UTF-8)
    std::wstring implicitSuffix;             // trailing characters that follow a complete implicit match
    int          correctingKeyLock = 0;      // set to pass one keyup/keydown pair because it is being sent to correct the lock state
    bool         composing = false;          // true when a compose sequence is in progress


    // ImplicitCombination implicitCombination keeps track of the progress of an implicit combination.
    // 
    // AddStatus add(std::wstring_view s)
    //     Takes a new key to be added to the implicit combination.
    //     Returns:
    //         Accept:   The key has been added; more keys can be added.
    //         Complete: The key has been added; the combination is finished and no more keys can be added.
    //         Reject:   The key has not been added; the combination is finished and the supplied key is left over.
    //
    //  void clear()
    //      Resets the implicit combination to an empty sequence.
    //
    //  std::wstring compose()
    //      Returns the composed string for the implicit combination.
    //
    //  AddStatus status() const
    //      Returns the status from the last add operation.

    class ImplicitCombination {
    public:
        enum AddStatus {Accept, Complete, Reject};
    private:
        enum ModStatus {ModNone, ModUp, ModDown};
        struct Marks { bool one = false, two = false, up = false, down = false; };
        std::map<std::wstring, Marks> haveMark;
        std::wstring          base;
        std::vector<char32_t> comb;
        int                   value = 0;
        AddStatus             addStatus = Accept;
        ModStatus             modPending = ModNone;
    public:
        AddStatus add(const std::wstring& s);
        void clear() { base.clear(); comb.clear(); haveMark.clear(); value = 0; addStatus = Accept; modPending = ModNone; }
        std::wstring  compose() const;
        AddStatus status() const { return addStatus; }
    } implicitCombination;


    ImplicitCombination::AddStatus ImplicitCombination::add(const std::wstring& s) {

        if (addStatus != Accept) return addStatus = Reject;
        if (s == L"\r") return addStatus = Complete;

        if (base.length() == 1) {
            if (s.length() == 1 && comb.empty() && modPending == ModNone) {
                wchar_t b = base[0];
                wchar_t c = s[0];
                if (b == L'&' && c == L'#') {
                    base = L"&#";
                    return Accept;
                }
                else if (iswxdigit(b) && iswxdigit(c)) {
                    base = L"#x";
                    comb.push_back(b);
                    comb.push_back(c);
                    value = (b <= L'9' ? b - L'0' : b >= L'a' ? b - L'a' + 10 : b - L'A' + 10) * 16
                          + (c <= L'9' ? c - L'0' : c >= L'a' ? c - L'a' + 10 : c - L'A' + 10);
                    return Accept;
                }
            }
        }

        else if (base == L"&#" || base == L"&#x" || base == L"#x") {
            if (s.length() != 1) return addStatus = Reject;
            const wchar_t c = s[0];
            if (c == L';') {
                if (base == L"#x") return addStatus = Reject;
                comb.push_back(c);
                return addStatus = Complete;
            }
            int v;
            if (base == L"&#") {
                if (comb.empty() && (c == L'X' || c == L'x')) {
                    base = L"&#x";
                    return Accept;
                }
                if (!iswdigit(c)) return addStatus = Reject;
                v = value * 10;
            }
            else {
                if (!iswxdigit(c)) return addStatus = Reject;
                v = value * 16;
            }
            v += c <= L'9' ? c - L'0' : c >= L'a' ? c - L'a' + 10 : c - L'A' + 10;
            if (v < 0x110000) {
                value = v;
                comb.push_back(c);
                return addStatus = (base == L"#x" && (value >= 0x11000 || comb.size() > 5) ? Complete : Accept);
            }
            return addStatus = Reject;
        }

        if (s.length() > 2) /* bracketed name of a non-character key */ {
            if (s == L"[Up]"  ) { modPending = ModUp  ; return Accept; }
            if (s == L"[Down]") { modPending = ModDown; return Accept; }
            return addStatus = Reject;
        }

        if (data.combiningRules.contains(s)) {
            const CommonData::CombiningRule& rule = data.combiningRules[s];
            Marks& marks = haveMark[s];
            switch (modPending) {
            case ModUp:
                if (rule.up == 1) break;
                if (marks.up || !rule.up) return addStatus = Reject;
                marks.up = true;
                comb.push_back(rule.up);
                modPending = ModNone;
                return Accept;
            case ModDown:
                if (rule.down == 1) break;
                if (marks.down || !rule.down) return addStatus = Reject;
                marks.down = true;
                comb.push_back(rule.down);
                modPending = ModNone;
                return Accept;
            default:;
            }
            if (marks.two) return addStatus = Reject;
            if (marks.one) {
                if (!rule.two) return addStatus = Reject;
                if (comb.empty() || comb.back() != rule.one) return addStatus = Reject;
                marks.one = false;
                marks.two = true;
                comb.back() = rule.two;
                modPending = ModNone;
                return Accept;
            }
            if (rule.one) {
                marks.one = true;
                comb.push_back(rule.one);
                modPending = ModNone;
                return Accept;
            }
            return addStatus = Reject;
        }

        if (modPending != ModNone || !base.empty()) return addStatus = Reject;
        base = s;
        return addStatus = (comb.empty() ? Accept : Complete);

    }


    std::wstring ImplicitCombination::compose() const {

        if ( ((base == L"&#" || base == L"&#x") && comb.size() > 1 && comb.back() == L';') || (base == L"#x" && !comb.empty()) ) {
            std::wstring r;
            if (value >= 0x10000) {
                r  = static_cast<wchar_t>(0xD800 + ((value - 0x10000) >> 10));
                r += static_cast<wchar_t>(0xDC00 + (value & 0x03FF));
            }
            else r = static_cast<wchar_t>(value);
            return r;
        }

        std::wstring s = base;
        for (char32_t c : comb)
            if (c >= 0x10000) {
                s += static_cast<wchar_t>(0xD800 + ((c - 0x10000) >> 10));
                s += static_cast<wchar_t>(0xDC00 + (c & 0x03FF));
            }
            else s += static_cast<wchar_t>(c);

        std::wstring r;
        if (!s.empty()) {
            const int sl = static_cast<int>(s.length());
            const int rl = NormalizeString(NormalizationC, s.data(), sl, 0, 0);
            if (rl > 0) {
                r.resize(rl);
                const int nl = NormalizeString(NormalizationC, s.data(), sl, r.data(), rl);
                if (nl <= 0) r.clear(); else r.resize(nl);
            }
        }

        switch (modPending) {
        case ModUp  : r += L"[Up]"  ; break;
        case ModDown: r += L"[Down]"; break;
        default: ;
        }
        return r;
    }


    // void sendString(std::wstring_view text)
    //
    // Sends a string as simulated keyboard input.

    void sendString(std::wstring_view text) {
        size_t n = text.length();
        if (!n) return;
        INPUT* input = new INPUT[2 * n];
        for (size_t i = 0; i < n; ++i) {
            input[2 * i].type           = input[2 * i + 1].type           = INPUT_KEYBOARD;
            input[2 * i].ki.wVk         = input[2 * i + 1].ki.wVk         = 0;
            input[2 * i].ki.wScan       = input[2 * i + 1].ki.wScan       = text[i];
            input[2 * i].ki.time        = input[2 * i + 1].ki.time        = 0;
            input[2 * i].ki.dwExtraInfo = input[2 * i + 1].ki.dwExtraInfo = 0;
            input[2 * i].ki.dwFlags     = KEYEVENTF_UNICODE;
            input[2 * i + 1].ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_UNICODE;
        }
        SendInput(static_cast<UINT>(n * 2), input, sizeof INPUT);
        delete[] input;
    }


    // void reverseLockingKey(WPARAM virtualKey = 0)
    //
    // If the supplied virtual key is Caps Lock, Num Lock or Scroll Lock, sends a keyup followed by a keydown
    // and sets correctingKeyLock to 2.  This process effectively reverses the toggle the key caused.
    // 
    // If the supplied virtual key is not a locking key, this routine does nothing.
    // 
    // If no virtual key is supplied, the compose key is assumed.

    void reverseLockingKey(WPARAM virtualKey = 0) {
        WORD vk = static_cast<WORD>(virtualKey ? virtualKey : data.composeKey.get()) & 0xFF;
        if (vk != VK_CAPITAL && vk != VK_SCROLL && vk != VK_NUMLOCK) return;
        INPUT input[2];
        input[0].type           = input[1].type           = INPUT_KEYBOARD;
        input[0].ki.wVk         = input[1].ki.wVk         = vk;
        input[0].ki.wScan       = input[1].ki.wScan       = static_cast<WORD>(MapVirtualKey(vk, MAPVK_VK_TO_VSC));
        input[0].ki.time        = input[1].ki.time        = 0;
        input[0].ki.dwExtraInfo = input[1].ki.dwExtraInfo = 0;
        input[0].ki.dwFlags     = KEYEVENTF_KEYUP;
        input[1].ki.dwFlags     = 0;
        correctingKeyLock = 2;
        SendInput(2, input, sizeof INPUT);
    }


    // void processSequence(WPARAM wParam, LPARAM lParam)
    //
    // Accumulates keystrokes while composing.
    // When an explicit match is found or an implicit match is complete, sends composition and sets composing = false.

    void processSequence(WPARAM wParam, LPARAM lParam) {

        UINT scanCode = (lParam & 0x00FF0000) >> 16;
        unsigned char keyboardState[256];
        GetKeyboardState(keyboardState);
        keyboardState[VK_CAPITAL] = 0;  // Ignore caps lock if toggled

        wchar_t charsTyped[16];
        int len = ToUnicode(static_cast<UINT>(wParam), scanCode, keyboardState, charsTyped, 16, 0);
        if (len < 0) return;

        std::wstring stringTyped;
        if (len == 0) /* map some non-character keys we can use */ {
            switch (wParam) {
            case VK_SHIFT:
            case VK_CONTROL:
            case VK_MENU:
                return;
            case VK_LEFT   : stringTyped = L"[Left]" ; break;
            case VK_UP     : stringTyped = L"[Up]"   ; break;
            case VK_RIGHT  : stringTyped = L"[Right]"; break;
            case VK_DOWN   : stringTyped = L"[Down]" ; break;
            default:
            {
                reverseLockingKey(wParam);
                wchar_t keyname[32];
                if (GetKeyNameText(static_cast<LONG>(lParam), keyname, 32))
                    stringTyped = L"[" + std::wstring(keyname) + L"]";
                else return;
            }
            }
        }
        else stringTyped = std::wstring(charsTyped, len);

        if (implicitCombination.add(stringTyped) == ImplicitCombination::Reject) implicitSuffix += stringTyped;

        composeSequence += utf16to8(stringTyped);
        size_t sequenceLength = composeSequence.length();
        for (auto& rule : data.rules.items()) {
            if (!rule.value().is_string()) continue;
            std::string key = rule.key();
            if (key == composeSequence) {
                composing = false;
                sendString(utf8to16(rule.value()));
                composeSequence.clear();
                return;
            }
            else if (key.length() > sequenceLength && key.substr(0, sequenceLength) == composeSequence) return;
        }

        if (implicitCombination.status() == ImplicitCombination::Accept) return;
        composing = false;
        sendString(implicitCombination.compose() + implicitSuffix);
        implicitSuffix.clear();
        composeSequence.clear();

    }


    // bool processCompose(WPARAM wParam, LPARAM lParam)
    //
    // Handles the compose key and delegates keystrokes while composing to processSequence.
    // Returns true if keystroke should be blocked or false if it should be passed on.

    bool processCompose(WPARAM wParam, LPARAM lParam) {
        if (correctingKeyLock) {
            --correctingKeyLock;
            return false;
        }
        bool releasing  = lParam & 0x80000000;
        bool composeKey = (wParam | (GetKeyState(VK_SHIFT  ) < 0  ? HOTKEYF_SHIFT   << 8 : 0)
                                  | (GetKeyState(VK_CONTROL) < 0  ? HOTKEYF_CONTROL << 8 : 0)
                                  | ((lParam >> 16) & KF_ALTDOWN  ? HOTKEYF_ALT     << 8 : 0)
                                  | ((lParam >> 16) & KF_EXTENDED ? HOTKEYF_EXT     << 8 : 0)) == data.composeKey;
        if (composing) {
            if (composeKey) {
                if (releasing) return true;
                else if (composeSequence.empty()) composing = false;
                else {
                    reverseLockingKey();
                    sendString(implicitCombination.compose() + implicitSuffix);
                    composeSequence.clear();
                    implicitSuffix.clear();
                    implicitCombination.clear();
                    return true;
                }
            }
            else {
                if ((lParam & 0xC0000000) == 0) /* ignore everything except initial keydown messages */ {
                    processSequence(wParam, lParam);
                }
                return true;
            }
        }
        else if (composeKey) {
            if (releasing) {
                if (!composeSequence.empty()) {
                    composeSequence.clear();
                    implicitSuffix.clear();
                    implicitCombination.clear();
                    return true;
                }
            }
            else {
                composing = true;
                composeSequence.clear();
                implicitSuffix.clear();
                implicitCombination.clear();
                reverseLockingKey();
                return true;
            }
        }
        return false;
    }
}


// LRESULT CALLBACK processMessages(int code, WPARAM wParam, LPARAM lParam)
// 
// This is the WH_GETMESSAGE hook installed by toggleEnabled() in ProcessCommands.cpp.
// Every message in the message loop passes through here when composition is enabled.
// Though we mostly need WM_(SYS)KEY* messages, avoiding a spurious context menu when VK_APPS is pressed during composition
// or as the compose key requires trapping WM_CONTEXTMENU, so we use WH_GETMESSAGE instead of WH_KEYBOARD.

LRESULT CALLBACK processMessages(int code, WPARAM wParam, LPARAM lParam) {
    MSG& msg = *reinterpret_cast<MSG*>(lParam);
    static bool suppressNextContextMenu = false;
    if (code == HC_ACTION && wParam == PM_REMOVE) {
        switch (LOWORD(msg.message)) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (composing && msg.wParam == VK_APPS) {
                suppressNextContextMenu = true;
            }
            [[fallthrough]];
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (!data.bypassCompose && msg.wParam != VK_PACKET) {
                if (processCompose(msg.wParam, msg.lParam)) {
                    msg.message = WM_NULL;
                    return 0;
                }
                else if ((data.composeKey & 0xFF) == VK_APPS) suppressNextContextMenu = false;
            }
            break;
        case WM_CONTEXTMENU:
            if (composing || suppressNextContextMenu) {
                suppressNextContextMenu = false;
                msg.message = WM_NULL;
                return 0;
            }
            break;
        }
    }
    return CallNextHookEx(0, code, wParam, lParam);
}
