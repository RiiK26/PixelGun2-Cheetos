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

#include "config.h"
#include "settings_registry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Config
{
  std::string       configPath;

  static std::mutex s_configMutex;

  static bool       HasPersistentEntries(const SettingsRegistry::SettingGroup& group)
  {
    if (group.iniSection.empty())
      return false;

    for (const auto& entry : group.entries) {
      if (!entry.iniKey.empty() && entry.type != SettingsRegistry::SettingType::Action)
        return true;
    }
    return false;
  }

  static std::string Trim(const std::string& str)
  {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
  }

  void SetConfigPath(const std::string& dllPath)
  {
    size_t lastSlash = dllPath.find_last_of("\\/");
    if (lastSlash != std::string::npos)
      configPath = dllPath.substr(0, lastSlash + 1) + "featureconfig.txt";
    else
      configPath = "featureconfig.txt";
  }

  void Save()
  {
    std::lock_guard<std::mutex> lock(s_configMutex);
    if (configPath.empty())
      return;

    std::ofstream file(configPath);
    if (!file.is_open())
      return;

    file << "# Pixel Gun 2 Config\n\n";

    for (const auto& group : SettingsRegistry::GetGroups()) {
      if (!HasPersistentEntries(group))
        continue;

      file << "[" << group.iniSection << "]\n";
      for (const auto& entry : group.entries) {
        if (entry.iniKey.empty() || entry.type == SettingsRegistry::SettingType::Action)
          continue;

        switch (entry.type) {
        case SettingsRegistry::SettingType::Bool :
          if (auto* const ptr = std::get_if<bool*>(&entry.target); ptr && *ptr) {
            file << entry.iniKey << "=" << (**ptr ? "1" : "0") << "\n";
          }
          break;
        case SettingsRegistry::SettingType::Float :
          if (auto* const ptr = std::get_if<float*>(&entry.target); ptr && *ptr) {
            file << entry.iniKey << "=" << **ptr << "\n";
          }
          break;
        case SettingsRegistry::SettingType::Int :
          if (auto* const ptr = std::get_if<int*>(&entry.target); ptr && *ptr) {
            file << entry.iniKey << "=" << **ptr << "\n";
          }
          break;
        case SettingsRegistry::SettingType::Action :
          break;
        }
      }
      file << "\n";
    }

    file.close();
  }

  void Load()
  {
    std::lock_guard<std::mutex> lock(s_configMutex);
    if (configPath.empty())
      return;

    std::ifstream file(configPath);
    if (!file.is_open())
      return;

    std::unordered_map<std::string, std::string> entries;
    std::string                                  currentSection;
    std::string                                  line;
    constexpr size_t                             kMaxConfigLines = 4096;
    size_t                                       lineCount       = 0;

    while (lineCount < kMaxConfigLines && std::getline(file, line)) {
      ++lineCount;
      std::string trimmed = Trim(line);
      if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
        continue;

      if (trimmed.front() == '[' && trimmed.back() == ']') {
        currentSection = Trim(trimmed.substr(1, trimmed.length() - 2));
        continue;
      }

      size_t equalPos = trimmed.find('=');
      if (equalPos != std::string::npos) {
        std::string key     = Trim(trimmed.substr(0, equalPos));
        std::string val     = Trim(trimmed.substr(equalPos + 1));
        std::string fullKey = currentSection.empty() ? key : (currentSection + "." + key);
        entries[fullKey]    = val;
      }
    }
    file.close();

    auto parseBool = [&](const std::string& key, bool& outVal) {
      auto it = entries.find(key);
      if (it != entries.end()) {
        std::string v = it->second;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
          return static_cast<char>(std::tolower(c));
        });
        if (v == "1" || v == "true" || v == "on" || v == "yes")
          outVal = true;
        else if (v == "0" || v == "false" || v == "off" || v == "no")
          outVal = false;
      }
    };

    auto parseFloat = [&](const std::string& key, float& outVal) {
      auto it = entries.find(key);
      if (it != entries.end()) {
        try {
          float parsed = std::stof(it->second);
          if (std::isfinite(parsed)) {
            outVal = parsed;
          }
        } catch (...) {
        }
      }
    };

    auto parseInt = [&](const std::string& key, int& outVal) {
      auto it = entries.find(key);
      if (it != entries.end()) {
        try {
          outVal = std::stoi(it->second);
        } catch (...) {
        }
      }
    };

    for (const auto& group : SettingsRegistry::GetGroups()) {
      if (group.iniSection.empty())
        continue;

      for (const auto& entry : group.entries) {
        if (entry.iniKey.empty() || entry.type == SettingsRegistry::SettingType::Action)
          continue;

        const std::string fullKey = group.iniSection + "." + entry.iniKey;
        switch (entry.type) {
        case SettingsRegistry::SettingType::Bool :
          if (auto* const ptr = std::get_if<bool*>(&entry.target); ptr && *ptr) {
            parseBool(fullKey, **ptr);
          }
          break;
        case SettingsRegistry::SettingType::Float :
          if (auto* const ptr = std::get_if<float*>(&entry.target); ptr && *ptr) {
            parseFloat(fullKey, **ptr);
          }
          break;
        case SettingsRegistry::SettingType::Int :
          if (auto* const ptr = std::get_if<int*>(&entry.target); ptr && *ptr) {
            parseInt(fullKey, **ptr);
          }
          break;
        case SettingsRegistry::SettingType::Action :
          break;
        }
      }
    }
  }
}  // namespace Config
