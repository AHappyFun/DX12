#include "stdafx.h"

using namespace DirectX;

//定义Vertex结构
struct Vertex
{
    DirectX::XMFLOAT3 pos; 
};

//初始化DX
bool InitD3D()
{
    HRESULT hr;

    //-----------Create the DXGI device---------------
    IDXGIFactory4* dxgiFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if(FAILED(hr))
    {
        return false;
    }

    IDXGIAdapter1* adapter;

    int adapterIndex = 0;

    bool adapterFound = false;

    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) 
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            adapterIndex++;
            continue;
        }

        hr = D3D12CreateDevice(
            adapter, 
            D3D_FEATURE_LEVEL_11_0, 
            _uuidof(ID3D12Device), 
            nullptr);

        if (SUCCEEDED(hr)) 
        {
            adapterFound = true;
            break;
        }

        adapterIndex++;
    }

    if (!adapterFound)
        return false;

    //--------create dx12 device-----------
    hr = D3D12CreateDevice(
        adapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device));
    if(FAILED(hr))
    {
        return false;
    }

    //-------create command queue----------
    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue));
    if(FAILED(hr))
    {
        return false;
    }

    //------create swap chain(双缓冲/三缓冲)--------
    DXGI_MODE_DESC backBufferDesc = {};
    backBufferDesc.Width = Width;
    backBufferDesc.Height = Height;
    backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    DXGI_SAMPLE_DESC sampleDesc = {};
    sampleDesc.Count = 1;

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = frameBufferCount; // number of buffers we have
    swapChainDesc.BufferDesc = backBufferDesc; // our back buffer description
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swapChainDesc.OutputWindow = hwnd; // handle to our window
    swapChainDesc.SampleDesc = sampleDesc; // our multi-sampling description
    swapChainDesc.Windowed = !FullScreen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

    IDXGISwapChain* tempSwapChain;
    dxgiFactory->CreateSwapChain(
        commandQueue,
        &swapChainDesc,
        &tempSwapChain);

    swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);
    frameIndex = swapChain->GetCurrentBackBufferIndex();

    //-------Create Back Buffers RT DescriptorHeap----------
    // describe an rtv descriptor heap and create
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = frameBufferCount; // number of descriptors for this heap.
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // this heap is a render target view heap

    // This heap will not be directly referenced by the shaders (not shader visible), as this will store the output from the pipeline
    // otherwise we would set the heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
    if (FAILED(hr))
    {
        return false;
    }

    // get the size of a descriptor in this heap (this is a rtv heap, so only rtv descriptors should be stored in it.
    // descriptor sizes may vary from device to device, which is why there is no set size and we must ask the 
    // device to give us the size. we will use this size to increment a descriptor handle offset
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
    // but we cannot literally use it like a c++ pointer.
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a RTV for each buffer (double buffering is two buffers, tripple buffering is 3).
    for (int i = 0; i < frameBufferCount; i++)
    {
        // first we get the n'th buffer in the swap chain and store it in the n'th
        // position of our ID3D12Resource array
        hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
        if (FAILED(hr))
        {
            return false;
        }

        // the we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
        device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);

        // we increment the rtv handle by the rtv descriptor size we got above
        rtvHandle.Offset(1, rtvDescriptorSize);
    }

    //-----------Create Command Allocators----------
    for (int i = 0; i < frameBufferCount; i++)
    {
        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
        if (FAILED(hr))
        {
            return false;
        }
    }

    //--------Create Command List--------------
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[0], NULL, IID_PPV_ARGS(&commandList));
    if (FAILED(hr))
    {
        return false;
    }
    //main loop 里会设置，这里close
    commandList->Close();

    //--------Create Fence & Fence Event-----------
    for (int i = 0; i < frameBufferCount; i++)
    {
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
        if (FAILED(hr))
        {
            return false;
        }
        fenceValue[i] = 0; // set the initial fence value to 0
    }

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        return false;
    }

    //---------Create Root Signature-----------
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ID3DBlob* signature;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
    if(FAILED(hr))
    {
        return false;
    }

    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if(FAILED(hr))
    {
        return false;
    }

    //--------------Create vert and pixel shader-----------
    //debug的时候，可以实时编译shader文件
    
    //编译VertShader
    ID3DBlob* vertShader;
    ID3DBlob* errorBuff;
    hr = D3DCompileFromFile(L"VertexShader.hlsl",
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vertShader,
        &errorBuff);
    if(FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
        return false;
    }

    D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
    vertexShaderBytecode.BytecodeLength = vertShader->GetBufferSize();
    vertexShaderBytecode.pShaderBytecode = vertShader->GetBufferPointer();

    //编译PixelShader
    ID3DBlob* pixelShader;
    hr = D3DCompileFromFile(L"PixelShader.hlsl",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &pixelShader,
        &errorBuff);
    if(FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
        return false;
    }

    D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
    pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
    pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

    //----------Create Input Layout----------------
    D3D12_INPUT_ELEMENT_DESC  inputLayout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
    inputLayoutDesc.pInputElementDescs = inputLayout;

    //------------Create PSO------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = inputLayoutDesc;
    psoDesc.pRootSignature = rootSignature;
    psoDesc.VS = vertexShaderBytecode;
    psoDesc.PS = pixelShaderBytecode;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc = sampleDesc;
    psoDesc.SampleMask = 0xfffffff;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.NumRenderTargets = 1;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if(FAILED(hr))
    {
        return false;
    }

    //------------Create VertexBuffer-------------
    Vertex vList[] = {
        { {0.0f, 0.5f, 0.5f} },
        { {0.5f, -0.5f, 0.5f} },
        { {-0.5f, -0.5f, 0.5f} }
    };

    int vBufferSize = sizeof(vList);

    //-----------Create Default heap-----------
    CD3DX12_HEAP_PROPERTIES Heap_p = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resource_des = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
    device->CreateCommittedResource(
        &Heap_p,
        D3D12_HEAP_FLAG_NONE,
        &resource_des,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer));

    vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

    //-----------Create Upload Heap-----------
    ID3D12Resource* vBufferUploadHeap;
    CD3DX12_HEAP_PROPERTIES Heap_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resource_des2 = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
    device->CreateCommittedResource(
        &Heap_upload, // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &resource_des2,
        D3D12_RESOURCE_STATE_GENERIC_READ,  //gpu从这里读取并复制到默认堆
        nullptr,
        IID_PPV_ARGS(&vBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(vList);
    vertexData.RowPitch = vBufferSize;
    vertexData.SlicePitch = vBufferSize;

    UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);
    CD3DX12_RESOURCE_BARRIER resource_barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    commandList->ResourceBarrier(1, &resource_barrier);

    //执行Cmdlist
    commandList->Close();
    ID3D12CommandList* ppCmdLists[] = {commandList};
    commandQueue->ExecuteCommandLists(_countof(ppCmdLists), ppCmdLists);

    fenceValue[frameIndex]++;
    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if(FAILED(hr))
    {
        Running = false;
    }

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vBufferSize;

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = Width;
    viewport.Height = Height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = Width;
    scissorRect.bottom = Height;
    
    return true;
}

