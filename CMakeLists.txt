cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config.json" VERSION_JSON_CONTENT)
string(JSON PARSED_VERSION GET "${VERSION_JSON_CONTENT}" "cheetos-version")
string(JSON PARSED_PROJECT_NAME GET "${VERSION_JSON_CONTENT}" "project")

project(
    ${PARSED_PROJECT_NAME}
    LANGUAGES CXX C RC
)

if(NOT WIN32)
  message(FATAL_ERROR "Target system is not Windows! This project requires Windows binaries (.dll and .exe). "
                      "If you are compiling from Linux or macOS, please specify a cross-compiler toolchain "
                      "(e.g., -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Release or Debug)." FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug")

option(BUILD_INJECTOR "Build standalone injector executable" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_compile_definitions("PROJECT_VERSION=\"${PARSED_VERSION}\"")
add_compile_definitions("DLL_NAME=\"${PARSED_PROJECT_NAME}.dll\"")
add_compile_definitions("INJECTOR_NAME=\"${PARSED_PROJECT_NAME}_Injector.exe\"")
add_compile_definitions("TARGET_DLL_NAME=\"${PARSED_PROJECT_NAME}.dll\"")
add_compile_definitions("TARGET_INJECTOR_NAME=\"${PARSED_PROJECT_NAME}_Injector.exe\"")

add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)
add_compile_definitions($<$<CONFIG:Debug>:ENABLE_DEBUG_LOG>)

if(MSVC)
  add_compile_options($<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/Ob2> $<$<CONFIG:Release>:/Oi> $<$<CONFIG:Release>:/Ot> $<$<CONFIG:Release>:/GL>)
  add_link_options($<$<CONFIG:Release>:/LTCG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
else()
  add_compile_options($<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-fno-ident>)
endif()

# ============================================
# Vendor Dependencies (MinHook, ImGui)
# ============================================

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(MINHOOK_HDE_SRC "src/vendor/minhook/src/hde/hde64.c")
else()
  set(MINHOOK_HDE_SRC "src/vendor/minhook/src/hde/hde32.c")
endif()

add_library(minhook STATIC
  src/vendor/minhook/src/buffer.c
  src/vendor/minhook/src/hook.c
  src/vendor/minhook/src/trampoline.c
  ${MINHOOK_HDE_SRC}
)
target_include_directories(minhook PUBLIC
  src/vendor/minhook/include
)
target_include_directories(minhook PRIVATE
  src/vendor/minhook/src
  src/vendor/minhook/src/hde
)

add_library(imgui STATIC
  src/vendor/ImGui/imgui.cpp
  src/vendor/ImGui/imgui_draw.cpp
  src/vendor/ImGui/imgui_tables.cpp
  src/vendor/ImGui/imgui_widgets.cpp
  src/vendor/ImGui/backends/imgui_impl_dx11.cpp
  src/vendor/ImGui/backends/imgui_impl_dx12.cpp
  src/vendor/ImGui/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
  src/vendor/ImGui
  src/vendor/ImGui/backends
)

# ============================================
# The DLL
# ============================================
file(GLOB_RECURSE PROJECT_SRC_FILES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
list(FILTER PROJECT_SRC_FILES EXCLUDE REGEX "(/src/vendor/|/src/injector/)") # Vendor and Injector directory are standalone

add_library(trainer MODULE
    ${PROJECT_SRC_FILES}
)

target_include_directories(trainer PRIVATE
    src
)

set_target_properties(trainer PROPERTIES
    OUTPUT_NAME "${PARSED_PROJECT_NAME}"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

target_link_libraries(trainer PRIVATE
    minhook
    imgui
    d3d11
    d3d12
    dxgi
    d3dcompiler
    dwmapi
)

# ============================================
# Standalone Injector (console app)
# ============================================
if(BUILD_INJECTOR)
  add_executable(injector
      src/injector/mainInjector.cpp
      resources/fetchinjectorimage.rc
  )
  set_target_properties(injector PROPERTIES
      OUTPUT_NAME "${PARSED_PROJECT_NAME}_Injector"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endif()

if(MSVC)
  set_property(TARGET trainer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set_property(TARGET minhook PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set_property(TARGET imgui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if(BUILD_INJECTOR)
    set_property(TARGET injector PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
elseif(MINGW)
  target_link_options(trainer PRIVATE -static)
  target_link_options(trainer PRIVATE $<$<CONFIG:Release>:-s> $<$<CONFIG:Release>:-Wl,--gc-sections>)
  if(BUILD_INJECTOR)
    target_link_options(injector PRIVATE -static)
    target_link_options(injector PRIVATE $<$<CONFIG:Release>:-s> $<$<CONFIG:Release>:-Wl,--gc-sections>)
  endif()
endif()
