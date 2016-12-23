#include "CiceroInputMethod.h"
#include "DisplayMenuManager.h"
#include <memory>
#define IMEINFO "状态: "

//==========================================================
//					CiceroInputMethod
//==========================================================
std::unique_ptr<char[]> WcharToChar(wchar_t* wc)
{
	int len = WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), NULL, 0, NULL, NULL);
	std::unique_ptr<char[]> m_char(new char[len + 1]);
	WideCharToMultiByte(CP_ACP, 0, wc, wcslen(wc), m_char.get(), len, NULL, NULL);
	m_char[len] = '\0';
	return m_char;
}

CiceroInputMethod* CiceroInputMethod::GetSingleton()
{
	static CiceroInputMethod instance;
	return &instance;
}

STDAPI_(ULONG) CiceroInputMethod::AddRef()
{
	return ++m_refCount;
}

STDAPI_(ULONG) CiceroInputMethod::Release()
{
	LONG tempCount = --m_refCount;
	if (m_refCount == 0)
		delete this;
	return tempCount;
}

BOOL CiceroInputMethod::SetupSinks()
{
	CoInitialize(NULL);
	HRESULT hr;
	hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfThreadMgrEx), (void**)&m_pThreadMgrEx);
	if (FAILED(hr))
		return FALSE;
	TfClientId cid;
	if (FAILED(m_pThreadMgrEx->ActivateEx(&cid, TF_TMAE_UIELEMENTENABLEDONLY)))
		return FALSE;
	ITfSource *srcTm;
	if (SUCCEEDED(hr = m_pThreadMgrEx->QueryInterface(__uuidof(ITfSource), (void **)&srcTm)))
	{
		srcTm->AdviseSink(__uuidof(ITfUIElementSink), (ITfUIElementSink*)this, &m_dwUIElementSinkCookie);
		srcTm->AdviseSink(__uuidof(ITfInputProcessorProfileActivationSink), (ITfInputProcessorProfileActivationSink*)this, &m_dwAlpnSinkCookie);
		srcTm->Release();
	}
	hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (LPVOID*)&m_pProfiles);
	if (FAILED(hr))
		return FALSE;
	m_pProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr, (void **)&m_pProfileMgr);

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED(hr))
	{
		hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfThreadMgr), (void**)&m_pThreadMgr);
		if (FAILED(hr)) return FALSE;
	}
	return S_OK;
}

void CiceroInputMethod::ReleaseSinks()
{
	HRESULT hr;
	ITfSource* source;
	if (m_pThreadMgrEx && SUCCEEDED(m_pThreadMgrEx->QueryInterface(__uuidof(ITfSource), (void**)&source)))
	{
		hr = source->UnadviseSink(m_dwUIElementSinkCookie);
		hr = source->UnadviseSink(m_dwAlpnSinkCookie);
		source->Release();
		m_pThreadMgrEx->Deactivate();
		m_pThreadMgr->Release();
		m_pProfileMgr->Release();
		m_pProfiles->Release();
		m_pThreadMgrEx->Release();
	}
	CoUninitialize();
}

STDAPI CiceroInputMethod::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == NULL)
		return E_INVALIDARG;
	*ppvObj = NULL;
	if (IsEqualIID(riid, IID_IUnknown))
		*ppvObj = reinterpret_cast<IUnknown *>(this);
	else if (IsEqualIID(riid, __uuidof(ITfUIElementSink)))
		*ppvObj = (ITfUIElementSink *)this;
	else if (IsEqualIID(riid, __uuidof(ITfInputProcessorProfileActivationSink)))
		*ppvObj = (ITfInputProcessorProfileActivationSink*)this;
	else if (IsEqualIID(riid, __uuidof(ITfLanguageProfileNotifySink)))
		*ppvObj = (ITfLanguageProfileNotifySink*)this;
	if (*ppvObj)
	{
		AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

STDAPI CiceroInputMethod::OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags)
{
	bool bActive = (dwFlags & TF_IPSINK_FLAG_ACTIVE);
	if (!bActive)
		return S_OK;
	this->GetCurrentInputMethodName();
	if (dwProfileType & TF_PROFILETYPE_INPUTPROCESSOR)
	{
		m_ciceroState = true;
		//_MESSAGE("TIPÊäÈë·¨ [%08X]", (unsigned int)hkl);
	}
	else if (dwProfileType & TF_PROFILETYPE_KEYBOARDLAYOUT)
	{
		m_ciceroState = false;
		//_MESSAGE("HKL/IME [%08X]", (unsigned int)hkl);
	}
	else
	{
		m_ciceroState = false;
		//_MESSAGE("dwProfileType unknown!!!\n");
	}
	return S_OK;
}


