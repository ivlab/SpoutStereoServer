//
// Game.cpp
//

/* TODO
* test fullscreen and sync and performance with Unity apps
* test minvr connection for events
* add data file search feature similar to MinGfx
* install in V
*/

#include "pch.h"

#include "Game.h"

#include <windows.h>
#include <stdio.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <codecvt>
#include <locale>


#include <minvr3.h>

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

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
    m_width(1280),
    m_height(1280),
    m_featureLevel(D3D_FEATURE_LEVEL_9_1)
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
                size_t equalsPos = keyValPair.find("=");
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
    m_width = ConfigVal::Get("WINDOW_WIDTH", 1280);
    m_height = ConfigVal::Get("WINDOW_HEIGHT", 1280);
}

Game::~Game() {

}

void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // note minimum size is 320x200
    width = ConfigVal::Get("WINDOW_WIDTH", 1280);
    height = ConfigVal::Get("WINDOW_HEIGHT", 1280);
}

void Game::GetDefaultPosition(int& x, int& y) const noexcept
{
    x = ConfigVal::Get("WINDOW_X", 0);
    y = ConfigVal::Get("WINDOW_Y", 0);
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, bool fullscreen)
{
    m_window = window;
    // The SpoutStereoWindow doesn't need to know about the fullscreen state.
    m_spoutStereoWindow.Initialize(window); // blank name because only one window

    CreateDevice();
    CreateDeviceResources();
    CreateWindowResources();

    if (fullscreen) {
        // This is already set in Main.cpp, but we can ensure it here.
        SetWindowPos(m_window, HWND_TOPMOST, m_xPos, m_yPos, m_width, m_height, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    } else if (!ConfigVal::Get("WINDOW_TITLEBAR_VISIBLE", true)) {
        // For windowed mode, optionally remove the title bar.
        LONG style = GetWindowLong(m_window, GWL_STYLE);
        SetWindowLong(m_window, GWL_STYLE, (LONG)(style & ~(WS_CAPTION | WS_SIZEBOX | WS_THICKFRAME)));
        SetWindowPos(m_window, HWND_NOTOPMOST, m_xPos, m_yPos, m_width, m_height, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    }

    // Start minimized
    ShowWindow(m_window, SW_MINIMIZE);

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
}

void Game::ReleaseDevice()
{
    // note: these are windows ComPtrs, Reset() is the call to release the ptr and decrease the ref count
    m_d3dContext.Reset();
    m_d3dDevice.Reset();
}


void Game::CreateDeviceResources()
{
    // TODO: Initialize device dependent objects here (independent of window size).
    m_spoutStereoWindow.CreateDeviceResources(m_d3dDevice, m_d3dContext);
}

void Game::ReleaseDeviceResources()
{
    m_spoutStereoWindow.ReleaseDeviceResources();
}

void Game::CreateWindowResources()
{
    // TODO: Initialize window dependent objects here

    const UINT backBufferWidth = static_cast<UINT>(m_width);
    const UINT backBufferHeight = static_cast<UINT>(m_height);
    const DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    constexpr UINT backBufferCount = 2;

    // If the swap chain already exists, resize it, otherwise create one.
    if (m_swapChain) {
        HRESULT hr = m_swapChain->ResizeBuffers(backBufferCount, backBufferWidth, backBufferHeight, backBufferFormat, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            // If the device was removed for any reason, a new device and swap chain will need to be created.
            OnDeviceLost();

            // Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method 
            // and correctly set up the new device.
            return;
        }
        else {
            DX::ThrowIfFailed(hr);
        }
    }
    else {
        // First, retrieve the underlying DXGI Device from the D3D Device.
        ComPtr<IDXGIDevice1> dxgiDevice;
        DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

        // Identify the physical adapter (GPU or card) this device is running on and get the factory.
        ComPtr<IDXGIAdapter> dxgiAdapter;
        DX::ThrowIfFailed(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));
        ComPtr<IDXGIFactory> dxgiFactory;
        DX::ThrowIfFailed(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = backBufferCount;
        swapChainDesc.BufferDesc.Width = backBufferWidth;
        swapChainDesc.BufferDesc.Height = backBufferHeight;
        swapChainDesc.BufferDesc.Format = backBufferFormat;
        // This is the critical part for windowed stereo with the bitblt model.
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 120;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = m_window;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // This flag enables stereo for this model.

        // Create a SwapChain from a Win32 window.
        DX::ThrowIfFailed(dxgiFactory->CreateSwapChain(m_d3dDevice.Get(), &swapChainDesc, m_swapChain.ReleaseAndGetAddressOf()));

        // This template does not support exclusive fullscreen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
        DX::ThrowIfFailed(dxgiFactory->MakeWindowAssociation(m_window, DXGI_MWA_NO_ALT_ENTER));
    }

    // Obtain the backbuffer for this window which will be the final 3D rendertarget.
    DX::ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(m_backBuffer.ReleaseAndGetAddressOf())));

    // Create a single render target view from the swap chain.
    // The driver will handle stereo separation based on the swap chain's stereo flag.
    DX::ThrowIfFailed(
        m_d3dDevice->CreateRenderTargetView(m_backBuffer.Get(), nullptr, m_renderTargetView.ReleaseAndGetAddressOf())
    );

    // Create intermediate textures for left and right eyes
    D3D11_TEXTURE2D_DESC texDesc = {};
    m_backBuffer->GetDesc(&texDesc);
    texDesc.ArraySize = 1; // Not a texture array
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE; // Use NT Handles for robust stereo compatibility

    DX::ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&texDesc, nullptr, m_leftEyeTexture.ReleaseAndGetAddressOf())
    );
    DX::ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&texDesc, nullptr, m_rightEyeTexture.ReleaseAndGetAddressOf())
    );

    // Create render target views for the intermediate textures
    CD3D11_RENDER_TARGET_VIEW_DESC rtvDesc(D3D11_RTV_DIMENSION_TEXTURE2D, texDesc.Format);
    DX::ThrowIfFailed(
        m_d3dDevice->CreateRenderTargetView(m_leftEyeTexture.Get(), &rtvDesc, m_leftEyeRTV.ReleaseAndGetAddressOf())
    );
    DX::ThrowIfFailed(
        m_d3dDevice->CreateRenderTargetView(m_rightEyeTexture.Get(), &rtvDesc, m_rightEyeRTV.ReleaseAndGetAddressOf())
    );



    m_spoutStereoWindow.CreateWindowResources();
}

