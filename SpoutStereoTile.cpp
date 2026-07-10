#include "pch.h"
#include "SpoutStereoTile.h"
#include "SpoutStereoWindow.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;


SpoutStereoTile::SpoutStereoTile() :
    m_receivingFromSpout(false),
    m_requiresDeviceReset(false),
    m_neverShowDebugGraphics(false),
    m_showDebugGraphics(true)
{
}

SpoutStereoTile::~SpoutStereoTile()
{
}

void
SpoutStereoTile::Initialize(const std::string &name, SpoutStereoWindow* parentWindow)
{
	m_name = name;
	if ((m_name.length() > 0) && (m_name[m_name.length() - 1] != '_')) {
		m_name += "_";
	}
    m_parentWindow = parentWindow;

	int x = ConfigVal::Get(m_name + "VIEWPORT_X", 0);
	int y = ConfigVal::Get(m_name + "VIEWPORT_Y", 0);
	int w = ConfigVal::Get(m_name + "VIEWPORT_WIDTH", 1280);
	int h = ConfigVal::Get(m_name + "VIEWPORT_HEIGHT", 1280);
	m_viewport = CD3D11_VIEWPORT(x, y, w, h);

    // Spout receiver setup
    m_senderNameLeft = ConfigVal::Get(m_name + "SPOUT_SENDER_NAME_LEFT", std::string(m_name + "_LeftEye"));
    m_receiverLeft.SetReceiverName(m_senderNameLeft.c_str());
    
    m_senderNameRight = ConfigVal::Get(m_name + "SPOUT_SENDER_NAME_RIGHT", std::string(m_name + "_RightEye"));
    m_receiverRight.SetReceiverName(m_senderNameRight.c_str());

    // configure the default display when no spout source is detected
    m_spoutLabelX = ConfigVal::Get(m_name + "SPOUT_LABEL_X", 0);
    m_spoutLabelY = ConfigVal::Get(m_name + "SPOUT_LABEL_Y", 0);

    m_neverShowDebugGraphics = ConfigVal::Get(m_name + "NEVER_SHOW_DEBUG_GRAPHICS", false);
}

void
SpoutStereoTile::CreateDeviceResources(Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice,
                                       Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext)
{
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;

    // tell spout receivers about the dx context
    if (!m_receiverLeft.OpenDirectX11(m_d3dDevice.Get())) {
        DX::ThrowIfFailed(0);
    }

    if (!m_receiverRight.OpenDirectX11(m_d3dDevice.Get())) {
        DX::ThrowIfFailed(0);
    }

    // Create left and right default textures to use with testing and/or when no 
    // spout connection is present
    int texWidth = m_viewport.Width;
    int texHeight = m_viewport.Height;
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
            m_d3dDevice->CreateShaderResourceView(m_defaultTextureLeft, nullptr, &m_defaultTextureViewLeft)
        );
    }

    DX::ThrowIfFailed(
        m_d3dDevice->CreateTexture2D(&textureDesc, &textureSubresourceDataRight, &m_defaultTextureRight)
    );
    if (m_defaultTextureRight != NULL) {
        DX::ThrowIfFailed(
            m_d3dDevice->CreateShaderResourceView(m_defaultTextureRight, nullptr, &m_defaultTextureViewRight)
        );
    }

    delete[] texBytesLeft;
    delete[] texBytesRight;

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
}

void
SpoutStereoTile::ReleaseDeviceResources()
{
    m_receiverLeft.ReleaseReceiver();
    m_defaultTextureLeft->Release();
    m_defaultTextureViewLeft->Release();
    m_receivedTextureLeft->Release();
    m_receivedTextureViewLeft->Release();

    m_receiverRight.ReleaseReceiver();
    m_defaultTextureRight->Release();
    m_defaultTextureViewRight->Release();
    m_receivedTextureRight->Release();
    m_receivedTextureViewRight->Release();

    m_d3dContext.Reset();
    m_d3dDevice.Reset();
}

void
SpoutStereoTile::CreateWindowResources()
{
}


void
SpoutStereoTile::ReleaseWindowResources()
{
}


void
SpoutStereoTile::Update()
{
    // --- LEFT TEXTURE SPOUT CONNECTION ---

    bool receivedLeft = false;
    bool receivedRight = false;

   // Receive a new texture
    if (m_receiverLeft.ReceiveTexture()) {
        receivedLeft = true;
        // The D3D11 device within the SpoutDX class could have changed.
        // If it has switched to use a different sender graphics adapter,
        // stop receiving the texture and re-initialize the application.
        if (m_receiverLeft.GetAdapterAuto()) {
            if (m_d3dDevice.Get() != m_receiverLeft.GetDX11Device()) {
                m_requiresDeviceReset = true;
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


    // --- RIGHT TEXTURE SPOUT CONNECTION ---

    // Receive a new texture
    if (m_receiverRight.ReceiveTexture()) {
        receivedRight = true;

        // The D3D11 device within the SpoutDX class could have changed.
        // If it has switched to use a different sender graphics adapter,
        // stop receiving the texture and re-initialize the application.
        if (m_receiverRight.GetAdapterAuto()) {
            if (m_d3dDevice.Get() != m_receiverRight.GetDX11Device()) {
                m_requiresDeviceReset = true;
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

    m_receivingFromSpout = receivedLeft || receivedRight;
}



void
SpoutStereoTile::Draw(ComPtr<ID3D11RenderTargetView> renderTargetViewLeft, 
                      ComPtr<ID3D11RenderTargetView> renderTargetViewRight)
{
    m_d3dContext->RSSetViewports(1, &m_viewport);

    // -- LEFT EYE --
    m_d3dContext->OMSetRenderTargets(1, renderTargetViewLeft.GetAddressOf(), nullptr);
    // Clear the left eye to Red
    m_d3dContext->ClearRenderTargetView(renderTargetViewLeft.Get(), Colors::Red);

    // -- RIGHT EYE --
    m_d3dContext->OMSetRenderTargets(1, renderTargetViewRight.GetAddressOf(), nullptr);
    // Clear the right eye to Blue
    m_d3dContext->ClearRenderTargetView(renderTargetViewRight.Get(), Colors::Blue);

    // For this test, we are not drawing any textures.
    // We are only clearing the render targets to distinct colors.
}
