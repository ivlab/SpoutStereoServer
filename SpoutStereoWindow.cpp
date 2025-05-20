#include "pch.h"
#include "SpoutStereoWindow.h"
#include "Generated Files/PixelShader.h"
#include "Generated Files/VertexShader.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

SpoutStereoWindow::SpoutStereoWindow() : m_lastReceivingFromSpout(false)
{

}

SpoutStereoWindow::~SpoutStereoWindow()
{
	for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
		delete (*tile);
	}
	m_tiles.clear();
}

void
SpoutStereoWindow::Initialize(HWND window, bool stereo)
{
	m_window = window;
    m_stereo = stereo;

	// create tiles based on config file settings
	std::vector<std::string> tileNames = ConfigVal::Get("TILES", std::vector<std::string>());
	for (auto tileName = tileNames.begin(); tileName != tileNames.end(); tileName++) {
		SpoutStereoTile* tile = new SpoutStereoTile();
		tile->Initialize(*tileName, this);
		m_tiles.push_back(tile);
	}

	// Init MinVR Event Connection
	m_openMinVREventConnection = ConfigVal::Get("OPEN_MINVR_EVENT_CONNECTION", true);
	m_port = ConfigVal::Get("MINVR_EVENT_CONNECTION_PORT", 9030);
	m_readWriteTimeoutMs = ConfigVal::Get("MINVR_EVENT_CONNECTION_READ_WRITE_TIMEOUT_MS", 500);
	if (m_openMinVREventConnection) {
		MinVR3Net::Init();
		MinVR3Net::CreateListener(m_port, &m_listenerFd);
	}

    // Init DX input devices
    m_keyboard = std::make_unique<Keyboard>();
    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);
}

void
SpoutStereoWindow::CreateDeviceResources(Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice,
                                         Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext)
{
    m_d3dDevice = d3dDevice;
    m_d3dContext = d3dContext;

    m_font = std::make_unique<SpriteFont>(m_d3dDevice.Get(), L"CourierNew-32.spritefont");
    m_spriteBatch = std::make_unique<SpriteBatch>(m_d3dContext.Get());

    // Create simple shaders for fullscreen quad (these are compiled into header files during the build)
    // Right-click on the .hlsl files and go to Properties to configure this.
    DX::ThrowIfFailed(
        m_d3dDevice->CreateVertexShader(g_vertexshader, sizeof(g_vertexshader), nullptr, &m_fullscreenVertexShader)
    );
    DX::ThrowIfFailed(
        m_d3dDevice->CreatePixelShader(g_pixelshader, sizeof(g_pixelshader), nullptr, &m_fullscreenPixelShader)
    );

    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->CreateDeviceResources(m_d3dDevice, m_d3dContext);
    }
}

void 
SpoutStereoWindow::ReleaseDeviceResources()
{
    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->ReleaseDeviceResources();
    }

    m_font.reset();
    m_spriteBatch.reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();
}

void 
SpoutStereoWindow::CreateWindowResources()
{
    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->CreateWindowResources();
    }
}


void 
SpoutStereoWindow::ReleaseWindowResources()
{
    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->ReleaseWindowResources();
    }
}

void
SpoutStereoWindow::OnSpoutOpenStream()
{
    // Restore the window in case it is currently minimized
    ShowWindow(m_window, SW_RESTORE);

    // Turn off debug graphics for all tiles
    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->setShowDebugGraphics(false);
    }
}

void
SpoutStereoWindow::OnSpoutCloseStream()
{
    // Minimize the window
    ShowWindow(m_window, SW_MINIMIZE);

    // Turn on debug graphics for all tiles
    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->setShowDebugGraphics(true);
    }
}

void
SpoutStereoWindow::ResetInputTrackers()
{
    m_keyboardStateTracker.Reset();
    m_mouseStateTracker.Reset();
}

void 
SpoutStereoWindow::PollKeyUpDownEvent(DirectX::Keyboard::Keys keyId, const std::string& keyName, std::vector<VREvent*>* eventList)
{
    if (m_keyboardStateTracker.IsKeyPressed(keyId)) {
        eventList->push_back(new VREvent("Keyboard/" + keyName + "/Down"));
    }
    if (m_keyboardStateTracker.released.W) {
        eventList->push_back(new VREvent("Keyboard/" + keyName + "/Up"));
    }
}

