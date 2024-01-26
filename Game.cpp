//
// Game.cpp
//

/*
* convert to d3d win32 game template (not uwp) to support win32 calls used by spout
* link with spout
* borderless window
* set pos and size
* maybe text for debug output
* trap/send mouse and kbd events
*/

#include "pch.h"
#include "Game.h"

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept :
    m_window(nullptr),
    m_outputWidth(800),
    m_outputHeight(600),
    m_featureLevel(D3D_FEATURE_LEVEL_9_1)
{
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    m_window = window;
    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);

    CreateDevice();

    CreateOrUpdateWindowSpecificResources();


    // Create simple shaders for fullscreen quad (these are compiled into header files during the build)
    // Right-click on the .hlsl files and go to Properties to configure this.
    DX::ThrowIfFailed(
        m_d3dDevice->CreateVertexShader(g_vertexshader, sizeof(g_vertexshader), nullptr, &m_vertexShader)
    );
    DX::ThrowIfFailed(
        m_d3dDevice->CreatePixelShader(g_pixelshader, sizeof(g_pixelshader), nullptr, &m_pixelShader)
    );        

    // Create Sampler State (Texturing Settings)
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    DX::ThrowIfFailed(
        m_d3dDevice->CreateSamplerState(&samplerDesc, &m_samplerState)
    );

    // Create a texture by hand (TODO: Replace with Spout connection)
    int texWidth = m_outputWidth;
    int texHeight = m_outputHeight;
    int texNumChannels = 4;
    int texNumPixels = texWidth * texHeight;
    int texBytesPerRow = texNumChannels * texWidth;
    
    unsigned char* texBytes = new unsigned char[texNumPixels * texNumChannels];
    for (int i = 0; i < texNumPixels; i++) {
        texBytes[i * texNumChannels + 0] = 255 * (i % 2);  // red
        texBytes[i * texNumChannels + 1] = 255 * (i % 2);  // green
        texBytes[i * texNumChannels + 2] = 255 * (i % 2);  // blue
        texBytes[i * texNumChannels + 3] = 255;  // alpha
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = texWidth;
    textureDesc.Height = texHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureSubresourceData = {};
    textureSubresourceData.pSysMem = texBytes;
    textureSubresourceData.SysMemPitch = texBytesPerRow;

    DX::ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&textureDesc, &textureSubresourceData, &m_textureLeft)
    );
    DX::ThrowIfFailed(
        m_d3dDevice->CreateShaderResourceView(m_textureLeft, nullptr, &m_textureViewLeft)
    );

    delete[] texBytes;


    // Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;
}

// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // -- LEFT EYE --
    m_d3dContext->OMSetRenderTargets(1, m_renderTargetViewLeft.GetAddressOf(), nullptr);
    m_d3dContext->ClearRenderTargetView(m_renderTargetViewLeft.Get(), Colors::Red);

    m_d3dContext->RSSetViewports(1, &m_viewport);
    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_d3dContext->VSSetShader(m_vertexShader, nullptr, 0);
    m_d3dContext->PSSetShader(m_pixelShader, nullptr, 0);
    m_d3dContext->PSSetShaderResources(0, 1, &m_textureViewLeft);
    m_d3dContext->PSSetSamplers(0, 1, &m_samplerState);
    m_d3dContext->Draw(4, 0);
    
    // -- RIGHT EYE --
    m_d3dContext->OMSetRenderTargets(1, m_renderTargetViewLeft.GetAddressOf(), nullptr);
    m_d3dContext->ClearRenderTargetView(m_renderTargetViewLeft.Get(), Colors::Blue);

    m_d3dContext->RSSetViewports(1, &m_viewport);
    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    m_d3dContext->VSSetShader(m_vertexShader, nullptr, 0);
    m_d3dContext->PSSetShader(m_pixelShader, nullptr, 0);
    m_d3dContext->PSSetShaderResources(0, 1, &m_textureViewRight);
    m_d3dContext->PSSetSamplers(0, 1, &m_samplerState);
    m_d3dContext->Draw(4, 0);

    Present();
}

