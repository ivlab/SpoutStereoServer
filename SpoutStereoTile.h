#pragma once

#include "pch.h"

#include "SpoutDX/SpoutDX.h"
#include "StepTimer.h"

#include <minvr3.h>

// forward declaration
class SpoutStereoWindow;

class SpoutStereoTile
{
public:
	SpoutStereoTile();
	virtual ~SpoutStereoTile();

	void Initialize(const std::string &name, SpoutStereoWindow *parentWindow);

    void CreateDeviceResources(Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice,
                               Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext);
    void ReleaseDeviceResources();

    void CreateWindowResources();
    void ReleaseWindowResources();

    void Update();

    void Draw(Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViewLeft, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetViewRight);

    bool getShowDebugGraphics() {
        return m_showDebugGraphics;
    }

    void setShowDebugGraphics(bool value) {
        m_showDebugGraphics = value;
    }

    bool receivingFromSpout() {
        return m_receivingFromSpout;
    }

    bool requiresDeviceReset() {
        return m_requiresDeviceReset;
    }

    void clearRequiresDeviceReset() {
        m_requiresDeviceReset = false;
    }

protected:
    CD3D11_VIEWPORT m_viewport;
    float m_spoutLabelX;
    float m_spoutLabelY;
    bool m_neverShowDebugGraphics;

    std::string m_name;
    SpoutStereoWindow* m_parentWindow = nullptr;
    bool m_receivingFromSpout;
    bool m_requiresDeviceReset;
    bool m_showDebugGraphics;

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;

    // Common sampler state for all texturing
    ID3D11SamplerState* m_samplerState = nullptr;

    // Left eye specific
    std::string m_senderNameLeft;
    spoutDX m_receiverLeft;
    ID3D11Texture2D* m_defaultTextureLeft = nullptr;
    ID3D11ShaderResourceView* m_defaultTextureViewLeft = nullptr;
    ID3D11Texture2D* m_receivedTextureLeft = nullptr;
    ID3D11ShaderResourceView* m_receivedTextureViewLeft = nullptr;

    // Right eye specific
    std::string m_senderNameRight;
    spoutDX m_receiverRight;
    ID3D11Texture2D* m_defaultTextureRight = nullptr;
    ID3D11ShaderResourceView* m_defaultTextureViewRight = nullptr;
    ID3D11Texture2D* m_receivedTextureRight = nullptr;
    ID3D11ShaderResourceView* m_receivedTextureViewRight = nullptr;
};
