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

#include "logger.h"

#ifdef ENABLE_DEBUG_LOG

  #include <cstdarg>
  #include <cstdio>
  #include <mutex>
  #include <string>

namespace Logger
{
  static std::mutex g_logMutex;
  static FILE*      g_logFile = nullptr;

  void              Init(HMODULE hModule)
  {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile)
      return;

    char        path[MAX_PATH]{};
    DWORD       len     = GetModuleFileNameA(hModule, path, MAX_PATH);
    std::string logPath = "PG2-Cheetos.log";
    if (len > 0 && len < MAX_PATH) {
      logPath    = path;
      size_t dot = logPath.find_last_of('.');
      if (dot != std::string::npos) {
        logPath.replace(dot, std::string::npos, ".log");
      }
      else {
        logPath += ".log";
      }
    }

    g_logFile = std::fopen(logPath.c_str(), "w");
  }

  void Log(const char* fmt, ...)
  {
    std::lock_guard<std::mutex> lock(g_logMutex);

    char                        msg[2048];
    va_list                     args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    SYSTEMTIME st{};
    GetLocalTime(&st);

    char line[2176];
    std::snprintf(line, sizeof(line), "[%02u:%02u:%02u] %s\n", st.wHour, st.wMinute, st.wSecond, msg);

    OutputDebugStringA(line);
    if (g_logFile) {
      std::fputs(line, g_logFile);
      std::fflush(g_logFile);
    }
  }

  void Shutdown()
  {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
      std::fclose(g_logFile);
      g_logFile = nullptr;
    }
  }
}  // namespace Logger

#endif  // ENABLE_DEBUG_LOG
