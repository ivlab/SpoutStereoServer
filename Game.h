//
// Game.h
//

#pragma once

#include "pch.h"
#include "SpoutDX/SpoutDX.h"
#include "StepTimer.h"

#include <minvr3.h>

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
    void OnSpoutConnectionOpen();
    void OnSpoutConnectionClose();

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();
    void Present();

    void CreateDevice();
    void CreateOrUpdateWindowSpecificResources();
    void ResetDevice();

    void OnDeviceLost();

    void PollKeyUpDownEvent(DirectX::Keyboard::Keys keyId, const std::string& keyName, std::vector<VREvent*>* eventList);
    
    // Device resources.
    HWND                                            m_window;
    int                                             m_xPos;
    int                                             m_yPos;
    int                                             m_outputWidth;
    int                                             m_outputHeight;

    D3D_FEATURE_LEVEL                               m_featureLevel;

    Microsoft::WRL::ComPtr<ID3D11Device>           m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1>         m_swapChain;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetViewLeft;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetViewRight;
    CD3D11_VIEWPORT                                 m_viewport;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // Left/Right Textures
    ID3D11Texture2D*                                m_defaultTextureLeft;
    ID3D11Texture2D*                                m_defaultTextureRight;
    ID3D11ShaderResourceView*                       m_textureViewLeft;
    ID3D11ShaderResourceView*                       m_textureViewRight;
    ID3D11SamplerState*                             m_samplerState;

    // Shaders for Rendering Fullscreen Quad
    ID3D11VertexShader*                             m_vertexShader;
    ID3D11PixelShader*                              m_pixelShader;

    spoutDX m_receiverLeft;
    ID3D11Texture2D* m_receivedTextureLeft = nullptr;
    ID3D11ShaderResourceView* m_receivedTextureViewLeft = nullptr;

    spoutDX m_receiverRight;
    ID3D11Texture2D* m_receivedTextureRight = nullptr;
    ID3D11ShaderResourceView* m_receivedTextureViewRight = nullptr;

    bool m_inStandbyMode;

    std::unique_ptr<DirectX::SpriteFont> m_font;
    std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;

    bool m_quitOnEsc;
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardStateTracker;
    DirectX::Mouse::ButtonStateTracker m_mouseStateTracker;

    bool m_openMinVREventConnection;
    std::string m_minVRDevName;
    int m_port;
    int m_readWriteTimeoutMs;
    SOCKET m_listenerFd;
    std::vector<SOCKET> m_clientFds;
    std::vector<std::string> m_clientDescs;
};
