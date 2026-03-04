#pragma once
#include <windows.h>
#include <d3d11.h>

// DirectX11 device access (for preview texture creation)
ID3D11Device* GetD3DDevice();
ID3D11DeviceContext* GetD3DDeviceContext();

// GUI lifecycle
bool InitGui(HINSTANCE hInstance, int nCmdShow);
int  RunGuiLoop();
void CleanupGui();