STDAPI CiceroInputMethod::BeginUIElement(DWORD dwUIElementId, BOOL *pbShow)
{
	ITfUIElement* pElement = GetUIElement(dwUIElementId);
	if (!pElement)
		return E_INVALIDARG;
	*pbShow = FALSE;
	ITfCandidateListUIElement* lpCandidate = nullptr;
	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement), (void**)&lpCandidate)))
	{
		GetCandidateStrings(lpCandidate);
		lpCandidate->Release();
	}
	pElement->Release();
	return S_OK;
}

STDAPI CiceroInputMethod::UpdateUIElement(DWORD dwUIElementId)
{
	ITfUIElement* pElement = GetUIElement(dwUIElementId);
	if (!pElement)
		return E_INVALIDARG;
	ITfCandidateListUIElement* lpCandidate = nullptr;
	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement), (void**)&lpCandidate)))
	{
		GetCandidateStrings(lpCandidate);
		lpCandidate->Release();
	}
	pElement->Release();
	return S_OK;
}

STDAPI CiceroInputMethod::EndUIElement(DWORD dwUIElementId)
{
	ITfUIElement* pElement = GetUIElement(dwUIElementId);
	if (!pElement)
		return E_INVALIDARG;
	ITfCandidateListUIElement* lpCandidate = nullptr;
	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement), (void**)&lpCandidate)))
	{
		//GetCandidateStrings(lpCandidate);//to do something...
		lpCandidate->Release();
	}
	pElement->Release();
	ITfDocumentMgr* pDocMgr = nullptr;
	ITfContext* pContex = nullptr;
	ITfContextView* pContexView = nullptr;
	HWND hActiveHwnd = nullptr;
	if (SUCCEEDED(m_pThreadMgrEx->GetFocus(&pDocMgr)))
	{
		if (SUCCEEDED(pDocMgr->GetTop(&pContex)))
		{
			if (SUCCEEDED(pContex->GetActiveView(&pContexView)))
			{
				pContexView->GetWnd(&hActiveHwnd);
				pContexView->Release();
			}
			pContex->Release();
		}
		pDocMgr->Release();
	}
	if (hActiveHwnd)
		SendMessageW(hActiveHwnd, WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 0);
	return S_OK;
}

ITfUIElement* CiceroInputMethod::GetUIElement(DWORD dwUIElementId)
{
	ITfUIElementMgr* pElementMgr;
	ITfUIElement* pElement = nullptr;
	if (SUCCEEDED(m_pThreadMgrEx->QueryInterface(__uuidof(ITfUIElementMgr), (void**)&pElementMgr)))
	{
		pElementMgr->GetUIElement(dwUIElementId, &pElement);
		pElementMgr->Release();
	}
	return pElement;
}

void CiceroInputMethod::GetCandidateStrings(ITfCandidateListUIElement* lpCandidate)
{
	static DisplayMenuManager* mm = DisplayMenuManager::GetSingleton();
	if (!mm->enableState)
		return;
	UINT uIndex = NULL;
	UINT uCount = NULL;
	UINT uCurrentPage = NULL;
	UINT uPageCount = NULL;
	DWORD dwPageStart = NULL;
	DWORD dwPageSize = NULL;
	DWORD dwPageSelection = NULL;
	WCHAR* result = nullptr;
	lpCandidate->GetSelection(&uIndex);
	lpCandidate->GetCount(&uCount);
	lpCandidate->GetCurrentPage(&uCurrentPage);
	//_MESSAGE("CurrentPage = %d", uCurrentPage);
	dwPageSelection = (DWORD)uIndex;
	lpCandidate->GetPageIndex(NULL, 0, &uPageCount);
	if (uPageCount > NULL)
	{
		std::unique_ptr<UINT[]> indexList(new UINT[uPageCount]);
		lpCandidate->GetPageIndex(indexList.get(), uPageCount, &uPageCount);
		dwPageStart = indexList[uCurrentPage];
		dwPageSize = (uCurrentPage < uPageCount - 1) ? min(uCount, indexList[uCurrentPage + 1]) - dwPageStart : uCount - dwPageStart;
	}
	dwPageSize = min(dwPageSize, MAX_CANDLIST);
	char buffer[0x8];
	mm->candidateList.clear();
	mm->selectedIndex = dwPageSelection;
	mm->pageSize = dwPageSize;
	mm->pageStartIndex = dwPageStart;
	for (size_t i = 0; i < dwPageSize; i++)
	{
		if (SUCCEEDED(lpCandidate->GetString(i + dwPageStart, &result)))
		{
			if (result)
			{
				sprintf(buffer, "%d.", i + 1);
				string temp(buffer);
				temp += WcharToChar(result).get();
				mm->candidateList.push_back(temp);
				SysFreeString(result);
			}
		}
	}
}

