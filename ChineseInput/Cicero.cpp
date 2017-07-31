#include "Cicero.h"
#include "InputMenu.h"
#include "GameData.h"

#include <memory>
#include <windows.h>

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


CiceroInputMethod* CiceroInputMethod::GetSingleton()
{
	static CiceroInputMethod instance;
	return &instance;
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
	LONG result = --m_refCount;
	if (result == 0)
		delete this;
	return result;
}

BOOL CiceroInputMethod::SetupSinks()
{
	HRESULT hr;
	CoInitialize(NULL);
	hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, __uuidof(ITfThreadMgrEx), (void**)&m_pThreadMgrEx);
	if (FAILED(hr) || FAILED(m_pThreadMgrEx->ActivateEx(&m_clientID, TF_TMAE_UIELEMENTENABLEDONLY)))
		return false;
	ITfSource* source = nullptr;
	if (SUCCEEDED(hr = m_pThreadMgrEx->QueryInterface(__uuidof(ITfSource), (void **)&source)))
	{
		source->AdviseSink(__uuidof(ITfUIElementSink), (ITfUIElementSink*)this, &m_uiElementSinkCookie);
		source->AdviseSink(__uuidof(ITfInputProcessorProfileActivationSink), (ITfInputProcessorProfileActivationSink*)this, &m_inputProfileSinkCookie);
		source->AdviseSink(__uuidof(ITfThreadMgrEventSink), (ITfThreadMgrEventSink*)this, &m_threadMgrEventSinkCookie);

		source->Release();
	}
	if(FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (LPVOID*)&m_pProfiles)))
		return false;
	m_pProfiles->QueryInterface(IID_ITfInputProcessorProfileMgr, (void **)&m_pProfileMgr);

	GetCurrentInputMethodName();

	_MESSAGE("[CI] Enable cicero input...");

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
	if (!(dwFlags & TF_IPSINK_FLAG_ACTIVE))
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
	InputMenu* mm = InputMenu::GetSingleton();
	InterlockedExchange(&mm->enableState, 1);

	ITfUIElement* pElement = GetUIElement(dwUIElementId);
	if (!pElement)
		return E_INVALIDARG;
	*pbShow = FALSE;
	ITfCandidateListUIElement* lpCandidate = nullptr;
	if (SUCCEEDED(pElement->QueryInterface(__uuidof(ITfCandidateListUIElement), (void**)&lpCandidate)))
	{
		this->GetCandidateStrings(lpCandidate);
		lpCandidate->Release();
	}
	pElement->Release();
	return S_OK;
}

