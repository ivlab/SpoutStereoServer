//
// Game.h
//

#pragma once

#include "pch.h"
#include "SpoutDX/SpoutDX.h"
#include "StepTimer.h"
#include "SpoutStereoWindow.h"

// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game
{
public:

    Game() noexcept;
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(HWND window);

    // Basic game loop
    void Tick();

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height);
    void OnDeviceLost();

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();
    void Present();

    void CreateDevice();
    void ReleaseDevice();
    void ResetDevice();

    void CreateDeviceResources();
    void ReleaseDeviceResources();

    void CreateWindowResources();
    void ReleaseWindowResources();    

    HWND                                            m_window;
    D3D_FEATURE_LEVEL                               m_featureLevel;
    Microsoft::WRL::ComPtr<ID3D11Device>            m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetViewLeft;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetViewRight;
    DX::StepTimer                                   m_timer;

    int m_xPos;
    int m_yPos;
    int m_width;
    int m_height;
    int m_stereo;

    SpoutStereoWindow m_spoutStereoWindow;
};
