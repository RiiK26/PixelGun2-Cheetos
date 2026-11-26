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

#include <windows.h>
#include <tlhelp32.h>
#include <cwctype>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <array>

// Console colors
#define COLOR_DEFAULT 7
#define COLOR_GREEN   10
#define COLOR_RED     12
#define COLOR_CYAN    11
#define COLOR_YELLOW  14
#define COLOR_PURPLE  13

namespace pg2_injector
{
  class ScopedHandle
  {
  public:
    ScopedHandle() noexcept = default;
    explicit ScopedHandle(HANDLE h) noexcept :
        m_handle(h)
    {
    }
    ~ScopedHandle() noexcept { Reset(); }

    ScopedHandle(const ScopedHandle&)            = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept :
        m_handle(other.m_handle)
    {
      other.m_handle = nullptr;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
      if (this != &other) {
        Reset();
        m_handle       = other.m_handle;
        other.m_handle = nullptr;
      }
      return *this;
    }

    void Reset(HANDLE h = nullptr) noexcept
    {
      if (m_handle && m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
      }
      m_handle = h;
    }

    [[nodiscard]] HANDLE Get() const noexcept { return m_handle; }
    [[nodiscard]] bool   IsValid() const noexcept { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }
    explicit             operator bool() const noexcept { return IsValid(); }

  private:
    HANDLE m_handle = nullptr;
  };
}  // namespace pg2_injector

using pg2_injector::ScopedHandle;

void SetColor(int color)
{
  static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hConsole && hConsole != INVALID_HANDLE_VALUE) {
    SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
  }
}

#ifndef PROJECT_VERSION
  #define PROJECT_VERSION "Unknown"
#endif

#define WIDEN2(x) L##x
#define WIDEN(x)  WIDEN2(x)

#ifndef DLL_NAME
  #define DLL_NAME "trainer.dll"
#endif

#ifndef INJECTOR_NAME
  #define INJECTOR_NAME "PG2_Injector.exe"
#endif

#define DLL_NAME_W      WIDEN(DLL_NAME)
#define INJECTOR_NAME_W WIDEN(INJECTOR_NAME)

void PrintBanner()
{
  SetColor(COLOR_PURPLE);
  std::string title   = std::string("PIXEL GUN 2 PC TRAINER v") + PROJECT_VERSION;
  int         padding = 46 - static_cast<int>(title.length());
  std::string padStr(padding > 0 ? padding : 0, ' ');

  std::cout << "\n"
            << "    +----------------------------------------------+\n"
            << "    |" << title << padStr << "|\n"
            << "    |                                              |\n"
            << "    |       DLL Injector                           |\n"
            << "    +----------------------------------------------+\n"
            << std::endl;
  SetColor(COLOR_DEFAULT);
}

// Case-insensitive substring search without heap allocation
static bool ContainsCaseInsensitive(std::wstring_view haystack, std::wstring_view needle) noexcept
{
  if (needle.empty() || haystack.size() < needle.size())
    return false;

  const size_t limit = haystack.size() - needle.size();
  for (size_t i = 0; i <= limit; ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      if (std::towlower(haystack[i + j]) != std::towlower(needle[j])) {
        match = false;
        break;
      }
    }
    if (match)
      return true;
  }
  return false;
}

// Known process names for Pixel Gun 2
static constexpr std::array<const wchar_t*, 1> GAME_PROCESS_NAMES = {
  L"Pixel Gun 2.exe",
};

struct ProcessResult
{
  DWORD        pid = 0;
  std::wstring matchedName;
};

static bool IsExactMatch(const wchar_t* exeName) noexcept
{
  for (const wchar_t* name : GAME_PROCESS_NAMES) {
    if (name && _wcsicmp(exeName, name) == 0) {
      return true;
    }
  }
  return false;
}

static bool IsPartialMatch(const wchar_t* exeName) noexcept
{
  // Never match the injector itself
  if (_wcsicmp(exeName, INJECTOR_NAME_W) == 0 || ContainsCaseInsensitive(exeName, L"injector")) {
    return false;
  }

  std::wstring_view view(exeName);
  return ContainsCaseInsensitive(view, L"pixel") && ContainsCaseInsensitive(view, L"gun")
      && ContainsCaseInsensitive(view, L".exe");
}

