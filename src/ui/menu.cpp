/*  Copyright 2026 - RiiK
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "menu.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Menu
{
  HWND    hGameWindow = nullptr;
  WNDPROC oWndProc    = nullptr;
  bool    initialized = false;

  namespace Settings
  {
    inline bool bMenuOpen = true;
  }

  std::vector<MenuItem> items;
  int                   selectedIndex = 0;
  int                   scrollOffset  = 0;

  void                  AddMenuItem(MenuItem item) { items.push_back(std::move(item)); }

  void                  ClearMenuItems()
  {
    items.clear();
    selectedIndex = 0;
    scrollOffset  = 0;
  }

  bool                    IsOpen() { return Settings::bMenuOpen; }

  void                    SetOpen(bool open) { Settings::bMenuOpen = open; }

  static std::vector<int> s_visibleIndices;

  const std::vector<int>& GetVisibleIndices()
  {
    s_visibleIndices.clear();
    s_visibleIndices.reserve(items.size());
    bool currentHeaderExpanded = true;
    for (size_t i = 0; i < items.size(); ++i) {
      if (const auto* header = std::get_if<HeaderPayload>(&items[i].payload)) {
        currentHeaderExpanded = header->isExpanded;
        s_visibleIndices.push_back(static_cast<int>(i));
      }
      else if (currentHeaderExpanded) {
        s_visibleIndices.push_back(static_cast<int>(i));
      }
    }
    return s_visibleIndices;
  }

  void MoveSelection(int dir)
  {
    if (items.empty())
      return;

    const auto& visible = GetVisibleIndices();
    if (visible.empty())
      return;

    auto it           = std::find(visible.begin(), visible.end(), selectedIndex);
    int  visibleIndex = 0;
    if (it != visible.end()) {
      visibleIndex = static_cast<int>(std::distance(visible.begin(), it));
    }

    visibleIndex += dir;
    if (visibleIndex < 0)
      visibleIndex = static_cast<int>(visible.size()) - 1;
    if (visibleIndex >= static_cast<int>(visible.size()))
      visibleIndex = 0;

    selectedIndex = visible[visibleIndex];
  }

  template<class... Ts>
  struct Overloaded: Ts...
  {
    using Ts::operator()...;
  };
  template<class... Ts>
  Overloaded(Ts...) -> Overloaded<Ts...>;

  template<typename T>
  void AdjustSlider(const SliderPayload<T>& slider, int dir)
  {
    if (dir > 0) {
      *slider.value = std::min(slider.max, *slider.value + slider.step);
    }
    else if (dir < 0) {
      *slider.value = std::max(slider.min, *slider.value - slider.step);
    }
  }

  void Interact(int dir)
  {
    if (items.empty() || selectedIndex < 0 || selectedIndex >= static_cast<int>(items.size()))
      return;
    auto& item = items[selectedIndex];

    std::visit(
      Overloaded{
        [&](HeaderPayload& header) {
          if (dir != 0)
            header.isExpanded = !header.isExpanded;
        },
        [&](const BoolPayload& toggle) {
          if (!toggle.value)
            return;
          if (dir < 0) {
            *toggle.value = false;
          }
          else if (dir > 0 && dir != 2) {  // 1 is right, 2 is enter
            *toggle.value = true;
          }
          else if (dir == 2) {
            *toggle.value = !(*toggle.value);
          }
        },
        [&]<typename T>(const SliderPayload<T>& slider) {
          if (!slider.value)
            return;
          AdjustSlider(slider, dir);
        },
        [&](const ChoicePayload& choice) {
          if (!choice.value || choice.labels.empty())
            return;
          const int count = static_cast<int>(choice.labels.size());
          int       idx   = *choice.value;
          if (dir < 0) {
            idx = (idx - 1 + count) % count;
          }
          else if (dir > 0) {  // right arrow or enter
            idx = (idx + 1) % count;
          }
          *choice.value = idx;
        },
        [&](const ActionPayload& act) {
          if (act.action && dir != 0) {
            act.action();
          }
        },
      },
      item.payload
    );
  }

  static bool IsMouseMessage(UINT uMsg)
  {
    return uMsg == WM_MOUSEMOVE || uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_RBUTTONDOWN
        || uMsg == WM_RBUTTONUP || uMsg == WM_MOUSEWHEEL || uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONUP;
  }

  static bool IsKeyboardMessage(UINT uMsg)
  {
    return uMsg == WM_KEYDOWN || uMsg == WM_KEYUP || uMsg == WM_CHAR || uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP;
  }

  LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
      if ((lParam & (1 << 30)) == 0) {
        Settings::bMenuOpen = !Settings::bMenuOpen;
      }
      return 1;
    }
    if (msg == WM_KEYUP && wParam == VK_INSERT) {
      return 1;
    }

    if (ImGui::GetCurrentContext()) {
      ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    if (Settings::bMenuOpen) {
      if (msg == WM_KEYDOWN) {
        if (wParam == VK_UP) {
          MoveSelection(-1);
          return 1;
        }
        if (wParam == VK_DOWN) {
          MoveSelection(1);
          return 1;
        }
        if (wParam == VK_LEFT) {
          Interact(-1);
          return 1;
        }
        if (wParam == VK_RIGHT) {
          Interact(1);
          return 1;
        }
        if (wParam == VK_RETURN) {
          Interact(2);
          return 1;
        }
      }

      ImGuiIO& io = ImGui::GetIO();
      if (io.WantCaptureMouse && IsMouseMessage(msg)) {
        return 1;
      }
      if (io.WantCaptureKeyboard && IsKeyboardMessage(msg)) {
        return 1;
      }
    }

    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
  }

  void InitImGuiCommon(HWND hWnd)
  {
    if (initialized)
      return;

    hGameWindow = hWnd;
    if (hGameWindow && !oWndProc) {
      oWndProc =
        reinterpret_cast<WNDPROC>(SetWindowLongPtr(hGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    if (!ImGui::GetCurrentContext()) {
      ImGui::CreateContext();
    }
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    static std::string s_iniPath;
    HMODULE            hSelf = nullptr;
    if (
      GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&InitImGuiCommon), &hSelf
      )
      && hSelf
    ) {
      char  dllPath[MAX_PATH]{};
      DWORD len = GetModuleFileNameA(hSelf, dllPath, MAX_PATH);
      if (len > 0 && len < MAX_PATH) {
        std::string dir(dllPath, len);
        size_t      slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) {
          s_iniPath = dir.substr(0, slash + 1) + "imgui.ini";
        }
      }
    }
    io.IniFilename = s_iniPath.empty() ? nullptr : s_iniPath.c_str();

    ImFontConfig fontConfig;
    fontConfig.OversampleH                 = 1;
    fontConfig.OversampleV                 = 1;
    fontConfig.PixelSnapH                  = true;

    static const ImWchar basicAndSymbols[] = {
      0x0020, 0x00FF,  // Basic Latin + Latin Supplement
      0x0100, 0x024F,  // Latin Extended-A & Latin Extended-B
      0x0370, 0x03FF,  // Greek and Coptic
      0x0400, 0x052F,  // Cyrillic + Cyrillic Supplement
      0x0E00, 0x0E7F,  // Thai
      0x2000, 0x20CF,  // General Punctuation, Superscripts/Subscripts, Currency
      0x2100, 0x218F,  // Letterlike Symbols, Number Forms
      0x2190, 0x22FF,  // Arrows, Mathematical Operators
      0x2300, 0x23FF,  // Miscellaneous Technical
      0x2460, 0x24FF,  // Enclosed Alphanumerics
      0x2500, 0x257F,  // Box Drawing
      0x2580, 0x259F,  // Block Elements
      0x25A0, 0x25FF,  // Geometric Shapes
      0x2600, 0x26FF,  // Miscellaneous Symbols
      0x2700, 0x27BF,  // Dingbats
      0x2B00, 0x2BFF,  // Miscellaneous Symbols and Arrows
      0x2DE0, 0x2DFF,  // Cyrillic Extended-A
      0x3000, 0x303F,  // CJK Symbols and Punctuation
      0x3040, 0x309F,  // Hiragana
      0x30A0, 0x30FF,  // Katakana
      0x31F0, 0x31FF,  // Katakana Phonetic Extensions
      0xA640, 0xA69F,  // Cyrillic Extended-B
      0xFF00, 0xFFEF,  // Halfwidth and Fullwidth Forms
      0,
    };

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(basicAndSymbols);
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    static ImVector<ImWchar> combinedRanges;
    if (combinedRanges.empty()) {
      builder.BuildRanges(&combinedRanges);
    }

    const char* fontCandidates[] = {"C:\\Windows\\Fonts\\msyh.ttc",     "C:\\Windows\\Fonts\\msyh.ttf",
                                    "C:\\Windows\\Fonts\\msgothic.ttc", "C:\\Windows\\Fonts\\malgun.ttf",
                                    "C:\\Windows\\Fonts\\segoeui.ttf",  "C:\\Windows\\Fonts\\arial.ttf"};

    bool        fontLoaded       = false;
    for (const char* fontPath : fontCandidates) {
      DWORD fileAttr = GetFileAttributesA(fontPath);
      if (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        if (io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &fontConfig, combinedRanges.Data)) {
          fontLoaded = true;
          break;
        }
      }
    }

    if (!fontLoaded) {
      io.Fonts->AddFontDefault();
    }
    else {
      DWORD symAttr = GetFileAttributesA("C:\\Windows\\Fonts\\seguisym.ttf");
      if (symAttr != INVALID_FILE_ATTRIBUTES && !(symAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        static const ImWchar symRanges[] = {0x2000, 0x2BFF, 0};
        ImFontConfig         symConfig   = fontConfig;
        symConfig.MergeMode              = true;
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", 16.0f, &symConfig, symRanges);
      }
    }

    ImGui_ImplWin32_Init(hGameWindow);

    ImGuiStyle& style               = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);

    initialized                     = true;
  }

  void Render()
  {
    if (!Settings::bMenuOpen)
      return;

    auto*       drawList       = ImGui::GetBackgroundDrawList();

    float       screenH        = ImGui::GetIO().DisplaySize.y;
    float       startX         = 5.0f;
    float       startY         = 5.0f;
    float       rowHeight      = 20.0f;
    float       padding        = 10.0f;

    const auto& visibleIndices = GetVisibleIndices();
    if (visibleIndices.empty())
      return;

#ifndef PROJECT_VERSION
  #define PROJECT_VERSION "dev"
#endif

    static const std::string titleStr = std::string("Pixel Gun 2 Cheat v") + PROJECT_VERSION;

    float                    width    = 250.0f;
    float                    titleW   = ImGui::CalcTextSize(titleStr.c_str()).x + padding * 2;
    if (titleW > width)
      width = titleW;

    for (int idx : visibleIndices) {
      auto& item       = items[idx];
      float textW      = ImGui::CalcTextSize(item.name.c_str()).x;
      float itemTotalW = textW + 100.0f;
      if (itemTotalW > width) {
        width = itemTotalW;
      }
    }

    constexpr const char* kFooterMeasureText = "[ARROWS] Navigate | [INSERT] Hide";
    float                 footerTextW        = ImGui::CalcTextSize(kFooterMeasureText).x + padding * 2;
    if (footerTextW > width) {
      width = footerTextW;
    }

    float availableHeight = screenH - startY - (padding * 2) - 30.0f - 20.0f;
    int   maxVisibleItems = static_cast<int>(availableHeight / rowHeight);
    if (maxVisibleItems < 5)
      maxVisibleItems = 5;

    auto it                   = std::find(visibleIndices.begin(), visibleIndices.end(), selectedIndex);
    int  selectedVisibleIndex = 0;
    if (it != visibleIndices.end()) {
      selectedVisibleIndex = static_cast<int>(std::distance(visibleIndices.begin(), it));
    }
    else {
      selectedIndex        = visibleIndices[0];
      selectedVisibleIndex = 0;
    }

    if (selectedVisibleIndex < scrollOffset) {
      scrollOffset = selectedVisibleIndex;
    }
    else if (selectedVisibleIndex >= scrollOffset + maxVisibleItems) {
      scrollOffset = selectedVisibleIndex - maxVisibleItems + 1;
    }

    if (scrollOffset > static_cast<int>(visibleIndices.size()) - maxVisibleItems) {
      scrollOffset = std::max(0, static_cast<int>(visibleIndices.size()) - maxVisibleItems);
    }

    int   itemsToDraw = std::min(maxVisibleItems, static_cast<int>(visibleIndices.size()) - scrollOffset);
    float totalHeight = itemsToDraw * rowHeight + padding * 2 + 30.0f;

    // Background
    drawList->AddRectFilled(
      ImVec2(startX, startY), ImVec2(startX + width, startY + totalHeight), IM_COL32(20, 20, 25, 230)
    );
    drawList->AddRect(
      ImVec2(startX, startY), ImVec2(startX + width, startY + totalHeight), IM_COL32(100, 50, 200, 255), 0, 0, 2.0f
    );

    // Title
    drawList->AddText(ImVec2(startX + padding, startY + padding), IM_COL32(100, 200, 255, 255), titleStr.c_str());

    float currentY = startY + padding + 30.0f;

    for (int i = 0; i < itemsToDraw; ++i) {
      int   actualIndex = visibleIndices[scrollOffset + i];
      auto& item        = items[actualIndex];
      ImU32 textColor   = IM_COL32(200, 200, 200, 255);

      if (actualIndex == selectedIndex) {
        drawList->AddRectFilled(
          ImVec2(startX, currentY), ImVec2(startX + width, currentY + rowHeight), IM_COL32(80, 40, 160, 180)
        );
        textColor = IM_COL32(255, 255, 255, 255);
      }

      if (!std::holds_alternative<HeaderPayload>(item.payload)) {
        drawList->AddText(ImVec2(startX + padding, currentY + 2), textColor, item.name.c_str());
      }

      std::visit(
        Overloaded{
          [&](const HeaderPayload& header) {
            textColor = IM_COL32(150, 150, 150, 255);
            char headerBuf[128];
            snprintf(headerBuf, sizeof(headerBuf), "%s%s", item.name.c_str(), header.isExpanded ? " [-]" : " [+]");
            drawList->AddText(ImVec2(startX + width / 2 - 40, currentY + 2), textColor, headerBuf);
          },
          [&](const BoolPayload& toggle) {
            if (!toggle.value)
              return;
            const char* valStr   = *toggle.value ? "[ON]" : "[OFF]";
            ImU32       valColor = *toggle.value ? IM_COL32(50, 255, 50, 255) : IM_COL32(255, 50, 50, 255);
            drawList->AddText(ImVec2(startX + width - 60, currentY + 2), valColor, valStr);
          },
          [&]<typename T>(const SliderPayload<T>& slider) {
            if (!slider.value)
              return;
            char buf[32];
            if constexpr (std::is_floating_point_v<T>) {
              snprintf(buf, sizeof(buf), "< %.2f >", *slider.value);
            }
            else {
              snprintf(buf, sizeof(buf), "< %d >", *slider.value);
            }
            drawList->AddText(ImVec2(startX + width - 80, currentY + 2), IM_COL32(255, 255, 100, 255), buf);
          },
          [&](const ChoicePayload& choice) {
            if (!choice.value || choice.labels.empty())
              return;
            const int count = static_cast<int>(choice.labels.size());
            const int idx   = std::clamp(*choice.value, 0, count - 1);
            char      buf[48];
            snprintf(buf, sizeof(buf), "< %s >", choice.labels[idx].c_str());
            const float textWidth = ImGui::CalcTextSize(buf).x;
            drawList->AddText(
              ImVec2(startX + width - padding - textWidth, currentY + 2), IM_COL32(255, 255, 100, 255), buf
            );
          },
          [&](const ActionPayload&) {
            drawList->AddText(ImVec2(startX + width - 80, currentY + 2), IM_COL32(200, 200, 200, 255), "[ENTER]");
          },
        },
        item.payload
      );

      currentY += rowHeight;
    }

    // Footer
    float footerHeight = 25.0f;
    float footerStartY = startY + totalHeight;
    drawList->AddRectFilled(
      ImVec2(startX, footerStartY), ImVec2(startX + width, footerStartY + footerHeight), IM_COL32(30, 30, 40, 230)
    );
    drawList->AddRect(
      ImVec2(startX, footerStartY), ImVec2(startX + width, footerStartY + footerHeight), IM_COL32(100, 50, 200, 255), 0,
      0, 2.0f
    );

    constexpr const char* kFooterText = "[ARROWS] Navigate|[INSERT] Hide";
    footerTextW                       = ImGui::CalcTextSize(kFooterText).x;
    drawList->AddText(
      ImVec2(startX + (width - footerTextW) / 2.0f, footerStartY + 5.0f), IM_COL32(150, 150, 150, 255), kFooterText
    );
  }

  void Shutdown()
  {
    if (!initialized)
      return;

    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (oWndProc && hGameWindow) {
      SetWindowLongPtr(hGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
      oWndProc = nullptr;
    }
    hGameWindow = nullptr;

    initialized = false;
  }
}  // namespace Menu