void Game::ReleaseWindowResources()
{
    m_spoutStereoWindow.ReleaseWindowResources();

    // Clear the previous window size specific context.
    m_d3dContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTargetView.Reset();
    m_backBuffer.Reset();
    m_leftEyeTexture.Reset();
    m_rightEyeTexture.Reset();
    m_leftEyeRTV.Reset();
    m_rightEyeRTV.Reset();
    m_swapChain.Reset();
    m_d3dContext->Flush();
}

void Game::OnDeviceLost()
{
    ResetDevice();
}

void Game::ResetDevice()
{
    ReleaseWindowResources();
    ReleaseDeviceResources();
    ReleaseDevice();

    CreateDevice();
    CreateDeviceResources();
    CreateWindowResources();
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

    m_spoutStereoWindow.Update();

    if (m_spoutStereoWindow.requiresDeviceReset()) {
        ResetDevice();
        m_spoutStereoWindow.clearRequiresDeviceReset();
    }
}

// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0) {
        return;
    }

    // Draw the scene to our intermediate textures
    m_spoutStereoWindow.Draw(m_leftEyeRTV, m_rightEyeRTV);

    // Copy the intermediate textures to the correct slices of the swap chain's back buffer
    m_d3dContext->CopySubresourceRegion(m_backBuffer.Get(), 0, 0, 0, 0, m_leftEyeTexture.Get(), 0, nullptr);
    m_d3dContext->CopySubresourceRegion(m_backBuffer.Get(), 1, 0, 0, 0, m_rightEyeTexture.Get(), 0, nullptr);

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
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        OnDeviceLost();
    }
    else {
        DX::ThrowIfFailed(hr);
    }
}

// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
    m_spoutStereoWindow.ResetInputTrackers();
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
    // TODO: Game is being power-resumed (or returning from minimize).
    m_timer.ResetElapsedTime();
    m_spoutStereoWindow.ResetInputTrackers();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_window)
        return;

    // TODO: Game window is being resized.
    m_width = (std::max)(width, 1);
    m_height = (std::max)(height, 1);

    ReleaseWindowResources();
    CreateWindowResources();
}