//更新逻辑
void UpdateLogic()
{
    
}

//向Cmdlist添加命令/record command
void UpdatePipeline()
{
    HRESULT hr;
    //重置命令分配器之前，需要确保GPU已经完成和命令分配器相关的Cmdlist
    //检查栅栏的值,如果增加了说明已经执行完
    WaitForPreviousFrame();

    hr = commandAllocator[frameIndex]->Reset();
    if(FAILED(hr))
    {
        Running = false;
    }

    hr = commandList->Reset(commandAllocator[frameIndex], NULL);
    if(FAILED(hr))
    {
        Running = false;
    }

    //记录command 到commandlist
    //ResourceBarrier是管理资源访问模式的转换
    CD3DX12_RESOURCE_BARRIER res_barrier_rt = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &res_barrier_rt);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    CD3DX12_RESOURCE_BARRIER res_barrier_present = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &res_barrier_present);

    hr = commandList->Close();
    if(FAILED(hr))
    {
        Running = false;
    }
    
}

void Render()
{
    HRESULT hr;

    UpdatePipeline();

    ID3D12CommandList* ppCommandList[] = { commandList };

    //执行cmdlist
    commandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);

    //设置栅栏
    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if(FAILED(hr))
    {
        Running = false;
    }

    //present current backbuffer/提交当前backbuffer
    hr = swapChain->Present(0, 0);
    if(FAILED(hr))
    {
        Running = false;
    }
}