// Presents the back buffer contents to the screen.
void Game::Present()
{
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    HRESULT hr = m_swapChain->Present(1, 0);

    // If the device was reset we must completely reinitialize the renderer.
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OnDeviceLost();
    }
    else
    {
        DX::ThrowIfFailed(hr);
    }
}

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_window)
        return;

    m_outputWidth = std::max(width, 1);
    m_outputHeight = std::max(height, 1);

    CreateOrUpdateWindowSpecificResources();

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 800;
    height = 600;
}

// These are the resources that depend on the device.
void Game::CreateDevice()
{
    UINT creationFlags = 0;

#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    static const D3D_FEATURE_LEVEL featureLevels [] =
    {
        // TODO: Modify for supported Direct3D feature levels
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    // Create the DX11 API device object, and get a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    DX::ThrowIfFailed(D3D11CreateDevice(
        nullptr,                            // specify nullptr to use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        device.ReleaseAndGetAddressOf(),    // returns the Direct3D device created
        &m_featureLevel,                    // returns feature level of device created
        context.ReleaseAndGetAddressOf()    // returns the device immediate context
        ));

#ifndef NDEBUG
    ComPtr<ID3D11Debug> d3dDebug;
    if (SUCCEEDED(device.As(&d3dDebug)))
    {
        ComPtr<ID3D11InfoQueue> d3dInfoQueue;
        if (SUCCEEDED(d3dDebug.As(&d3dInfoQueue)))
        {
#ifdef _DEBUG
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif
            D3D11_MESSAGE_ID hide [] =
            {
                D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
                // TODO: Add more message IDs here as needed.
            };
            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
        }
    }
#endif

    DX::ThrowIfFailed(device.As(&m_d3dDevice));
    DX::ThrowIfFailed(context.As(&m_d3dContext));

    // TODO: Initialize device dependent objects here (independent of window size).
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateOrUpdateWindowSpecificResources()
{
    // Clear the previous window size specific context.
    m_d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetViewLeft.Reset();
    m_renderTargetViewRight.Reset();
    m_d3dContext->Flush();

    const UINT backBufferWidth = static_cast<UINT>(m_outputWidth);
    const UINT backBufferHeight = static_cast<UINT>(m_outputHeight);
    const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    constexpr UINT backBufferCount = 2;

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);

        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();

            // Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else
        {
            DX::ThrowIfFailed(hr);
        }
    }
    else
    {
        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));

        // And obtain the factory object that created it.
        ComPtr<IDXGIFactory2> dxgiFactory;
        DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

        // Create a descriptor for the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backBufferCount;
        swapChainDesc.Stereo = TRUE;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
        fsSwapChainDesc.Windowed = TRUE;

        // Create a SwapChain from a Win32 window.
        DX::ThrowIfFailed(
            dxgiFactory->CreateSwapChainForHwnd(m_d3dDevice.Get(), m_window, &swapChainDesc,
                &fsSwapChainDesc, nullptr, m_swapChain.ReleaseAndGetAddressOf())
        );

        // This template does not support exclusive fullscreen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
        DX::ThrowIfFailed(dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    ComPtr<ID3D11Texture2D> backBuffer;
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));

    // Create a view interface on the rendertarget to use on bind.
    DX::ThrowIfFailed(m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf()));

    // Create a descriptor for the left eye view.
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewLeftDesc(
        D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 0, 1
    );

    // Create a view interface on the rendertarget to use on bind for mono or left eye view.
    DX::ThrowIfFailed(
        m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), &renderTargetViewLeftDesc, m_renderTargetViewLeft.ReleaseAndGetAddressOf())
    );

    // Create a descriptor for the right eye view.
    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewRightDesc(
        D3D11_RTV_DIMENSION_TEXTURE2DARRAY, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 1, 1
    );

    // Create a view interface on the rendertarget to use on bine for right eye view.
    DX::ThrowIfFailed(
        m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), &renderTargetViewRightDesc, m_renderTargetViewRight.ReleaseAndGetAddressOf())
    );


    // Initialize window-size dependent objects here.
    m_viewport = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_outputWidth), static_cast<float>(m_outputHeight));
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.

    m_renderTargetViewLeft.Reset();
    m_renderTargetViewRight.Reset();
    m_swapChain.Reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();

    CreateDevice();

    CreateOrUpdateWindowSpecificResources();
}
