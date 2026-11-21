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
  static std::mutex  g_logMutex;
  static FILE*       g_logFile   = nullptr;
  static PVOID       g_vehHandle = nullptr;

  static LONG WINAPI CrashVectoredHandler(EXCEPTION_POINTERS* ep)
  {
    if (!ep || !ep->ExceptionRecord || !ep->ContextRecord) {
      return EXCEPTION_CONTINUE_SEARCH;
    }

    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    // Ignore benign / non-fatal exceptions (C++ EH, MSVC thread naming, breakpoints, RPC)
    if (
      code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION
      && code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_PRIV_INSTRUCTION
      && code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED
    ) {
      return EXCEPTION_CONTINUE_SEARCH;
    }

    void*     addr              = ep->ExceptionRecord->ExceptionAddress;
    HMODULE   mod               = nullptr;
    char      modName[MAX_PATH] = "unknown";
    uintptr_t rva               = 0;
    if (
      GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<LPCSTR>(addr), &mod
      )
      && mod
    ) {
      GetModuleFileNameA(mod, modName, MAX_PATH);
      rva = reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(mod);
    }

    const auto* ctx = ep->ContextRecord;
    char        buf[1024];
    std::snprintf(
      buf, sizeof(buf),
      "[CRASH] Exception 0x%08lX at %p (%s + 0x%llX) | RIP=%llX RSP=%llX RAX=%llX RCX=%llX RDX=%llX R8=%llX R9=%llX\n",
      static_cast<unsigned long>(code), addr, modName, static_cast<unsigned long long>(rva),
      static_cast<unsigned long long>(ctx->Rip), static_cast<unsigned long long>(ctx->Rsp),
      static_cast<unsigned long long>(ctx->Rax), static_cast<unsigned long long>(ctx->Rcx),
      static_cast<unsigned long long>(ctx->Rdx), static_cast<unsigned long long>(ctx->R8),
      static_cast<unsigned long long>(ctx->R9)
    );

    OutputDebugStringA(buf);
    if (g_logFile) {
      std::fputs(buf, g_logFile);
      std::fflush(g_logFile);
    }

    return EXCEPTION_CONTINUE_SEARCH;
  }

  void Init(HMODULE hModule)
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
    if (!g_vehHandle) {
      g_vehHandle = AddVectoredExceptionHandler(1, CrashVectoredHandler);
    }
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
    if (g_vehHandle) {
      RemoveVectoredExceptionHandler(g_vehHandle);
      g_vehHandle = nullptr;
    }
    if (g_logFile) {
      std::fclose(g_logFile);
      g_logFile = nullptr;
    }
  }
}  // namespace Logger

#endif  // ENABLE_DEBUG_LOG