ProcessResult FindGameProcess()
{
  ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot)
    return {};

  const DWORD     selfPid = GetCurrentProcessId();

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);

  ProcessResult partialMatch;

  if (Process32FirstW(snapshot.Get(), &pe)) {
    do {
      if (pe.th32ProcessID == 0 || pe.th32ProcessID == selfPid)
        continue;

      if (IsExactMatch(pe.szExeFile)) {
        return {pe.th32ProcessID, pe.szExeFile};
      }

      if (partialMatch.pid == 0 && IsPartialMatch(pe.szExeFile)) {
        partialMatch = {pe.th32ProcessID, pe.szExeFile};
      }
    } while (Process32NextW(snapshot.Get(), &pe));
  }

  return partialMatch;
}

// Check if a module is loaded in the target process (handles transient ERROR_BAD_LENGTH during startup)
bool IsModuleLoaded(DWORD pid, const wchar_t* moduleName, HMODULE* outModBase = nullptr)
{
  ScopedHandle snapshot;
  for (int attempt = 0; attempt < 4; ++attempt) {
    snapshot.Reset(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid));
    if (snapshot)
      break;
    DWORD err = GetLastError();
    if (err != ERROR_BAD_LENGTH && err != ERROR_PARTIAL_COPY)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  if (!snapshot)
    return false;

  MODULEENTRY32W me{};
  me.dwSize = sizeof(me);

  if (Module32FirstW(snapshot.Get(), &me)) {
    do {
      if (_wcsicmp(me.szModule, moduleName) == 0) {
        if (outModBase) {
          *outModBase = me.hModule;
        }
        return true;
      }
    } while (Module32NextW(snapshot.Get(), &me));
  }

  return false;
}

// O(1) zero-allocation check whether a specific PID is still running
bool IsProcessAlive(DWORD pid) noexcept
{
  if (pid == 0)
    return false;

  ScopedHandle hProc(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
  if (!hProc) {
    // Fallback for environments where SYNCHRONIZE is restricted
    hProc.Reset(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid));
    if (!hProc)
      return false;
  }

  DWORD waitRes = WaitForSingleObject(hProc.Get(), 0);
  if (waitRes == WAIT_TIMEOUT)
    return true;

  DWORD exitCode = 0;
  if (GetExitCodeProcess(hProc.Get(), &exitCode)) {
    return exitCode == STILL_ACTIVE;
  }
  return false;
}

// List all running processes (debug helper)
void ListProcesses()
{
  ScopedHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot)
    return;

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);

  SetColor(COLOR_YELLOW);
  std::cout << "  [*] Listing all running processes:" << std::endl;
  SetColor(COLOR_DEFAULT);

  int count = 0;
  if (Process32FirstW(snapshot.Get(), &pe)) {
    do {
      std::wcout << L"      PID " << pe.th32ProcessID << L": " << pe.szExeFile << std::endl;
      count++;
    } while (Process32NextW(snapshot.Get(), &pe));
  }
  std::cout << "  [*] Total: " << count << " processes" << std::endl;
}