bool CiceroInputMethod::DisableInputMethod(LANGID langID = CHS_KB)
{
	typedef HRESULT(WINAPI *_CREATEINPUTPROCESSORPROFILES)(ITfInputProcessorProfiles**);
	_CREATEINPUTPROCESSORPROFILES CreateInputProcessorProfiles = (_CREATEINPUTPROCESSORPROFILES)GetProcAddress(GetModuleHandle("msctf.dll"), "TF_CreateInputProcessorProfiles");
	bool result = false;
	if (CreateInputProcessorProfiles)
	{
		HRESULT hr;
		ITfInputProcessorProfiles *pInputProcessorProfiles;

		hr = CreateInputProcessorProfiles(&pInputProcessorProfiles);
		if (SUCCEEDED(hr))
		{
			GUID clsid = GUID_NULL;
			hr = pInputProcessorProfiles->ActivateLanguageProfile(clsid, langID, clsid);
			if (SUCCEEDED(hr))
				result = true;
			pInputProcessorProfiles->Release();
		}
	}
	return result;
}

bool CiceroInputMethod::GetLayoutName(const WCHAR* kl, WCHAR* nm)
{
	long lRet;
	HKEY hKey;
	static WCHAR tchData[64];
	DWORD dwSize;
	WCHAR keypath[200];
	wsprintfW(keypath, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%s", kl);
	lRet = RegOpenKeyExW(HKEY_LOCAL_MACHINE, keypath, 0, KEY_QUERY_VALUE, &hKey);
	if (lRet == ERROR_SUCCESS)
	{
		dwSize = sizeof(tchData);
		lRet = RegQueryValueExW(hKey, L"Layout Text", NULL, NULL, (LPBYTE)tchData, &dwSize);
	}
	RegCloseKey(hKey);
	if (lRet == ERROR_SUCCESS && wcslen(nm) < 64)
	{
		wcscpy(nm, tchData);
		return true;
	}
	return false;
}

void CiceroInputMethod::GetCurrentInputMethodName()
{
	static WCHAR lastTipName[64];
	static DisplayMenuManager* menu = DisplayMenuManager::GetSingleton();
	ZeroMemory(lastTipName, sizeof(lastTipName));
	TF_INPUTPROCESSORPROFILE tip;
	m_pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &tip);
	if (tip.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR)
	{
		WCHAR* bstrImeName = NULL;
		m_pProfiles->GetLanguageProfileDescription(tip.clsid, tip.langid, tip.guidProfile, &bstrImeName);
		if (wcslen(bstrImeName) < 64)
			wcscpy(lastTipName, bstrImeName);
		SysFreeString(bstrImeName);
	}
	else if (tip.dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT)
	{
		static WCHAR klnm[KL_NAMELENGTH];
		if (GetKeyboardLayoutNameW(klnm))
		{
			GetLayoutName(klnm, lastTipName);
		}
	}
	std::string name = WcharToChar(lastTipName).get();
	UInt32 pos = name.find("-", NULL);
	std::string temp(IMEINFO);
	if (pos != string::npos)
	{
		pos += 2;
		temp.append(name, pos, name.size());
	}
	else
		temp.append(name);
	menu->title = temp;
	//_MESSAGE("InputMethodName = %s,size = %d", menu->title.c_str(), menu->title.size());
}