//释放和清理
void CleanUp()
{
    for (int i = 0; i < frameBufferCount ;i++)
    {
        frameIndex = i;
        WaitForPreviousFrame();
    }

    BOOL fs = false;
    if(swapChain->GetFullscreenState(&fs, NULL))
        swapChain->SetFullscreenState(false, NULL);

    SAFE_RELEASE(device);
    SAFE_RELEASE(swapChain);
    SAFE_RELEASE(commandQueue);
    SAFE_RELEASE(rtvDescriptorHeap);
    SAFE_RELEASE(commandList);

    for(int i = 0; i < frameBufferCount; i++)
    {
        SAFE_RELEASE(renderTargets[i]);
        SAFE_RELEASE(commandAllocator[i]);
        SAFE_RELEASE(fence[i]);
    }

    SAFE_RELEASE(pso);
    SAFE_RELEASE(rootSignature);
    SAFE_RELEASE(vertexBuffer);
    
}

//等待上一帧完成.检查栅栏
void WaitForPreviousFrame()
{
    HRESULT hr;

    frameIndex = swapChain->GetCurrentBackBufferIndex();

    //如果fence的值小于设置的FenceValue，就说明GPU还没执行完
    if(fence[frameIndex] ->GetCompletedValue() < fenceValue[frameIndex])
    {
        //设置fenceEvent，完成后设置对应fenceValue
        hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
        if(FAILED(hr))
        {
            Running = false;
        }

        WaitForSingleObject(fenceEvent, INFINITE);
    }

    //fence值自增
    fenceValue[frameIndex]++;
    
}

bool InitializeWindow(HINSTANCE hInstance,
    int ShowWnd,
    int width, int height,
    bool fullscreen)
{
    if (fullscreen)
    {
        HMONITOR hmon = MonitorFromWindow(hwnd,
            MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hmon, &mi);

        width = mi.rcMonitor.right - mi.rcMonitor.left;
        height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }

    WNDCLASSEX wc;
    
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WindowName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, L"Error registering class",
            L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    hwnd = CreateWindowEx(NULL,
        WindowName,
        WindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        Width, Height,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!hwnd)
    {
        MessageBox(NULL, L"Error creating window",
            L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    if (fullscreen)
    {
        SetWindowLong(hwnd, GWL_STYLE, 0);
    }

    ShowWindow(hwnd, ShowWnd);
    UpdateWindow(hwnd);
    
    return true;
}

void mainLoop()
{
    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));
    
    while (Running)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;
    
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // run game code
            UpdateLogic();
            Render();
        }
    }
}


LRESULT CALLBACK WndProc(HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (MessageBox(0, L"Are you sure you want to exit?",
                L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
                    DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        Running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd,
        msg,
        wParam,
        lParam);
}

int WINAPI WinMain(HINSTANCE hInstance,    //Main windows function
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nShowCmd)
{
    // create the window
    if (!InitializeWindow(hInstance, nShowCmd, Width, Height, FullScreen))
    {
        MessageBox(0, L"Window InitWindow - Failed",
            L"Error", MB_OK);
        return 0;
    }

    //init D3D
    if (!InitD3D())
    {
        MessageBox(0, L"Init DX12 - Failed",
            L"Error", MB_OK);
        CleanUp();
        return 1;
    }

    // start the main loop
    mainLoop();

    //wait gpu execute command list finish
    WaitForPreviousFrame();

    //删除fenceEvent
    CloseHandle(fenceEvent);

    //释放资源
    CleanUp();

    return 0;

}