// Inject DLL via LoadLibraryW
bool InjectDLL(DWORD pid, const std::wstring& dllPath)
{
  constexpr DWORD kRequiredAccess =
    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;

  ScopedHandle hProcess(OpenProcess(kRequiredAccess, FALSE, pid));
  if (!hProcess) {
    // Fallback to PROCESS_ALL_ACCESS if Wine/OS mapping prefers it
    hProcess.Reset(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
  }

  if (!hProcess) {
    DWORD err = GetLastError();
    SetColor(COLOR_RED);
    std::cout << "  [!] Failed to open process (error " << err << ")." << std::endl;
    if (err == ERROR_ACCESS_DENIED) {
      std::cout << "  [!] Access denied. Run as Administrator or in the same Wine prefix." << std::endl;
    }
    SetColor(COLOR_DEFAULT);
    return false;
  }

  SetColor(COLOR_CYAN);
  std::cout << "  [*] Process opened successfully." << std::endl;
  SetColor(COLOR_DEFAULT);

  // Allocate memory in target process for DLL path
  const size_t pathSize  = (dllPath.length() + 1) * sizeof(wchar_t);
  void*        remoteMem = VirtualAllocEx(hProcess.Get(), nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remoteMem) {
    DWORD err = GetLastError();
    SetColor(COLOR_RED);
    std::cout << "  [!] Failed to allocate memory in target (error " << err << ")." << std::endl;
    SetColor(COLOR_DEFAULT);
    return false;
  }

  // Write DLL path to target process
  SIZE_T bytesWritten = 0;
  if (
    !WriteProcessMemory(hProcess.Get(), remoteMem, dllPath.c_str(), pathSize, &bytesWritten) || bytesWritten != pathSize
  ) {
    DWORD err = GetLastError();
    SetColor(COLOR_RED);
    std::cout << "  [!] Failed to write DLL path to target (error " << err << ")." << std::endl;
    SetColor(COLOR_DEFAULT);
    VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
    return false;
  }

  // Get LoadLibraryW address
  HMODULE hKernel32   = GetModuleHandleW(L"kernel32.dll");
  FARPROC loadLibrary = hKernel32 ? GetProcAddress(hKernel32, "LoadLibraryW") : nullptr;
  if (!loadLibrary) {
    DWORD err = GetLastError();
    SetColor(COLOR_RED);
    std::cout << "  [!] Failed to resolve LoadLibraryW (error " << err << ")." << std::endl;
    SetColor(COLOR_DEFAULT);
    VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
    return false;
  }

  // Create remote thread to call LoadLibraryW
  ScopedHandle hThread(CreateRemoteThread(
    hProcess.Get(), nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary), remoteMem, 0, nullptr
  ));

  if (!hThread) {
    DWORD err = GetLastError();
    SetColor(COLOR_RED);
    std::cout << "  [!] Failed to create remote thread (error " << err << ")." << std::endl;
    SetColor(COLOR_DEFAULT);
    VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);
    return false;
  }

  SetColor(COLOR_CYAN);
  std::cout << "  [*] Remote thread created, waiting for DLL load..." << std::endl;
  SetColor(COLOR_DEFAULT);

  // Wait for injection to complete
  DWORD waitResult = WaitForSingleObject(hThread.Get(), 15000);
  if (waitResult == WAIT_TIMEOUT) {
    SetColor(COLOR_RED);
    std::cout << "  [!] Timed out waiting for remote LoadLibraryW thread (15s)." << std::endl;
    SetColor(COLOR_DEFAULT);
    // Do NOT call VirtualFreeEx while the remote thread is still executing and reading remoteMem!
    return false;
  }

  // Thread has finished executing LoadLibraryW; safe to release remote path buffer
  VirtualFreeEx(hProcess.Get(), remoteMem, 0, MEM_RELEASE);

  DWORD exitCode = 0;
  GetExitCodeThread(hThread.Get(), &exitCode);

  // Note: On x86_64, GetExitCodeThread truncates the 64-bit HMODULE to 32 bits.
  // Verify via module snapshot as well in case the low DWORD of the 64-bit base address is 0.
  bool loaded = (exitCode != 0) || IsModuleLoaded(pid, DLL_NAME_W);
  if (!loaded) {
    SetColor(COLOR_RED);
    std::cout << "  [!] LoadLibraryW failed — DLL did not load in target." << std::endl;
    std::cout << "  [!] Check that the DLL path is valid from the game's perspective." << std::endl;
    SetColor(COLOR_DEFAULT);
  }

  return loaded;
}

