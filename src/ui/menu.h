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

#pragma once

#include <windows.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Menu
{
  struct HeaderPayload
  {
    bool isExpanded{true};
  };

  struct BoolPayload
  {
    bool* value{nullptr};
  };

  template<typename T>
  struct SliderPayload
  {
    T* value{nullptr};
    T  min{};
    T  max{};
    T  step{};
  };

  using FloatPayload = SliderPayload<float>;
  using IntPayload   = SliderPayload<int>;

  struct ChoicePayload
  {
    int*                     value{nullptr};
    std::vector<std::string> labels{};
  };

  struct ActionPayload
  {
    void (*action)(){nullptr};
  };

  using ItemPayload = std::variant<HeaderPayload, BoolPayload, FloatPayload, IntPayload, ChoicePayload, ActionPayload>;

  struct MenuItem
  {
    std::string     name{};
    ItemPayload     payload{HeaderPayload{}};

    static MenuItem Header(std::string name, bool isExpanded = true)
    {
      return MenuItem{std::move(name), HeaderPayload{isExpanded}};
    }
    static MenuItem Toggle(std::string name, bool* val) { return MenuItem{std::move(name), BoolPayload{val}}; }
    static MenuItem Slider(std::string name, float* val, float min, float max, float step)
    {
      return MenuItem{
        std::move(name), FloatPayload{val, min, max, step}
      };
    }
    static MenuItem Slider(std::string name, int* val, int min, int max, int step)
    {
      return MenuItem{
        std::move(name), IntPayload{val, min, max, step}
      };
    }
    static MenuItem Choice(std::string name, int* val, std::vector<std::string> labels)
    {
      return MenuItem{
        std::move(name), ChoicePayload{val, std::move(labels)}
      };
    }
    static MenuItem Action(std::string name, void (*act)()) { return MenuItem{std::move(name), ActionPayload{act}}; }
  };

  void AddMenuItem(MenuItem item);
  void ClearMenuItems();
  bool IsOpen();
  void SetOpen(bool open);

  void InitImGuiCommon(HWND hWnd);
  void Render();
  void Shutdown();
}  // namespace Menu
