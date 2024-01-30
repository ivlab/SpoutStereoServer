//
// Game.cpp
//

/* TODO
* build release and install
* test sync and performance with Unity apps
* 
* ----
* use as a minvr event provider: trap/send mouse and kbd events
*/

#include <windows.h>
#include <stdio.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <codecvt>
#include <locale>

#include "pch.h"
#include "Game.h"
#include "Generated Files/PixelShader.h"
#include "Generated Files/VertexShader.h"

#include <minvr3.h>

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

void get_command_line_args(int* argc, char*** argv)
{
    // Get the command line arguments as wchar_t strings
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), argc);
    if (!wargv) { *argc = 0; *argv = NULL; return; }

    // Count the number of bytes necessary to store the UTF-8 versions of those strings
    int n = 0;
    for (int i = 0; i < *argc; i++)
        n += WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL) + 1;

    // Allocate the argv[] array + all the UTF-8 strings
    *argv = (char**)malloc((*argc + 1) * sizeof(char*) + n);
    if (!*argv) { *argc = 0; return; }

    // Convert all wargv[] --> argv[]
    char* arg = (char*)&((*argv)[*argc + 1]);
    for (int i = 0; i < *argc; i++)
    {
        (*argv)[i] = arg;
        arg += WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, arg, n, NULL, NULL) + 1;
    }
    (*argv)[*argc] = NULL;
}

void helpAndExit(int exitcode) {
    std::cout << "Command line format: [-f filename [-f filename...]] [-c key=value [-c key=value...]]" << std::endl;
    std::cout << std::endl;
    std::cout << "All parameters are optional and can be specified multiple times:" << std::endl;
    std::cout << "   -f filename      Specifies a key=value config file to process." << std::endl;
    std::cout << "   -c key=value     Specifies a config key=value pair to process." << std::endl;
    std::cout << "   -h               Show this help message." << std::endl;
    exit(exitcode);
}

Game::Game() noexcept :
    m_window(nullptr),
    m_xPos(0),
    m_yPos(0),
    m_outputWidth(1280),
    m_outputHeight(1280),
    m_featureLevel(D3D_FEATURE_LEVEL_9_1),
    m_defaultTextureLeft(NULL),
    m_defaultTextureRight(NULL),
    m_textureViewLeft(NULL),
    m_textureViewRight(NULL),
    m_samplerState(NULL),
    m_pixelShader(NULL),
    m_vertexShader(NULL)
{

    // Get command line args in the silly Windows way:
    int     argc;
    char** argv;
    get_command_line_args(&argc, &argv);

    // Parse the command line using the typical formatting used for MinVR ConfigVals
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if ((arg == "-f") || (arg == "--configfile")) {
            if (argc > i + 1) {
                std::string filename(argv[i + 1]);
                ConfigVal::ParseConfigFile(filename);
                i++;
            }
            else {
                std::cerr << "Command line error: missing config file name." << std::endl;
                helpAndExit(1);
            }
        }
        else if ((arg == "-c") || (arg == "--configval")) {
            if (argc > i + 1) {
                std::string keyValPair(argv[i + 1]);
                int equalsPos = keyValPair.find("=");
                if (equalsPos != std::string::npos) {
                    std::string key = keyValPair.substr(0, equalsPos);
                    std::string val = keyValPair.substr(equalsPos + 1);
                    ConfigVal::AddOrReplace(key, val);
                    i++;
                }
                else {
                    std::cerr << "Command line error: badly formed key=value pair." << std::endl;
                    helpAndExit(1);
                }
            }
            else {
                std::cerr << "Command line error: missing key=value pair." << std::endl;
                helpAndExit(1);
            }
        }
        else if ((arg == "-h") || (arg == "--help")) {
            helpAndExit(0);
        }
        else {
            std::cout << "Warning: Did not recognize command line argument: '" << arg << "'" << std::endl;
        }
    }

    free(argv);

    // Proceed to initialize variables using configvals
    m_xPos = ConfigVal::Get("WINDOW_X", 0);
    m_yPos = ConfigVal::Get("WINDOW_Y", 0);
    m_outputWidth = ConfigVal::Get("WINDOW_WIDTH", 1280);
    m_outputHeight = ConfigVal::Get("WINDOW_HEIGHT", 1280);

    std::string leftSender = ConfigVal::Get("SPOUT_SENDER_NAME_LEFT", std::string("Left Eye"));
    m_receiverLeft.SetReceiverName(leftSender.c_str());
    std::string rightSender = ConfigVal::Get("SPOUT_SENDER_NAME_RIGHT", std::string("Right Eye"));
    m_receiverRight.SetReceiverName(rightSender.c_str());
}

Game::~Game() {

}