int main(int argc, char* argv[])
{
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  SetConsoleTitleW(L"PG2 Trainer Injector");
  PrintBanner();

  bool debugMode      = false;
  bool noWait         = false;
  int  timeoutSeconds = 120;

  int  i              = 1;
  while (i < argc) {
    std::string_view arg(argv[i]);
    if (arg == "--debug" || arg == "-d") {
      debugMode = true;
      i++;
    }
    else if (arg == "--no-wait" || arg == "-n") {
      noWait = true;
      i++;
    }
    else if ((arg == "--timeout" || arg == "-t") && i + 1 < argc) {
      int parsed = std::atoi(argv[++i]);
      if (parsed > 0) {
        timeoutSeconds = parsed;
      }
      i++;
    }
    else {
      i++;
    }
  }

  // Find DLL path (same directory as injector)
  wchar_t exePath[MAX_PATH]{};
  DWORD   pathLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  if (pathLen == 0 || pathLen >= MAX_PATH) {
    SetColor(COLOR_RED);
    std::cout << "  [!] Failed to resolve injector executable path." << std::endl;
    SetColor(COLOR_DEFAULT);
    return 1;
  }

  std::filesystem::path dllPath = std::filesystem::path(exePath).parent_path() / DLL_NAME_W;

  std::error_code       ec;
  if (!std::filesystem::exists(dllPath, ec)) {
    SetColor(COLOR_RED);
    std::cout << "  [!] " << DLL_NAME << " not found!" << std::endl;
    std::cout << "  [!] Place " << DLL_NAME << " next to the injector." << std::endl;
    SetColor(COLOR_DEFAULT);
    if (!noWait) {
      std::cout << "\n  Press ENTER to exit..." << std::endl;
      std::cin.get();
    }
    return 1;
  }

  SetColor(COLOR_GREEN);
  std::cout << "  [+] Found " << DLL_NAME << std::endl;
  SetColor(COLOR_DEFAULT);

  std::wcout << L"  [*] DLL path: " << dllPath.wstring() << std::endl;

  // Wait for game process
  SetColor(COLOR_YELLOW);
  std::cout << "  [*] Waiting for Pixel Gun 2 (timeout: " << timeoutSeconds << "s)..." << std::endl;
  std::cout << "  [*] Scanning for: Pixel Gun 2.exe" << std::endl;
  SetColor(COLOR_DEFAULT);

  if (debugMode) {
    ListProcesses();
  }

  ProcessResult gameProc;
  int           waitCount = 0;
  while (gameProc.pid == 0) {
    gameProc = FindGameProcess();
    if (gameProc.pid == 0) {
      if (waitCount >= timeoutSeconds) {
        SetColor(COLOR_RED);
        std::cout << "  [!] Timed out waiting for Pixel Gun 2 after " << timeoutSeconds << "s." << std::endl;
        SetColor(COLOR_DEFAULT);
        return 1;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      waitCount++;
      if (waitCount % 10 == 0) {
        SetColor(COLOR_YELLOW);
        std::cout << "  [*] Still waiting... (" << waitCount << "s / " << timeoutSeconds << "s)" << std::endl;
        if (waitCount == 30) {
          std::cout << "  [*] Tip: If the game is running, try --debug flag to list processes." << std::endl;
        }
        SetColor(COLOR_DEFAULT);
      }
    }
  }

  SetColor(COLOR_GREEN);
  std::cout << "  [+] Found game process (PID: " << gameProc.pid << ")" << std::endl;
  std::wcout << L"  [+] Matched process name: " << gameProc.matchedName << std::endl;
  SetColor(COLOR_DEFAULT);

  // Wait for GameAssembly.dll to load (IL2CPP runtime)
  SetColor(COLOR_YELLOW);
  std::cout << "  [*] Waiting for GameAssembly.dll..." << std::endl;
  SetColor(COLOR_DEFAULT);

  int moduleChecks = 0;
  while (!IsModuleLoaded(gameProc.pid, L"GameAssembly.dll")) {
    if (!IsProcessAlive(gameProc.pid)) {
      SetColor(COLOR_RED);
      std::cout << "  [!] Game process exited before GameAssembly.dll loaded." << std::endl;
      SetColor(COLOR_DEFAULT);
      return 1;
    }
    if (++moduleChecks > 240) {  // 120 seconds (240 * 500ms)
      SetColor(COLOR_RED);
      std::cout << "  [!] Timed out waiting for GameAssembly.dll to load." << std::endl;
      SetColor(COLOR_DEFAULT);
      return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  SetColor(COLOR_GREEN);
  std::cout << "  [+] GameAssembly.dll loaded" << std::endl;
  SetColor(COLOR_DEFAULT);

  // Wait a bit for IL2CPP to fully initialize
  SetColor(COLOR_YELLOW);
  std::cout << "  [*] Waiting for IL2CPP initialization (3s)..." << std::endl;
  SetColor(COLOR_DEFAULT);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  if (!IsProcessAlive(gameProc.pid)) {
    SetColor(COLOR_RED);
    std::cout << "  [!] Game process exited during initialization." << std::endl;
    SetColor(COLOR_DEFAULT);
    return 1;
  }

  // Check if trainer is already injected
  if (IsModuleLoaded(gameProc.pid, DLL_NAME_W)) {
    SetColor(COLOR_YELLOW);
    std::cout << "  [!] Trainer is already injected!" << std::endl;
    SetColor(COLOR_DEFAULT);
    return 0;
  }

  // Inject
  SetColor(COLOR_CYAN);
  std::cout << "  [*] Injecting " << DLL_NAME << "..." << std::endl;
  SetColor(COLOR_DEFAULT);

  std::wstring fullDllPath = dllPath.wstring();
  bool         injected    = InjectDLL(gameProc.pid, fullDllPath);
  if (injected) {
    SetColor(COLOR_GREEN);
    std::cout << "  [+] Injection successful!" << std::endl;
    std::cout << "  [+] Press INSERT in-game to toggle menu" << std::endl;
    std::cout << "  [+] Press END to unload trainer" << std::endl;
    SetColor(COLOR_DEFAULT);
  }
  else {
    SetColor(COLOR_RED);
    std::cout << "  [!] Injection failed!" << std::endl;
    std::cout << "  [!] Make sure you are running in the same Wine/Proton prefix as the game." << std::endl;
    std::cout << "  [!] On Linux, use: protontricks -c 'wine " << INJECTOR_NAME << "' 3456950" << std::endl;
    SetColor(COLOR_DEFAULT);
  }

  if (!noWait) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return injected ? 0 : 1;
}