void
SpoutStereoWindow::Update()
{
    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->Update();
    }

    bool receivingFromSpoutThisFrame = receivingFromSpout();
    if (receivingFromSpoutThisFrame && !m_lastReceivingFromSpout) {
        OnSpoutOpenStream();
    }
    else if (!receivingFromSpoutThisFrame && m_lastReceivingFromSpout) {
        OnSpoutCloseStream();
    }
    m_lastReceivingFromSpout = receivingFromSpoutThisFrame;


    m_keyboardStateTracker.Update(m_keyboard->GetState());
    m_mouseStateTracker.Update(m_mouse->GetState());

    if (m_keyboardStateTracker.pressed.OemQuestion) {
        // Minimize the window
        ShowWindow(m_window, SW_MINIMIZE);
    }

	if (m_openMinVREventConnection) {
		// Accept new connections from any clients trying to connect
		while (MinVR3Net::IsReadyToRead(&m_listenerFd)) {
			SOCKET newClientFd;
			if (MinVR3Net::TryAcceptConnection(m_listenerFd, &newClientFd)) {
				m_clientFds.push_back(newClientFd);
				m_clientDescs.push_back(MinVR3Net::GetAddressAndPort(newClientFd));
			}
		}


        // Convert input events from DirectX to MinVR VREvents
        std::vector<VREvent*> events;
        if ((m_mouseStateTracker.GetLastState().x != m_mouse->GetState().x) ||
            (m_mouseStateTracker.GetLastState().y != m_mouse->GetState().y)) {
            events.push_back(new VREventVector2("Mouse/Position", m_mouse->GetState().x, m_mouse->GetState().y));
        }
        if (m_mouseStateTracker.leftButton == m_mouseStateTracker.PRESSED) {
            events.push_back(new VREventVector2("Mouse/Left DOWN", m_mouse->GetState().x, m_mouse->GetState().y));
        }
        if (m_mouseStateTracker.leftButton == m_mouseStateTracker.RELEASED) {
            events.push_back(new VREventVector2("Mouse/Left UP", m_mouse->GetState().x, m_mouse->GetState().y));
        }
        if (m_mouseStateTracker.middleButton == m_mouseStateTracker.PRESSED) {
            events.push_back(new VREventVector2("Mouse/Middle DOWN", m_mouse->GetState().x, m_mouse->GetState().y));
        }
        if (m_mouseStateTracker.middleButton == m_mouseStateTracker.RELEASED) {
            events.push_back(new VREventVector2("Mouse/Middle UP", m_mouse->GetState().x, m_mouse->GetState().y));
        }
        if (m_mouseStateTracker.rightButton == m_mouseStateTracker.PRESSED) {
            events.push_back(new VREventVector2("Mouse/Right DOWN", m_mouse->GetState().x, m_mouse->GetState().y));
        }
        if (m_mouseStateTracker.rightButton == m_mouseStateTracker.RELEASED) {
            events.push_back(new VREventVector2("Mouse/Right UP", m_mouse->GetState().x, m_mouse->GetState().y));
        }

        PollKeyUpDownEvent(Keyboard::Keys::A, "A", &events);
        PollKeyUpDownEvent(Keyboard::Keys::B, "B", &events);
        PollKeyUpDownEvent(Keyboard::Keys::C, "C", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D, "D", &events);
        PollKeyUpDownEvent(Keyboard::Keys::E, "E", &events);
        PollKeyUpDownEvent(Keyboard::Keys::F, "F", &events);
        PollKeyUpDownEvent(Keyboard::Keys::G, "G", &events);
        PollKeyUpDownEvent(Keyboard::Keys::H, "H", &events);
        PollKeyUpDownEvent(Keyboard::Keys::I, "I", &events);
        PollKeyUpDownEvent(Keyboard::Keys::J, "J", &events);
        PollKeyUpDownEvent(Keyboard::Keys::K, "K", &events);
        PollKeyUpDownEvent(Keyboard::Keys::L, "L", &events);
        PollKeyUpDownEvent(Keyboard::Keys::M, "M", &events);
        PollKeyUpDownEvent(Keyboard::Keys::N, "N", &events);
        PollKeyUpDownEvent(Keyboard::Keys::O, "O", &events);
        PollKeyUpDownEvent(Keyboard::Keys::P, "P", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Q, "Q", &events);
        PollKeyUpDownEvent(Keyboard::Keys::R, "R", &events);
        PollKeyUpDownEvent(Keyboard::Keys::S, "S", &events);
        PollKeyUpDownEvent(Keyboard::Keys::T, "T", &events);
        PollKeyUpDownEvent(Keyboard::Keys::U, "U", &events);
        PollKeyUpDownEvent(Keyboard::Keys::V, "V", &events);
        PollKeyUpDownEvent(Keyboard::Keys::W, "W", &events);
        PollKeyUpDownEvent(Keyboard::Keys::X, "X", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Y, "Y", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Z, "Z", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D0, "0", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D1, "1", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D2, "2", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D3, "3", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D4, "4", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D5, "5", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D6, "6", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D7, "7", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D8, "8", &events);
        PollKeyUpDownEvent(Keyboard::Keys::D9, "9", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Space, "Space", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Enter, "Enter", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Left, "LeftArrow", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Right, "RightArrow", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Up, "UpArrow", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Down, "DownArrow", &events);
        PollKeyUpDownEvent(Keyboard::Keys::OemComma, "Comma", &events);
        PollKeyUpDownEvent(Keyboard::Keys::OemPeriod, "Period", &events);
        PollKeyUpDownEvent(Keyboard::Keys::LeftShift, "LeftShift", &events);
        PollKeyUpDownEvent(Keyboard::Keys::RightShift, "RightShift", &events);
        PollKeyUpDownEvent(Keyboard::Keys::LeftAlt, "LeftAlt", &events);
        PollKeyUpDownEvent(Keyboard::Keys::RightAlt, "RightAlt", &events);
        PollKeyUpDownEvent(Keyboard::Keys::LeftControl, "LeftControl", &events);
        PollKeyUpDownEvent(Keyboard::Keys::RightControl, "RightControl", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Tab, "Tab", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Delete, "Delete", &events);
        PollKeyUpDownEvent(Keyboard::Keys::OemPlus, "Plus", &events);
        PollKeyUpDownEvent(Keyboard::Keys::OemMinus, "Minus", &events);
        PollKeyUpDownEvent(Keyboard::Keys::Escape, "Esc", &events);


        std::vector<SOCKET> disconnectedFds;
        for (int j = 0; j < m_clientFds.size(); j++) {
            int e = 0;
            bool success = true;
            while ((e < events.size()) && (success)) {
                success = MinVR3Net::SendVREvent(&m_clientFds[j], *(events[e]), m_readWriteTimeoutMs);
                e++;
            }
            if (!success) {
                // If there was a problem sending, then assume this client disconnected
                disconnectedFds.push_back(m_clientFds[j]);
            }
        }

        // Remove any disconnected clients from the list
        for (int i = 0; i < disconnectedFds.size(); i++) {
            auto it = std::find(m_clientFds.begin(), m_clientFds.end(), disconnectedFds[i]);
            if (it != m_clientFds.end()) {
                int index = it - m_clientFds.begin();
                std::cout << "Dropped connection from " << m_clientDescs[index] << std::endl;
                // officially close the socket
                MinVR3Net::CloseSocket(&disconnectedFds[i]);
                // remove the client from both lists
                m_clientFds.erase(m_clientFds.begin() + index);
                m_clientDescs.erase(m_clientDescs.begin() + index);
            }
        }
    } // end MinVR event connection updates
}


void
SpoutStereoWindow::Draw(ComPtr<ID3D11RenderTargetView> renderTargetViewLeft, 
                        ComPtr<ID3D11RenderTargetView> renderTargetViewRight)
{
    // LEFT EYE
    m_d3dContext->OMSetRenderTargets(1, renderTargetViewLeft.GetAddressOf(), nullptr);
    m_d3dContext->ClearRenderTargetView(renderTargetViewLeft.Get(), Colors::Red);

    if (renderTargetViewRight) {
        // RIGHT EYE
        m_d3dContext->OMSetRenderTargets(1, renderTargetViewRight.GetAddressOf(), nullptr);
        m_d3dContext->ClearRenderTargetView(renderTargetViewRight.Get(), Colors::Blue);
    }

    for (auto tile = m_tiles.begin(); tile != m_tiles.end(); tile++) {
        (*tile)->Draw(renderTargetViewLeft, renderTargetViewRight);
    }
}
