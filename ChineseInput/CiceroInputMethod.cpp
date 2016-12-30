#include "CiceroInputMethod.h"
#include "DisplayMenuManager.h"
#include "GameInputManager.h"
#include <memory>
#include <windows.h>
#define IMEINFO	"\u72b6\u6001: "
#define SAFE_RELEASE(p)                             \
{                                                   \
    if (p) {                                        \
        (p)->Release();								\
        (p) = 0;                                    \
	    }                                           \
}

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

bool GetCompartment(IUnknown* pUnk, const GUID& aID, ITfCompartment** pCompartment)
{
	if (!pUnk) return false;

	ITfCompartmentMgr* pCompMgr;
	pUnk->QueryInterface(IID_ITfCompartmentMgr, (void**)(&pCompMgr));
	if (!pCompMgr) return false;
	return SUCCEEDED(pCompMgr->GetCompartment(aID, pCompartment)) && (*pCompartment) != NULL;
}

CiceroInputMethod* CiceroInputMethod::GetSingleton()
{
	static CiceroInputMethod* instance = new CiceroInputMethod();
	return instance;
}

CiceroInputMethod::CiceroInputMethod() : m_refCount(1), m_ciceroState(false), m_pProfileMgr(nullptr), m_pProfiles(nullptr), m_pThreadMgrEx(nullptr), m_pThreadMgr(nullptr), m_pBaseContext(nullptr)
{
	m_uiElementSinkCookie = m_inputProfileSinkCookie = m_threadMgrEventSinkCookie = m_textEditSinkCookie = TF_INVALID_COOKIE;
}

CiceroInputMethod::~CiceroInputMethod()
{

}

STDAPI_(ULONG) CiceroInputMethod::AddRef()
{
	return ++m_refCount;
}

STDAPI_(ULONG) CiceroInputMethod::Release()
{
	if (--m_refCount == 0)
		delete this;
	return m_refCount;
}

BOOL CiceroInputMethod::SetupSinks()
{
	_MESSAGE(__FUNCTION__);
	HRESULT hr;
	CoInitialize(NULL);
	hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfThreadMgrEx), (void**)&m_pThreadMgrEx);
	if (FAILED(hr)) return FALSE;
	if (FAILED(m_pThreadMgrEx->ActivateEx(&m_clientID, TF_TMAE_UIELEMENTENABLEDONLY))) return FALSE;
	ITfSource* source;
	if (SUCCEEDED(hr = m_pThreadMgrEx->QueryInterface(__uuidof(ITfSource), (void **)&source)))
	{
		source->AdviseSink(__uuidof(ITfUIElementSink), (ITfUIElementSink*)this, &m_uiElementSinkCookie);
		source->AdviseSink(__uuidof(ITfInputProcessorProfileActivationSink), (ITfInputProcessorProfileActivationSink*)this, &m_inputProfileSinkCookie);
		source->AdviseSink(__uuidof(ITfThreadMgrEventSink), (ITfThreadMgrEventSink*)this, &m_threadMgrEventSinkCookie);
		source->Release();
	}
	hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (LPVOID*)&m_pProfiles);
	if (FAILED(hr)) return FALSE;
	m_pProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr, (void **)&m_pProfileMgr);

	hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfThreadMgr), (void**)&m_pThreadMgr);
	if (FAILED(hr)) return FALSE;
	_MESSAGE("Enable cicero input...");
	return S_OK;
}

void CiceroInputMethod::ReleaseSinks()
{
	ITfSource* source = nullptr;
	if (m_textEditSinkCookie != TF_INVALID_COOKIE)
	{
		if (m_pBaseContext && SUCCEEDED(m_pBaseContext->QueryInterface(&source)))
		{
			HRESULT hr = source->UnadviseSink(m_textEditSinkCookie);
			SAFE_RELEASE(source);
			SAFE_RELEASE(m_pBaseContext);
			m_textEditSinkCookie = TF_INVALID_COOKIE;
		}
	}
	if (m_pThreadMgrEx && SUCCEEDED(m_pThreadMgrEx->QueryInterface(__uuidof(ITfSource), (void**)&source)))
	{
		source->UnadviseSink(m_uiElementSinkCookie);
		source->UnadviseSink(m_inputProfileSinkCookie);
		source->UnadviseSink(m_threadMgrEventSinkCookie);
		m_pThreadMgrEx->Deactivate();
		SAFE_RELEASE(source);
		SAFE_RELEASE(m_pThreadMgr);
		SAFE_RELEASE(m_pProfileMgr);
		SAFE_RELEASE(m_pProfiles);
		SAFE_RELEASE(m_pThreadMgrEx);
	}
	CoUninitialize();
}

