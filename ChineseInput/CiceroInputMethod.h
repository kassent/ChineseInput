#pragma once
#include <windows.h>
#include <msctf.h>

#define MAX_CANDLIST 10
#define ENG_KB 0x0409
#define CHS_KB 0x0804


class CiceroInputMethod : public ITfUIElementSink, public ITfInputProcessorProfileActivationSink, public ITfTextEditSink, public ITfThreadMgrEventSink
{
public:
	using COOKIE = DWORD;
	STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);

	STDMETHODIMP BeginUIElement(DWORD dwUIElementId, BOOL *pbShow);
	STDMETHODIMP UpdateUIElement(DWORD dwUIElementId);
	STDMETHODIMP EndUIElement(DWORD dwUIElementId);

	STDMETHODIMP OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags);

	STDMETHODIMP OnEndEdit(ITfContext *cxt, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord);

	STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr *pdim);
	STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr *pdim);
	STDMETHODIMP OnSetFocus(ITfDocumentMgr *pdimFocus, ITfDocumentMgr *pdimPrevFocus);
	STDMETHODIMP OnPushContext(ITfContext *pic);
	STDMETHODIMP OnPopContext(ITfContext *pic);

	CiceroInputMethod();
	~CiceroInputMethod();

	BOOL SetupSinks();
	void ReleaseSinks();

	ITfUIElement* GetUIElement(DWORD dwUIElementId);
	void GetCandidateStrings(ITfCandidateListUIElement* pcandidate);
	bool DisableInputMethod(LANGID langID);
	bool GetLayoutName(const WCHAR* kl, WCHAR* nm);
	void GetCurrentInputMethodName();

	static CiceroInputMethod* GetSingleton();

	LONG							m_refCount;
	BOOL							m_ciceroState;
	COOKIE							m_uiElementSinkCookie;
	COOKIE							m_inputProfileSinkCookie;
	COOKIE							m_threadMgrEventSinkCookie;
	COOKIE							m_textEditSinkCookie;
	TfClientId						m_clientID;
	ITfContext*						m_pBaseContext;
	ITfThreadMgr*					m_pThreadMgr;
	ITfThreadMgrEx*					m_pThreadMgrEx;
	ITfInputProcessorProfiles*		m_pProfiles;
	ITfInputProcessorProfileMgr*	m_pProfileMgr;
};


