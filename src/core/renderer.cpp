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

#include "renderer.h"

#include "../ui/menu.h"
#include "../utils/logger.h"

#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <MinHook.h>

namespace RendererHook
{
  using fn_Present             = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
  inline fn_Present oPresent   = nullptr;

  using fn_Present1            = HRESULT(WINAPI*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
  inline fn_Present1 oPresent1 = nullptr;

  using fn_ResizeBuffers       = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
  inline fn_ResizeBuffers oResizeBuffers = nullptr;

  using fn_ExecuteCommandLists           = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
  inline fn_ExecuteCommandLists     oExecuteCommandLists           = nullptr;

  inline bool                       hookInstalled                  = false;
  inline bool                       g_shuttingDown                 = false;
  inline OverlayCallback            g_overlayCallback              = nullptr;

  // DirectX 11 Device & Resources
  inline ID3D11Device*              g_pd3d11Device                 = nullptr;
  inline ID3D11DeviceContext*       g_pd3d11Context                = nullptr;
  inline ID3D11RenderTargetView*    g_pd3d11RenderTarget           = nullptr;
  inline IDXGISwapChain*            g_pSwapChain                   = nullptr;
  inline bool                       g_initializedDX11              = false;

  // DirectX 12 Device & Resources
  inline ID3D12Device*              g_pd3dDevice                   = nullptr;
  inline ID3D12DescriptorHeap*      g_pd3dRtvDescHeap              = nullptr;
  inline ID3D12DescriptorHeap*      g_pd3dSrvDescHeap              = nullptr;
  inline ID3D12CommandQueue*        g_pd3dCommandQueue             = nullptr;
  inline ID3D12GraphicsCommandList* g_pd3dCommandList              = nullptr;
  inline ID3D12CommandAllocator*    g_commandAllocators[16]        = {};
  inline ID3D12Resource*            g_mainRenderTargetResource[16] = {};
  inline UINT                       g_NUM_FRAMES_IN_FLIGHT         = 16;
  inline bool                       g_initializedDX12              = false;

  static bool                       GetSwapChainDesc(IDXGISwapChain* pSwapChain, DXGI_SWAP_CHAIN_DESC& sd)
  {
    ZeroMemory(&sd, sizeof(sd));
    if (SUCCEEDED(pSwapChain->GetDesc(&sd))) {
      return true;
    }

    IDXGISwapChain1* pSwapChain1 = nullptr;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&pSwapChain1)))) {
      DXGI_SWAP_CHAIN_DESC1 sd1;
      if (SUCCEEDED(pSwapChain1->GetDesc1(&sd1))) {
        sd.BufferCount       = sd1.BufferCount;
        sd.BufferDesc.Format = sd1.Format;
        sd.BufferDesc.Width  = sd1.Width;
        sd.BufferDesc.Height = sd1.Height;
        sd.SampleDesc        = sd1.SampleDesc;
        sd.BufferUsage       = sd1.BufferUsage;
        sd.SwapEffect        = sd1.SwapEffect;
        sd.Flags             = sd1.Flags;
        pSwapChain1->GetHwnd(&sd.OutputWindow);
        pSwapChain1->Release();
        return true;
      }
      pSwapChain1->Release();
    }
    return false;
  }

  static void CleanupRenderTargetDX11()
  {
    if (g_pd3d11RenderTarget) {
      g_pd3d11RenderTarget->Release();
      g_pd3d11RenderTarget = nullptr;
    }
  }

  static void CreateRenderTargetDX11(IDXGISwapChain* pSwapChain)
  {
    if (!g_pd3d11Device)
      return;
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer)))) {
      g_pd3d11Device->CreateRenderTargetView(pBackBuffer, nullptr, &g_pd3d11RenderTarget);
      pBackBuffer->Release();
    }
  }

  static void ReleaseDX11Objects()
  {
    CleanupRenderTargetDX11();
    if (g_pd3d11Context) {
      g_pd3d11Context->Release();
      g_pd3d11Context = nullptr;
    }
    if (g_pd3d11Device) {
      g_pd3d11Device->Release();
      g_pd3d11Device = nullptr;
    }
    g_pSwapChain = nullptr;
  }

  static bool InitDX11(IDXGISwapChain* pSwapChain)
  {
    g_pSwapChain = pSwapChain;

    if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_pd3d11Device))))
      return false;

    g_pd3d11Device->GetImmediateContext(&g_pd3d11Context);

    CreateRenderTargetDX11(pSwapChain);
    if (!g_pd3d11RenderTarget) {
      ReleaseDX11Objects();
      return false;
    }

    DXGI_SWAP_CHAIN_DESC desc;
    if (!GetSwapChainDesc(pSwapChain, desc)) {
      ReleaseDX11Objects();
      return false;
    }

    Menu::InitImGuiCommon(desc.OutputWindow);
    if (!ImGui_ImplDX11_Init(g_pd3d11Device, g_pd3d11Context)) {
      ReleaseDX11Objects();
      return false;
    }

    g_initializedDX11 = true;
    return true;
  }

  static void CleanupRenderTargetDX12()
  {
    for (UINT i = 0; i < g_NUM_FRAMES_IN_FLIGHT; i++) {
      if (g_mainRenderTargetResource[i]) {
        g_mainRenderTargetResource[i]->Release();
        g_mainRenderTargetResource[i] = nullptr;
      }
    }
  }

  static void CreateRenderTargetDX12(IDXGISwapChain* pSwapChain, ID3D12Device* pDevice)
  {
    for (UINT i = 0; i < g_NUM_FRAMES_IN_FLIGHT; i++) {
      ID3D12Resource* pBackBuffer = nullptr;
      if (SUCCEEDED(pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&pBackBuffer)))) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr +=
          static_cast<SIZE_T>(i) * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        pDevice->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
        g_mainRenderTargetResource[i] = pBackBuffer;
      }
    }
  }

  static void RenderUICommon(IDXGISwapChain* pSwapChain)
  {
    Menu::Render();

    if (g_overlayCallback) {
      float    screenW = 0.0f;
      float    screenH = 0.0f;
      ImGuiIO& io      = ImGui::GetIO();
      if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f) {
        screenW = io.DisplaySize.x;
        screenH = io.DisplaySize.y;
      }
      else {
        DXGI_SWAP_CHAIN_DESC desc;
        if (GetSwapChainDesc(pSwapChain, desc)) {
          screenW = static_cast<float>(desc.BufferDesc.Width);
          screenH = static_cast<float>(desc.BufferDesc.Height);
        }
      }
      g_overlayCallback(ImGui::GetBackgroundDrawList(), screenW, screenH);
    }
  }

  static void RenderDX11(IDXGISwapChain* pSwapChain)
  {
    if (!g_initializedDX11) {
      InitDX11(pSwapChain);
    }

    if (g_initializedDX11 && g_pd3d11Context && g_pd3d11RenderTarget) {
      ImGui_ImplDX11_NewFrame();
      ImGui_ImplWin32_NewFrame();
      ImGui::NewFrame();

      RenderUICommon(pSwapChain);

      ImGui::Render();
      g_pd3d11Context->OMSetRenderTargets(1, &g_pd3d11RenderTarget, nullptr);
      ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
  }

  static void RenderDX12(IDXGISwapChain* pSwapChain, ID3D12Device* pDevice)
  {
    if (!g_pd3dCommandQueue)
      return;

    // Step 1: Initialize Direct3D 12 ImGui backend on first frame
    if (!g_initializedDX12) {
      g_pd3dDevice = pDevice;

      DXGI_SWAP_CHAIN_DESC sd;
      GetSwapChainDesc(pSwapChain, sd);

      g_NUM_FRAMES_IN_FLIGHT = sd.BufferCount;
      if (g_NUM_FRAMES_IN_FLIGHT > 16)
        g_NUM_FRAMES_IN_FLIGHT = 16;
      if (g_NUM_FRAMES_IN_FLIGHT < 1)
        g_NUM_FRAMES_IN_FLIGHT = 1;

      D3D12_DESCRIPTOR_HEAP_DESC desc = {};
      desc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      desc.NumDescriptors             = g_NUM_FRAMES_IN_FLIGHT;
      desc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
      desc.NodeMask                   = 1;
      if (FAILED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap))))
        return;

      D3D12_DESCRIPTOR_HEAP_DESC srvdesc = {};
      srvdesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      srvdesc.NumDescriptors             = 1;
      srvdesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      if (FAILED(pDevice->CreateDescriptorHeap(&srvdesc, IID_PPV_ARGS(&g_pd3dSrvDescHeap))))
        return;

      for (UINT i = 0; i < g_NUM_FRAMES_IN_FLIGHT; i++) {
        if (
          FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocators[i])))
        )
          return;
      }

      if (
        FAILED(pDevice->CreateCommandList(
          0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocators[0], nullptr, IID_PPV_ARGS(&g_pd3dCommandList)
        ))
      )
        return;
      g_pd3dCommandList->Close();

      CreateRenderTargetDX12(pSwapChain, pDevice);
      Menu::InitImGuiCommon(sd.OutputWindow);

      ImGui_ImplDX12_InitInfo info      = {};
      info.Device                       = pDevice;
      info.CommandQueue                 = g_pd3dCommandQueue;
      info.NumFramesInFlight            = static_cast<int>(g_NUM_FRAMES_IN_FLIGHT);
      info.RTVFormat                    = sd.BufferDesc.Format;
      info.DSVFormat                    = DXGI_FORMAT_UNKNOWN;
      info.SrvDescriptorHeap            = g_pd3dSrvDescHeap;
      info.LegacySingleSrvCpuDescriptor = g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
      info.LegacySingleSrvGpuDescriptor = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
      if (!ImGui_ImplDX12_Init(&info))
        return;

      g_initializedDX12 = true;
    }

    if (!g_initializedDX12 || !g_pd3dCommandQueue || !g_pd3dCommandList)
      return;

    // Step 2: Begin new ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Step 3: Draw UI Menu and Overlay
    RenderUICommon(pSwapChain);

    ImGui::Render();

    // Step 4: Retrieve active back buffer index
    IDXGISwapChain3* pSwapChain3   = nullptr;
    UINT             backBufferIdx = 0;
    if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&pSwapChain3)))) {
      backBufferIdx = pSwapChain3->GetCurrentBackBufferIndex();
      pSwapChain3->Release();
    }

    // Step 5: Reset command allocator and command list for current frame
    ID3D12CommandAllocator* commandAllocator = g_commandAllocators[backBufferIdx];
    commandAllocator->Reset();
    g_pd3dCommandList->Reset(commandAllocator, nullptr);

    // Step 6: Transition back buffer from PRESENT state to RENDER_TARGET state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = g_mainRenderTargetResource[backBufferIdx];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_pd3dCommandList->ResourceBarrier(1, &barrier);

    // Step 7: Set render target and bind SRV descriptor heap for ImGui textures
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr +=
      static_cast<SIZE_T>(backBufferIdx) * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_pd3dCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);

    // Step 8: Record ImGui draw commands into D3D12 command list
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);

    // Step 9: Transition back buffer back from RENDER_TARGET state to PRESENT state
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    g_pd3dCommandList->ResourceBarrier(1, &barrier);
    g_pd3dCommandList->Close();

    // Step 10: Submit recorded command list to Direct3D 12 command queue
    ID3D12CommandList* ppCommandLists[] = {g_pd3dCommandList};
    g_pd3dCommandQueue->ExecuteCommandLists(1, ppCommandLists);
  }

  static void RenderFrame(IDXGISwapChain* pSwapChain)
  {
    // Check if swapchain belongs to DX11
    ID3D11Device* pD11 = nullptr;
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pD11)))) {
      RenderDX11(pSwapChain);
      pD11->Release();
      return;
    }

    // Check if swapchain belongs to DX12 (e.g. Proton/VKD3D or Windows native)
    ID3D12Device* pD12 = nullptr;
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&pD12)))) {
      RenderDX12(pSwapChain, pD12);
      pD12->Release();
      return;
    }

    // Windows native DXGI: Swapchain device can be ID3D12CommandQueue
    ID3D12CommandQueue* pD12CommandQueue = nullptr;
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&pD12CommandQueue)))) {
      if (SUCCEEDED(pD12CommandQueue->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&pD12)))) {
        g_pd3dCommandQueue = pD12CommandQueue;
        RenderDX12(pSwapChain, pD12);
        pD12->Release();
      }
      pD12CommandQueue->Release();
    }
  }

  HRESULT WINAPI hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
  {
    if (!g_shuttingDown && (Flags & DXGI_PRESENT_TEST) == 0) {
      RenderFrame(pSwapChain);
    }
    return oPresent(pSwapChain, SyncInterval, Flags);
  }

  HRESULT WINAPI hkPresent1(
    IDXGISwapChain1* pSwapChain, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters
  )
  {
    if (!g_shuttingDown && (PresentFlags & DXGI_PRESENT_TEST) == 0) {
      RenderFrame(pSwapChain);
    }
    return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
  }

  HRESULT WINAPI hkResizeBuffers(
    IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags
  )
  {
    if (g_shuttingDown) {
      return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    CleanupRenderTargetDX11();
    if (g_initializedDX12) {
      CleanupRenderTargetDX12();
    }

    HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (g_pd3d11Device) {
      CreateRenderTargetDX11(pSwapChain);
    }
    if (g_initializedDX12 && g_pd3dDevice) {
      CreateRenderTargetDX12(pSwapChain, g_pd3dDevice);
    }

    return hr;
  }

  void WINAPI
  hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
  {
    if (!g_pd3dCommandQueue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
      g_pd3dCommandQueue = queue;
    }
    oExecuteCommandLists(queue, NumCommandLists, ppCommandLists);
  }

  static void CleanupDX11()
  {
    if (g_initializedDX11) {
      ImGui_ImplDX11_Shutdown();
      g_initializedDX11 = false;
    }
    ReleaseDX11Objects();
  }

  static void CleanupDX12()
  {
    if (g_initializedDX12) {
      ImGui_ImplDX12_Shutdown();
      g_initializedDX12 = false;
    }
    CleanupRenderTargetDX12();
    for (UINT i = 0; i < g_NUM_FRAMES_IN_FLIGHT; i++) {
      if (g_commandAllocators[i]) {
        g_commandAllocators[i]->Release();
        g_commandAllocators[i] = nullptr;
      }
    }
    if (g_pd3dCommandList) {
      g_pd3dCommandList->Release();
      g_pd3dCommandList = nullptr;
    }
    if (g_pd3dRtvDescHeap) {
      g_pd3dRtvDescHeap->Release();
      g_pd3dRtvDescHeap = nullptr;
    }
    if (g_pd3dSrvDescHeap) {
      g_pd3dSrvDescHeap->Release();
      g_pd3dSrvDescHeap = nullptr;
    }
  }

  void Shutdown()
  {
    g_shuttingDown = true;
    CleanupDX11();
    CleanupDX12();
  }

  bool IsHookInstalled() { return hookInstalled; }

  void SetOverlayCallback(OverlayCallback callback) { g_overlayCallback = callback; }

  bool Init()
  {
    Logger::Log("[RENDERER] Initializing hooks for DX11 and DX12...");

    // Step 1: Create a temporary hidden dummy window required for DXGI swapchain initialization
    WNDCLASSEXA wc = {sizeof(WNDCLASSEXA),
                      CS_CLASSDC,
                      DefWindowProcA,
                      0L,
                      0L,
                      GetModuleHandleA(nullptr),
                      nullptr,
                      nullptr,
                      nullptr,
                      nullptr,
                      "PG2_Dummy",
                      nullptr};
    RegisterClassExA(&wc);
    HWND dummyWindow =
      CreateWindowA("PG2_Dummy", "", WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, nullptr, nullptr, wc.hInstance, nullptr);

    // Step 2: Configure swap chain descriptor for dummy device creation
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount                       = 1;
    sd.BufferDesc.Format                 = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage                       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                      = dummyWindow;
    sd.SampleDesc.Count                  = 1;
    sd.Windowed                          = TRUE;
    sd.SwapEffect                        = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL    featureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    IDXGISwapChain*      pDummySwapChain = nullptr;
    ID3D11Device*        pDummyDevice    = nullptr;
    ID3D11DeviceContext* pDummyContext   = nullptr;

    // Step 3: Attempt hardware D3D11 device and swap chain creation
    HRESULT              hr              = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 3, D3D11_SDK_VERSION, &sd, &pDummySwapChain,
      &pDummyDevice, nullptr, &pDummyContext
    );

    // Fall back to WARP software rasterizer if hardware initialization fails
    if (FAILED(hr)) {
      hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, featureLevels, 3, D3D11_SDK_VERSION, &sd, &pDummySwapChain,
        &pDummyDevice, nullptr, &pDummyContext
      );
    }

    // Step 4: Extract DXGI swap chain vtable methods and install MinHook detours
    if (SUCCEEDED(hr) && pDummySwapChain) {
      void** pVTable  = *reinterpret_cast<void***>(pDummySwapChain);
      void*  pPresent = pVTable[8];

      MH_CreateHook(pPresent, reinterpret_cast<void*>(hkPresent), reinterpret_cast<void**>(&oPresent));

      IDXGISwapChain1* pSwapChain1 = nullptr;
      if (
        SUCCEEDED(pDummySwapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&pSwapChain1)))
      ) {
        void** pVTable1  = *reinterpret_cast<void***>(pSwapChain1);
        void*  pPresent1 = pVTable1[22];
        MH_CreateHook(pPresent1, reinterpret_cast<void*>(hkPresent1), reinterpret_cast<void**>(&oPresent1));
        pSwapChain1->Release();
      }

      void* pResizeBuffers = pVTable[13];
      MH_CreateHook(
        pResizeBuffers, reinterpret_cast<void*>(hkResizeBuffers), reinterpret_cast<void**>(&oResizeBuffers)
      );

      pDummySwapChain->Release();
      pDummyDevice->Release();
      pDummyContext->Release();
    }

    // Step 5: Dummy DX12 device creation to resolve vtable addresses for ExecuteCommandLists and DX12 Present
    ID3D12Device* pDummyD12Device = nullptr;
    if (
      SUCCEEDED(D3D12CreateDevice(
        nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&pDummyD12Device)
      ))
    ) {
      D3D12_COMMAND_QUEUE_DESC queueDesc = {};
      queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
      queueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
      queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
      queueDesc.NodeMask                 = 0;

      ID3D12CommandQueue* pDummyQueue    = nullptr;
      HRESULT             hrQueue        = pDummyD12Device->CreateCommandQueue(
        &queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&pDummyQueue)
      );
      if (SUCCEEDED(hrQueue)) {
        void** pQueueVTable         = *reinterpret_cast<void***>(pDummyQueue);
        void*  pExecuteCommandLists = pQueueVTable[10];

        MH_CreateHook(
          pExecuteCommandLists, reinterpret_cast<void*>(hkExecuteCommandLists),
          reinterpret_cast<void**>(&oExecuteCommandLists)
        );

        IDXGIFactory4* pFactory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&pFactory)))) {
          DXGI_SWAP_CHAIN_DESC1 sd1      = {};
          sd1.BufferCount                = 2;
          sd1.Width                      = 100;
          sd1.Height                     = 100;
          sd1.Format                     = DXGI_FORMAT_R8G8B8A8_UNORM;
          sd1.BufferUsage                = DXGI_USAGE_RENDER_TARGET_OUTPUT;
          sd1.SwapEffect                 = DXGI_SWAP_EFFECT_FLIP_DISCARD;
          sd1.SampleDesc.Count           = 1;

          IDXGISwapChain1* pD12SwapChain = nullptr;
          HRESULT          hrSwap =
            pFactory->CreateSwapChainForHwnd(pDummyQueue, dummyWindow, &sd1, nullptr, nullptr, &pD12SwapChain);
          if (SUCCEEDED(hrSwap)) {
            void** pVTable12        = *reinterpret_cast<void***>(pD12SwapChain);
            void*  pPresent12       = pVTable12[8];
            void*  pPresent1_12     = pVTable12[22];
            void*  pResizeBuffers12 = pVTable12[13];

            MH_CreateHook(pPresent12, reinterpret_cast<void*>(hkPresent), reinterpret_cast<void**>(&oPresent));
            MH_CreateHook(pPresent1_12, reinterpret_cast<void*>(hkPresent1), reinterpret_cast<void**>(&oPresent1));
            MH_CreateHook(
              pResizeBuffers12, reinterpret_cast<void*>(hkResizeBuffers), reinterpret_cast<void**>(&oResizeBuffers)
            );
            pD12SwapChain->Release();
          }
          pFactory->Release();
        }

        pDummyQueue->Release();
      }
      pDummyD12Device->Release();
    }

    DestroyWindow(dummyWindow);
    UnregisterClassA("PG2_Dummy", wc.hInstance);

    hookInstalled = (oPresent != nullptr);
    Logger::Log(
      "[RENDERER] Renderer hooks registered: %s (Present: %p, Present1: %p, ExecuteCommandLists: %p)",
      hookInstalled ? "YES" : "NO", oPresent, oPresent1, oExecuteCommandLists
    );
    return hookInstalled;
  }
}  // namespace RendererHook