void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = ConfigVal::Get("WINDOW_WIDTH", 1280);
    height = ConfigVal::Get("WINDOW_HEIGHT", 1280);
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window)
{
    m_window = window;

    CreateDevice();
    CreateOrUpdateWindowSpecificResources();

    // Get the current window style
    LONG style = GetWindowLong(m_window, GWL_STYLE);

    if (!ConfigVal::Get("WINDOW_TITLEBAR_VISIBLE", true)) {
        // Remove the caption and sizebox
        SetWindowLong(m_window, GWL_STYLE, (LONG)(style & ~(WS_CAPTION | WS_SIZEBOX)));
    }

    // Set the position, size, and make the window render above the toolbar
    SetWindowPos(m_window, HWND_TOPMOST, m_xPos, m_yPos, m_outputWidth, m_outputHeight, SWP_SHOWWINDOW);


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

    // Create left and right default textures to use with testing and/or when no 
    // spout connection is present
    int texWidth = m_outputWidth;
    int texHeight = m_outputHeight;
    int texNumChannels = 4;
    int texNumPixels = texWidth * texHeight;
    int texNumBytes = texNumPixels * texNumChannels;
    int texBytesPerRow = texNumChannels * texWidth;

    unsigned char* texBytesLeft = new unsigned char[texNumBytes];
    unsigned char* texBytesRight = new unsigned char[texNumBytes];
    for (int i = 0; i < texNumPixels; i++) {
        float a = (float)i / (float)(texNumPixels - 1);

        // left eye shows a gray to red gradient top to bottom
        texBytesLeft[i * texNumChannels + 0] = (unsigned char)128 + (unsigned char)std::roundf(127.0f * a);  // red
        texBytesLeft[i * texNumChannels + 1] = (unsigned char)128;  // green
        texBytesLeft[i * texNumChannels + 2] = (unsigned char)128;  // blue
        texBytesLeft[i * texNumChannels + 3] = (unsigned char)255;  // alpha

        // right eye shows a blue to gray gradient top to bottom
        texBytesRight[i * texNumChannels + 0] = (unsigned char)128;  // red
        texBytesRight[i * texNumChannels + 1] = (unsigned char)128;  // green
        texBytesRight[i * texNumChannels + 2] = (unsigned char)128 + (unsigned char)std::roundf(127.0f * (1.0f - a));  // blue
        texBytesRight[i * texNumChannels + 3] = (unsigned char)255;  // alpha
    }

    // the same desc works for both left and right
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = texWidth;
    textureDesc.Height = texHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA textureSubresourceDataLeft = {};
    textureSubresourceDataLeft.pSysMem = texBytesLeft;
    textureSubresourceDataLeft.SysMemPitch = texBytesPerRow;

    D3D11_SUBRESOURCE_DATA textureSubresourceDataRight = {};
    textureSubresourceDataRight.pSysMem = texBytesRight;
    textureSubresourceDataRight.SysMemPitch = texBytesPerRow;

    DX::ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&textureDesc, &textureSubresourceDataLeft, &m_defaultTextureLeft)
    );
    if (m_defaultTextureLeft != NULL) {
        DX::ThrowIfFailed(
            m_d3dDevice->CreateShaderResourceView(m_defaultTextureLeft, nullptr, &m_textureViewLeft)
        );
    }

    DX::ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&textureDesc, &textureSubresourceDataRight, &m_defaultTextureRight)
    );
    if (m_defaultTextureRight != NULL) {
        DX::ThrowIfFailed(
            m_d3dDevice->CreateShaderResourceView(m_defaultTextureRight, nullptr, &m_textureViewRight)
        );
    }

    delete[] texBytesLeft;
    delete[] texBytesRight;


    // Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

// These are the resources that depend on the device.
void Game::CreateDevice()
{
    // The Spout library can either create the DirectX device or it can use one that
    // the application creates -- there seem to be some advantages to allowing Spout
    // to create the device.
    bool haveSpoutCreateDevice = true;
    if (haveSpoutCreateDevice) {
        // Have Spout create the device
        if (m_receiverLeft.OpenDirectX11()) {
            m_d3dDevice = m_receiverLeft.GetDX11Device();
            m_d3dContext = m_receiverLeft.GetDX11Context();
        }
        else {
            DX::ThrowIfFailed(0);
        }
        // Tell the second receiver about the device
        if (!m_receiverRight.OpenDirectX11(m_d3dDevice.Get())) {
            DX::ThrowIfFailed(0);
        }
    }
    else {
        // Have the application create the device
        UINT creationFlags = 0;

#ifdef _DEBUG
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        static const D3D_FEATURE_LEVEL featureLevels[] =
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
                D3D11_MESSAGE_ID hide[] =
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

        // Tell Spout about the device the Application created
        if (!m_receiverLeft.OpenDirectX11(m_d3dDevice.Get())) {
            DX::ThrowIfFailed(0);
        }
        if (!m_receiverRight.OpenDirectX11(m_d3dDevice.Get())) {
            DX::ThrowIfFailed(0);
        }
    }

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

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
        swapChainDesc.Width = backBufferWidth;
        swapChainDesc.Height = backBufferHeight;
        swapChainDesc.Format = backBufferFormat;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = backBufferCount;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Scaling = DXGI_SCALING_NONE; // ASPECT_RATIO_STRETCH;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Stereo = TRUE;
        swapChainDesc.Flags = 0;

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
    ResetDevice();
}

