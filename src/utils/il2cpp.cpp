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

#include "il2cpp.h"
#include "logger.h"

#include <cstring>

namespace IL2CPP
{
  struct MethodInfo
  {
    void* methodPointer;
  };

  using fn_domain_get                 = void* (*)();
  using fn_domain_get_assemblies      = const void** (*)(void* domain, size_t* size);
  using fn_assembly_get_image         = const void* (*)(const void* assembly);
  using fn_image_get_name             = const char* (*)(const void* image);
  using fn_class_from_name            = void* (*)(const void* image, const char* namespaze, const char* name);
  using fn_class_get_method_from_name = const MethodInfo* (*)(void* klass, const char* name, int argsCount);
  using fn_class_get_field_from_name  = void* (*)(void* klass, const char* name);
  using fn_field_get_offset           = size_t (*)(void* field);

  static HMODULE                       g_gameAssembly           = nullptr;
  static bool                          g_initialized            = false;
  static fn_domain_get                 g_domainGet              = nullptr;
  static fn_domain_get_assemblies      g_domainGetAssemblies    = nullptr;
  static fn_assembly_get_image         g_assemblyGetImage       = nullptr;
  static fn_image_get_name             g_imageGetName           = nullptr;
  static fn_class_from_name            g_classFromName          = nullptr;
  static fn_class_get_method_from_name g_classGetMethodFromName = nullptr;
  static fn_class_get_field_from_name  g_classGetFieldFromName  = nullptr;
  static fn_field_get_offset           g_fieldGetOffset         = nullptr;

  template<typename T>
  static bool LoadExport(HMODULE mod, const char* name, T& outFn)
  {
    outFn = reinterpret_cast<T>(GetProcAddress(mod, name));
    if (!outFn) {
      Logger::Log("[IL2CPP] Missing export: %s", name);
      return false;
    }
    return true;
  }

  bool Init()
  {
    if (g_initialized)
      return true;

    g_gameAssembly = GetModuleHandleA("GameAssembly.dll");
    if (!g_gameAssembly) {
      Logger::Log("[IL2CPP] GameAssembly.dll not loaded.");
      return false;
    }

    bool ok = true;
    ok &= LoadExport(g_gameAssembly, "il2cpp_domain_get", g_domainGet);
    ok &= LoadExport(g_gameAssembly, "il2cpp_domain_get_assemblies", g_domainGetAssemblies);
    ok &= LoadExport(g_gameAssembly, "il2cpp_assembly_get_image", g_assemblyGetImage);
    ok &= LoadExport(g_gameAssembly, "il2cpp_image_get_name", g_imageGetName);
    ok &= LoadExport(g_gameAssembly, "il2cpp_class_from_name", g_classFromName);
    ok &= LoadExport(g_gameAssembly, "il2cpp_class_get_method_from_name", g_classGetMethodFromName);
    ok &= LoadExport(g_gameAssembly, "il2cpp_class_get_field_from_name", g_classGetFieldFromName);
    ok &= LoadExport(g_gameAssembly, "il2cpp_field_get_offset", g_fieldGetOffset);

    g_initialized = ok;
    Logger::Log("[IL2CPP] Initialized: %s (base: %p)", ok ? "YES" : "NO", g_gameAssembly);
    return ok;
  }

  bool    IsInitialized() { return g_initialized; }

  HMODULE GetGameAssembly()
  {
    if (!g_gameAssembly) {
      g_gameAssembly = GetModuleHandleA("GameAssembly.dll");
    }
    return g_gameAssembly;
  }

  void* FindClass(const char* assemblyName, const char* namespaze, const char* className)
  {
    if (!g_initialized || !className || !*className)
      return nullptr;

    void* domain = g_domainGet();
    if (!domain)
      return nullptr;

    size_t       count      = 0;
    const void** assemblies = g_domainGetAssemblies(domain, &count);
    if (!assemblies)
      return nullptr;

    const char* ns             = namespaze ? namespaze : "";
    const bool  filterAssembly = (assemblyName && *assemblyName);

    for (size_t i = 0; i < count; ++i) {
      const void* image = g_assemblyGetImage(assemblies[i]);
      if (!image)
        continue;

      if (filterAssembly) {
        const char* imgName = g_imageGetName(image);
        if (!imgName || std::strcmp(imgName, assemblyName) != 0)
          continue;
      }

      void* klass = g_classFromName(image, ns, className);
      if (klass)
        return klass;
    }

    return nullptr;
  }

  uintptr_t FindMethod(
    const char* assemblyName, const char* namespaze, const char* className, const char* methodName, int argCount
  )
  {
    if (!g_initialized || !methodName || !*methodName)
      return 0;

    void* klass = FindClass(assemblyName, namespaze, className);
    if (!klass)
      return 0;

    const MethodInfo* method = g_classGetMethodFromName(klass, methodName, argCount);
    if (!method || !method->methodPointer)
      return 0;

    return reinterpret_cast<uintptr_t>(method->methodPointer);
  }

  int32_t FindFieldOffset(const char* assemblyName, const char* namespaze, const char* className, const char* fieldName)
  {
    if (!g_initialized || !fieldName || !*fieldName)
      return -1;

    void* klass = FindClass(assemblyName, namespaze, className);
    if (!klass)
      return -1;

    void* field = g_classGetFieldFromName(klass, fieldName);
    if (!field)
      return -1;

    return static_cast<int32_t>(g_fieldGetOffset(field));
  }
}  // namespace IL2CPP
