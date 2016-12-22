#include "D3D9Interface.h"
#include "D3D9Device.h"
#include "DisplayMenuManager.h"

D3DPRESENT_PARAMETERS* tempPresent;

TESRDirect3D9::TESRDirect3D9(IDirect3D9 *d3d) : m_d3d(d3d) {

}

TESRDirect3D9::~TESRDirect3D9() {

}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::QueryInterface(REFIID riid, void **ppvObj) {
	return m_d3d->QueryInterface(riid, ppvObj);
}

ULONG STDMETHODCALLTYPE TESRDirect3D9::AddRef() {
	return m_d3d->AddRef();
}

ULONG STDMETHODCALLTYPE TESRDirect3D9::Release() {
	ULONG count = m_d3d->Release();

	if (count == 0)
	{
		delete this;
	}
	return count;
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::RegisterSoftwareDevice(void *pInitializeFunction) {
	return m_d3d->RegisterSoftwareDevice(pInitializeFunction);
}

UINT STDMETHODCALLTYPE TESRDirect3D9::GetAdapterCount() {
	return m_d3d->GetAdapterCount();
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9 *pIdentifier) {
	return m_d3d->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
}

UINT STDMETHODCALLTYPE TESRDirect3D9::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) {
	return m_d3d->GetAdapterModeCount(Adapter, Format);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE *pMode) {
	return m_d3d->EnumAdapterModes(Adapter, Format, Mode, pMode);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) {
	return m_d3d->GetAdapterDisplayMode(Adapter, pMode);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) {
	return m_d3d->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) {
	return m_d3d->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels) {
	return m_d3d->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) {
	return m_d3d->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) {
	return m_d3d->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps) {
	return m_d3d->GetDeviceCaps(Adapter, DeviceType, pCaps);
}

HMONITOR STDMETHODCALLTYPE TESRDirect3D9::GetAdapterMonitor(UINT Adapter) {
	return m_d3d->GetAdapterMonitor(Adapter);
}

HRESULT STDMETHODCALLTYPE TESRDirect3D9::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DDevice9 **ppReturnedDeviceInterface) {
	HRESULT hr = m_d3d->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	_MESSAGE("IDirect3DDevice9 = %p", *ppReturnedDeviceInterface);
	//DisplayMenuManager::GetSingleton()->device = *ppReturnedDeviceInterface;
	DisplayMenuManager::GetSingleton()->Initialize(*ppReturnedDeviceInterface);
	*ppReturnedDeviceInterface = new TESRDirect3DDevice9(this, *ppReturnedDeviceInterface);
	tempPresent = pPresentationParameters;
	return hr;
}