void Game::ResetDevice()
{
    m_receiverLeft.ReleaseReceiver();
    m_receiverRight.ReleaseReceiver();
    m_receiverLeft.CloseDirectX11();

    // TODO: Add Direct3D resource cleanup here.
    m_receivedTextureViewLeft->Release();
    m_receivedTextureViewRight->Release();
    m_receivedTextureLeft->Release();
    m_receivedTextureRight->Release();

    m_renderTargetViewLeft.Reset();
    m_renderTargetViewRight.Reset();
    m_swapChain.Reset();

    if (!m_receiverLeft.IsClassDevice()) {
        m_d3dContext.Reset();
        m_d3dDevice.Reset();
    }

    // Startup Again
    CreateDevice();
    CreateOrUpdateWindowSpecificResources();
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
    //float elapsedTime = float(timer.GetElapsedSeconds());

    // --- LEFT ---

    // Receive a new texture
    if (m_receiverLeft.ReceiveTexture()) {
        // The D3D11 device within the SpoutDX class could have changed.
        // If it has switched to use a different sender graphics adapter,
        // stop receiving the texture and re-initialize the application.
        if (m_receiverLeft.GetAdapterAuto()) {
            if (m_d3dDevice.Get() != m_receiverLeft.GetDX11Device()) {
                ResetDevice();
                return;
            }
        }
        // If the frame is new, then create/update the shader resource view
        if (m_receiverLeft.IsFrameNew()) {
            // release old view if it exists
            if (m_receivedTextureViewLeft != nullptr) {
                m_receivedTextureViewLeft->Release();
                m_receivedTextureViewLeft = nullptr;
            }
            // create new view
            D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
            ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
            // Match format of the sender
            shaderResourceViewDesc.Format = m_receiverLeft.GetSenderFormat();
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
            shaderResourceViewDesc.Texture2D.MipLevels = 1;
            m_d3dDevice->CreateShaderResourceView(m_receiverLeft.GetSenderTexture(), &shaderResourceViewDesc, &m_receivedTextureViewLeft);
        }
    }
    else {
        // A sender was not found or the connected sender closed
        // Release the texture resource view so render uses the default texture
        if (m_receivedTextureViewLeft != nullptr) {
            m_receivedTextureViewLeft->Release();
            m_receivedTextureViewLeft = nullptr;
        }
    }


    // --- RIGHT ---

    // Receive a new texture
    if (m_receiverRight.ReceiveTexture()) {
        // The D3D11 device within the SpoutDX class could have changed.
        // If it has switched to use a different sender graphics adapter,
        // stop receiving the texture and re-initialize the application.
        if (m_receiverRight.GetAdapterAuto()) {
            if (m_d3dDevice.Get() != m_receiverRight.GetDX11Device()) {
                ResetDevice();
                return;
            }
        }
        // If the frame is new, then create/update the shader resource view
        if (m_receiverRight.IsFrameNew()) {
            // release old view if it exists
            if (m_receivedTextureViewRight != nullptr) {
                m_receivedTextureViewRight->Release();
                m_receivedTextureViewRight = nullptr;
            }
            // create new view
            D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
            ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
            // Match format of the sender
            shaderResourceViewDesc.Format = m_receiverRight.GetSenderFormat();
            shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
            shaderResourceViewDesc.Texture2D.MipLevels = 1;
            m_d3dDevice->CreateShaderResourceView(m_receiverRight.GetSenderTexture(), &shaderResourceViewDesc, &m_receivedTextureViewRight);
        }
    }
    else {
        // A sender was not found or the connected sender closed
        // Release the texture resource view so render uses the default texture
        if (m_receivedTextureViewRight != nullptr) {
            m_receivedTextureViewRight->Release();
            m_receivedTextureViewRight = nullptr;
        }
    }
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

    if (m_receivedTextureViewLeft) {
        m_d3dContext->PSSetShaderResources(0, 1, &m_receivedTextureViewLeft);
    }
    else {
        m_d3dContext->PSSetShaderResources(0, 1, &m_textureViewLeft);
    }
    m_d3dContext->PSSetSamplers(0, 1, &m_samplerState);
    m_d3dContext->Draw(4, 0);
    
    // -- RIGHT EYE --
    
    m_d3dContext->OMSetRenderTargets(1, m_renderTargetViewRight.GetAddressOf(), nullptr);
    m_d3dContext->ClearRenderTargetView(m_renderTargetViewRight.Get(), Colors::Blue);

    //m_d3dContext->RSSetViewports(1, &m_viewport);
    //m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    //m_d3dContext->VSSetShader(m_vertexShader, nullptr, 0);
    //m_d3dContext->PSSetShader(m_pixelShader, nullptr, 0);
    if (m_receivedTextureViewRight) {
        m_d3dContext->PSSetShaderResources(0, 1, &m_receivedTextureViewRight);
    }
    else {
        m_d3dContext->PSSetShaderResources(0, 1, &m_textureViewRight);
    }
    //m_d3dContext->PSSetSamplers(0, 1, &m_samplerState);
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


