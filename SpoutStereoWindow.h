#pragma once

#include "pch.h"

#include "SpoutDX/SpoutDX.h"
#include "StepTimer.h"
#include "SpoutStereoTile.h"

#include <minvr3.h>

class SpoutStereoWindow
{
public:

    SpoutStereoWindow();
    virtual ~SpoutStereoWindow();

    void Initialize(HWND window);

    void CreateDeviceResources(Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice,
                               Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext);
    void ReleaseDeviceResources();

    void CreateWindowResources();
    void ReleaseWindowResources();

    void OnSpoutOpenStream();
    void OnSpoutCloseStream();

    void ResetInputTrackers();
    void PollKeyUpDownEvent(DirectX::Keyboard::Keys keyId, const std::string& keyName, std::vector<VREvent*>* eventList);

    void Update();
    void Draw(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViewLeft, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViewRight);

    ID3D11VertexShader* fullscreenVertexShader() {
        return m_fullscreenVertexShader;
    }
    ID3D11PixelShader* fullscreenPixelShader() {
        return m_fullscreenPixelShader;
    }

    std::shared_ptr<DirectX::SpriteFont> font() {
        return m_font;
    }

    std::shared_ptr<DirectX::SpriteBatch> fontSpriteBatch() {
        return m_spriteBatch;
    }

    bool receivingFromSpout() {
        for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
            if ((*tile)->receivingFromSpout()) {
                return true;
            }
        }
        return false;
    }


    bool requiresDeviceReset() {
        for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
            if ((*tile)->requiresDeviceReset()) {
                return true;
            }
        }
        return false;
    }

    void clearRequiresDeviceReset() {
        for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
            (*tile)->clearRequiresDeviceReset();
        }
    }

protected:
    HWND m_window;
    bool m_lastReceivingFromSpout;
    std::vector<SpoutStereoTile*> m_tiles;

    // Common Graphics Resources
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;

    ID3D11VertexShader* m_fullscreenVertexShader;
    ID3D11PixelShader* m_fullscreenPixelShader;
    std::shared_ptr<DirectX::SpriteFont> m_font;
    std::shared_ptr<DirectX::SpriteBatch> m_spriteBatch;

    // Window Input
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardStateTracker;
    DirectX::Mouse::ButtonStateTracker m_mouseStateTracker;

    // MinVR Event Connection
    bool m_openMinVREventConnection;
    int m_port;
    int m_readWriteTimeoutMs;
    SOCKET m_listenerFd;
    std::vector<SOCKET> m_clientFds;
    std::vector<std::string> m_clientDescs;
};