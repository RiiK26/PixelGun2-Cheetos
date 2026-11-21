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
#include <cstdint>

namespace IL2CPP
{
  bool      Init();
  bool      IsInitialized();
  HMODULE   GetGameAssembly();

  // Finds an Il2CppClass* by assembly (nullptr/"" searches all assemblies), namespace, and class name.
  void*     FindClass(const char* assemblyName, const char* namespaze, const char* className);

  // Resolves the native method pointer (MethodInfo::methodPointer) for a class method.
  // Pass argCount = -1 to match any parameter count.
  uintptr_t FindMethod(
    const char* assemblyName, const char* namespaze, const char* className, const char* methodName, int argCount = -1
  );

  // Resolves a field's byte offset within its class, or returns -1 if not found.
  int32_t
  FindFieldOffset(const char* assemblyName, const char* namespaze, const char* className, const char* fieldName);
}  // namespace IL2CPP