STDAPI CiceroInputMethod::UpdateUIElement(DWORD dwUIElementId)
{
	//_MESSAGE("NOTIFY: UDAPATE UIELEMENT");
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
	ITfContextComposition* pContextComposition;
	if (FAILED(cxt->QueryInterface(&pContextComposition)))
		return S_OK;
	IEnumITfCompositionView *pEnumComposition;
	if (FAILED(pContextComposition->EnumCompositions(&pEnumComposition))) 
	{
		pContextComposition->Release();
		return S_OK;
	}
	ITfCompositionView* pCompositionView = nullptr;
	std::string result;
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

			int len = WideCharToMultiByte(CP_ACP, 0, buffer, wcslen(buffer), NULL, 0, NULL, NULL);
			std::unique_ptr<char[], void(__cdecl*)(void*)> resultBuffer(reinterpret_cast<char*>(FormHeap_Allocate(len + 1)), FormHeap_Free);

			WideCharToMultiByte(CP_ACP, 0, buffer, wcslen(buffer), resultBuffer.get(), len, NULL, NULL);
			resultBuffer[len] = '\0';
			InputMenu* menu = InputMenu::GetSingleton();

			g_criticalSection.Enter();
			menu->inputContent = resultBuffer.get();
			g_criticalSection.Leave();

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
	ITfUIElementMgr* pElementMgr = nullptr;
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
	InputMenu* mm = InputMenu::GetSingleton();

	if (InterlockedCompareExchange(&mm->enableState, mm->enableState, 2))
	{
		UINT uIndex = 0, uCount = 0, uCurrentPage = 0, uPageCount = 0;
		DWORD dwPageStart = 0, dwPageSize = 0, dwPageSelection = 0;

		WCHAR* result = nullptr;

		lpCandidate->GetSelection(&uIndex);
		lpCandidate->GetCount(&uCount);
		lpCandidate->GetCurrentPage(&uCurrentPage);

		dwPageSelection = static_cast<DWORD>(uIndex);
		lpCandidate->GetPageIndex(nullptr, 0, &uPageCount);
		if (uPageCount > 0)
		{
			std::unique_ptr<UINT[], void(__cdecl*)(void*)> indexList(reinterpret_cast<UINT*>(FormHeap_Allocate(sizeof(UINT) * uPageCount)), FormHeap_Free);
			lpCandidate->GetPageIndex(indexList.get(), uPageCount, &uPageCount);
			dwPageStart = indexList[uCurrentPage];
			dwPageSize = (uCurrentPage < uPageCount - 1) ? min(uCount, indexList[uCurrentPage + 1]) - dwPageStart : uCount - dwPageStart;
		}
		dwPageSize = min(dwPageSize, MAX_CANDLIST);
		if(dwPageSize)
			InterlockedExchange(&mm->disableKeyState, 1);

		char buffer[8];

		InterlockedExchange(&mm->selectedIndex, dwPageSelection);
		InterlockedExchange(&mm->pageStartIndex, dwPageStart);

		g_criticalSection.Enter();
		mm->candidateList.clear();
		for (size_t i = 0; i < dwPageSize; i++)
		{
			if (SUCCEEDED(lpCandidate->GetString(i + dwPageStart, &result)))
			{
				if (result != nullptr)
				{
					sprintf_s(buffer, "%d.", i + 1);
					std::string temp(buffer);

					int len = WideCharToMultiByte(CP_ACP, 0, result, wcslen(result), NULL, 0, NULL, NULL);
					std::unique_ptr<char[], void(__cdecl*)(void*)> resultBuffer(reinterpret_cast<char*>(FormHeap_Allocate(len + 1)), FormHeap_Free);

					WideCharToMultiByte(CP_ACP, 0, result, wcslen(result), resultBuffer.get(), len, NULL, NULL);
					resultBuffer[len] = '\0';
					temp += resultBuffer.get();

					mm->candidateList.push_back(temp);
					SysFreeString(result);
				}
			}
		}
		g_criticalSection.Leave();
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

	ZeroMemory(lastTipName, sizeof(lastTipName));
	TF_INPUTPROCESSORPROFILE tip;
	m_pProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &tip);
	if (tip.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR)
	{
		WCHAR* bstrImeName = nullptr;
		m_pProfiles->GetLanguageProfileDescription(tip.clsid, tip.langid, tip.guidProfile, &bstrImeName);
		if (bstrImeName != nullptr && wcslen(bstrImeName) < 64)
			wcscpy(lastTipName, bstrImeName);
		if(bstrImeName != nullptr)
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

	int len = WideCharToMultiByte(CP_ACP, 0, lastTipName, wcslen(lastTipName), NULL, 0, NULL, NULL);
	std::unique_ptr<char[], void(__cdecl*)(void*)> resultBuffer(reinterpret_cast<char*>(FormHeap_Allocate(len + 1)), FormHeap_Free);

	WideCharToMultiByte(CP_ACP, 0, lastTipName, wcslen(lastTipName), resultBuffer.get(), len, NULL, NULL);
	resultBuffer[len] = '\0';

	std::string name = resultBuffer.get();

	UInt32 pos = name.find("-", NULL);
	std::string temp("\u72b6\u6001: ");
	if (pos != std::string::npos)
	{
		pos += 2;
		temp.append(name, pos, name.size());
	}
	else
		temp.append(name);

	InputMenu* menu = InputMenu::GetSingleton();

	g_criticalSection.Enter();
	menu->stateInfo = temp;
	g_criticalSection.Leave();

	//_MESSAGE("InputMethodName = %s,size = %d", menu->title.c_str(), menu->title.size());
}