STDAPI CiceroInputMethod::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_INVALIDARG;
	*ppvObj = nullptr;
	if (IsEqualIID(riid, IID_IUnknown))
		*ppvObj = reinterpret_cast<IUnknown*>(this);
	else if (IsEqualIID(riid, IID_ITfUIElementSink))
		*ppvObj = (ITfUIElementSink *)this;
	else if (IsEqualIID(riid, IID_ITfInputProcessorProfileActivationSink))
		*ppvObj = (ITfInputProcessorProfileActivationSink*)this;
	else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
		*ppvObj = (ITfThreadMgrEventSink*)this;
	else if (IsEqualIID(riid, IID_ITfTextEditSink))
		*ppvObj = (ITfTextEditSink*)this;
	else
		return E_NOINTERFACE;
	AddRef();
	return S_OK;
}

STDAPI CiceroInputMethod::OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags)
{
	bool bActive = (dwFlags & TF_IPSINK_FLAG_ACTIVE);
	if (!bActive)
		return S_OK;
	this->GetCurrentInputMethodName();
	//unk();
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


STDAPI CiceroInputMethod::OnInitDocumentMgr(ITfDocumentMgr *pdim)
{
	return S_OK;
}

STDAPI CiceroInputMethod::OnUninitDocumentMgr(ITfDocumentMgr *pdim)
{
	return S_OK;
}

STDAPI CiceroInputMethod::OnSetFocus(ITfDocumentMgr *pdimFocus, ITfDocumentMgr *pdimPrevFocus)
{
	if (!pdimFocus)
		return S_OK;
	ITfSource* source = nullptr;
	if (m_textEditSinkCookie != TF_INVALID_COOKIE)
	{
		if (m_pBaseContext && SUCCEEDED(m_pBaseContext->QueryInterface(&source)))
		{
			HRESULT hr = source->UnadviseSink(m_textEditSinkCookie);
			SAFE_RELEASE(source);
			if (FAILED(hr))
				return S_OK;
			SAFE_RELEASE(m_pBaseContext);
			m_textEditSinkCookie = TF_INVALID_COOKIE;
		}
	}
	if (SUCCEEDED(pdimFocus->GetBase(&m_pBaseContext)) && SUCCEEDED(m_pBaseContext->QueryInterface(&source)) && SUCCEEDED(source->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink *>(this), &m_textEditSinkCookie))){}
	SAFE_RELEASE(source);
	return S_OK;
}

STDAPI CiceroInputMethod::OnPushContext(ITfContext *pic)
{
	return S_OK;
}
STDAPI CiceroInputMethod::OnPopContext(ITfContext *pic)
{
	return S_OK;
}

STDAPI CiceroInputMethod::OnEndEdit(ITfContext *cxt, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord)//Get composition string...
{
	static DisplayMenuManager* menu = DisplayMenuManager::GetSingleton();
	ITfContextComposition* pContextComposition;
	if (FAILED(cxt->QueryInterface(&pContextComposition)))
		return S_OK;
	IEnumITfCompositionView *pEnumComposition;
	if (FAILED(pContextComposition->EnumCompositions(&pEnumComposition))) 
	{
		pContextComposition->Release();
		return S_OK;
	}
	ITfCompositionView* pCompositionView;
	string result;
	ULONG fetchCount = NULL;
	while (SUCCEEDED(pEnumComposition->Next(1, &pCompositionView, &fetchCount)) && fetchCount > NULL)
	{
		ITfRange* pRange;
		WCHAR buffer[MAX_PATH];
		ULONG bufferSize = NULL;
		if (SUCCEEDED(pCompositionView->GetRange(&pRange)))
		{
			pRange->GetText(ecReadOnly, TF_TF_MOVESTART, buffer, MAX_PATH, &bufferSize);
			buffer[bufferSize] = NULL;
			menu->inputContent = WcharToChar(buffer).get();
			//_MESSAGE("Composition:%s", WcharToChar(buffer).get());
			pRange->Release();
		}
		pCompositionView->Release();
	}
	pEnumComposition->Release();
	pContextComposition->Release();
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
	UINT uIndex = NULL, uCount = NULL, uCurrentPage = NULL, uPageCount = NULL;
	DWORD dwPageStart = NULL, dwPageSize = NULL, dwPageSelection = NULL;
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
	bool result = false;
	GUID clsid = GUID_NULL;
	HRESULT hr = m_pProfiles->ActivateLanguageProfile(clsid, langID, clsid);
	if (SUCCEEDED(hr))
		result = true;
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
