#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

//Release宏
#define SAFE_RELEASE(p) {if(p) {(p)->Release(); (p)=0; }}

// Handle to the window
HWND hwnd = NULL;

// name of the window (not the title)
LPCTSTR WindowName = L"LoyDX12App";

// title of the window
LPCTSTR WindowTitle = L"Loy DX12";

// width and height of the window
int Width = 800;
int Height = 600;

// is window full screen?
bool FullScreen = false;

bool Running = true;

// create a window
bool InitializeWindow(HINSTANCE hInstance,
    int ShowWnd,
    bool fullscreen);

// main application loop
void mainLoop();

// callback function for windows messages
LRESULT CALLBACK WndProc(HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam);

//------------DX 定义 Start----------------
//3缓冲
const int frameBufferCount = 3;

ID3D12Device* device;

IDXGISwapChain3* swapChain;

ID3D12CommandQueue* commandQueue;

ID3D12DescriptorHeap* rtvDescriptorHeap;

ID3D12Resource* renderTargets[frameBufferCount];

ID3D12CommandAllocator* commandAllocator[frameBufferCount];

ID3D12GraphicsCommandList* commandList;

ID3D12Fence* fence[frameBufferCount];

HANDLE fenceEvent;

UINT64 fenceValue[frameBufferCount];

int frameIndex;

int rtvDescriptorSize;

bool InitD3D();

void UpdateLogic();

void UpdatePipeline();

void Render();

void CleanUp();

void WaitForPreviousFrame();

ID3D12PipelineState* pso;

ID3D12RootSignature* rootSignature;

D3D12_VIEWPORT viewport;

D3D12_RECT scissorRect;

ID3D12Resource* vertexBuffer;

D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

ID3D12Resource* indexBuffer;

D3D12_INDEX_BUFFER_VIEW indexBufferView;

ID3D12Resource* depthStencilBuffer;

ID3D12DescriptorHeap* depthStencilDescHeap;

//---------------DX 定义 End-------------------------