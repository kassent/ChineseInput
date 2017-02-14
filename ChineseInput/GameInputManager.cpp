#pragma comment (lib, "imm32.lib")
#include "GameInputManager.h"
#include "DisplayMenuManager.h"
#include "CiceroInputMethod.h"
#include "D3D9Interface.h"
#include <memory>
#include <future>
#include "MinHook.h"
#include "imm.h"
#define WINDOW_NAME "Skyrim"

template <typename F, typename... Args>
auto really_async(F&& f, Args&&... args)-> std::future<typename std::result_of<F(Args...)>::type>
{
	using _Ret = typename std::result_of<F(Args...)>::type;
	auto _func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
	std::packaged_task<_Ret()> task(std::move(_func));
	auto _fut = task.get_future();
	std::thread thread(std::move(task));
	thread.detach();
	return _fut;
}

void* UIMessageEx::CreateUIMessageData(const BSFixedString &name)
{
	typedef void * (*_CreateUIMessageData)(const BSFixedString &name);
	const _CreateUIMessageData CreateUIMessageData = (_CreateUIMessageData)0x00547A00;
	return CreateUIMessageData(name);
}


LRESULT CALLBACK Hooked_CustomWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static GameInputManager* manager = GameInputManager::GetSingleton();
	static InputManager* input = InputManager::GetSingleton();
	static DisplayMenuManager* mm = DisplayMenuManager::GetSingleton();
	static CiceroInputMethod* cicero = CiceroInputMethod::GetSingleton();
	switch (uMsg)
	{
		case WM_ACTIVATE:
			{
				if (LOWORD(wParam) > 0)
					manager->ShowCursor(false);
				else
					manager->ShowCursor(true);
			}
			break;
		case WM_IME_NOTIFY:
			switch (wParam)
			{
				case IMN_OPENCANDIDATE:
				case IMN_SETCANDIDATEPOS:
				case IMN_CHANGECANDIDATE:
					mm->enableState = true;
					if (!cicero->m_ciceroState)
						GameInputManager::GetCandidateList(hWnd);
			}
			return NULL;
		case WM_IME_STARTCOMPOSITION:
			if (input->allowTextInput)
			{
				mm->enableState = true;
				mm->disableKeyState = true;
				if (!cicero->m_ciceroState)
				{
					auto f = [=](UInt32 time)->bool{std::this_thread::sleep_for(std::chrono::milliseconds(time)); GameInputManager::GetCandidateList(hWnd); return true; };
					really_async(f, 200);
					really_async(f, 300);
				}
			}
			return NULL;
		case WM_IME_COMPOSITION:
			{
				if (input->allowTextInput)
				{
					if (lParam & CS_INSERTCHAR){}
					if (lParam & GCS_CURSORPOS){}
					if (lParam & GCS_COMPSTR)
						GameInputManager::GetInputString(hWnd);
					if (lParam & GCS_RESULTSTR)
						GameInputManager::GetResultString(hWnd);
				}
			}
			return NULL;
		case WM_IME_ENDCOMPOSITION:
			{
				mm->enableState = false;
				mm->candidateList.clear();
				mm->inputContent.clear();
				if (input->allowTextInput)
				{
					auto f = [](UInt32 time)->bool{std::this_thread::sleep_for(std::chrono::milliseconds(time)); if (!mm->enableState){ mm->disableKeyState = false; } return true; };
					really_async(f, 150);
				}
			}
			return NULL;
		case WM_IME_SETSTATE:
			if (lParam)
				ImmAssociateContextEx(manager->m_hWnd, NULL, IACE_DEFAULT);
			else
				ImmAssociateContextEx(manager->m_hWnd, NULL, NULL);
			break;
		case WM_CHAR:
			{
				if (input->allowTextInput)
					manager->SendUnicodeMessage(wParam);
				return DefWindowProc(hWnd, WM_CHAR, wParam, lParam);
			}
		case WM_IME_SETCONTEXT:
			return DefWindowProc(hWnd, WM_IME_SETCONTEXT, wParam, NULL);
	}
	return CallWindowProc(manager->m_oldWndProc, hWnd, uMsg, wParam, lParam);
}

HWND WINAPI Hooked_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	_MESSAGE(__FUNCTION__);
	GameInputManager* manager = GameInputManager::GetSingleton();
	HWND hWnd = manager->m_oldCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

	std::unique_ptr<char[]> className(new char[MAX_PATH]);
	std::unique_ptr<char[]> windowName(new char[MAX_PATH]);

	GetClassNameA(hWnd, className.get(), MAX_PATH);
	GetWindowTextA(hWnd, windowName.get(), MAX_PATH);
	static const char* targetName = WINDOW_NAME;
	_MESSAGE("Window 0x%p: ClassName \"%s\", WindowName: \"%s\"", hWnd, className, windowName);
	if (strcmp(className.get(), targetName) == 0 && strcmp(windowName.get(), targetName) == 0)
	{
		manager->m_hWnd = hWnd;
		manager->m_newWndProc = (WNDPROC)Hooked_CustomWndProc;
		manager->m_oldWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)Hooked_CustomWndProc);
		_MESSAGE("WNDPROC = %p", manager->m_oldWndProc);
		CiceroInputMethod::GetSingleton()->SetupSinks();
		ImmAssociateContextEx(hWnd, NULL, NULL);
	}
	return hWnd;
}

IDirect3D9* WINAPI Hooked_Direct3DCreate9(UINT SDKVersion)
{
	GameInputManager* manager = GameInputManager::GetSingleton();
	IDirect3D9* directX = manager->m_oldDirect3DCreate9(SDKVersion);
	_MESSAGE("D3Device:%p", directX);
	TESRDirect3D9* m_directX = new TESRDirect3D9(directX);
	return m_directX;
}

GameInputManager::GameInputManager()
{
	_MESSAGE(__FUNCTION__);
	MH_Initialize();
}

bool GameInputManager::InstallDriverHooks()
{
	_MESSAGE(__FUNCTION__);

	m_newCreateWindowExA = (CreateWindowExA)Hooked_CreateWindowExA;
	MH_CreateHook(::CreateWindowExA, Hooked_CreateWindowExA, reinterpret_cast<void**>(&m_oldCreateWindowExA));

	m_newDirect3DCreate9 = (Direct3DCreate9)Hooked_Direct3DCreate9;
	MH_CreateHookApiEx(L"d3d9.dll", "Direct3DCreate9", Hooked_Direct3DCreate9, reinterpret_cast<void**>(&m_oldDirect3DCreate9), NULL);

	bool result = MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
	if (result)
		_MESSAGE("Install driver hooks sucessfully...");
	return result;
}

GameInputManager::~GameInputManager()
{
	_MESSAGE(__FUNCTION__);
	MH_Uninitialize();
}

void GameInputManager::GetCandidateList(const HWND& hWnd)
{
	DisplayMenuManager* mm = DisplayMenuManager::GetSingleton();
	if (!mm->enableState)
		return;
	HIMC hIMC;
	DWORD dwBufLen;
	UINT i;
	LPCANDIDATELIST pList;
	hIMC = ImmGetContext(hWnd);
	if (!hIMC)
		return;
	dwBufLen = ImmGetCandidateList(hIMC, 0, NULL, 0);
	if (!dwBufLen)
	{
		ImmReleaseContext(hWnd, hIMC);
		return;
	}
	pList = (LPCANDIDATELIST)malloc(dwBufLen);
	if (!pList)
	{
		ImmReleaseContext(hWnd, hIMC);
		return;
	}
	ImmGetCandidateList(hIMC, 0, pList, dwBufLen);
	if (pList->dwStyle != IME_CAND_CODE)
	{
		mm->candidateList.clear();
		char buffer[0x8];
		mm->selectedIndex = pList->dwSelection;
		mm->pageSize = pList->dwPageSize;
		mm->pageStartIndex = pList->dwPageStart;
		for (i = 0; i < pList->dwCount && i < pList->dwPageSize; i++)
		{
			sprintf(buffer, "%d.", i + 1);
			TCHAR* pSubStr = (TCHAR*)((BYTE*)pList + pList->dwOffset[i + mm->pageStartIndex]);
			string temp(buffer);
			temp += pSubStr;
			mm->candidateList.push_back(temp);
		}
		//_MESSAGE("selectedIndex = %d, pageSize = %d, pageStartIndex = %d", mm->selectedIndex, mm->pageSize, mm->pageStartIndex);
	}
	free(pList);
	ImmReleaseContext(hWnd, hIMC);
}

void GameInputManager::GetResultString(const HWND& hWnd)
{
	static GameInputManager* manager = GameInputManager::GetSingleton();
	HIMC hIMC;
	DWORD bufferSize;
	std::unique_ptr<TCHAR[]> buffer(new TCHAR[MAX_PATH]);
	hIMC = ImmGetContext(hWnd);
	if (hIMC)
	{
		bufferSize = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
		bufferSize += sizeof(WCHAR);
		memset(buffer.get(), 0, MAX_PATH);
		ImmGetCompositionString(hIMC, GCS_RESULTSTR, buffer.get(), bufferSize);
		UInt32 len = ::MultiByteToWideChar(CP_ACP, NULL, buffer.get(), -1, NULL, 0);
		std::unique_ptr<WCHAR[]> wcString(new WCHAR[2 * len + 2]);
		memset(wcString.get(), 0, 2 * len + 2);
		::MultiByteToWideChar(CP_ACP, 0, buffer.get(), -1, wcString.get(), len);
		UInt32 count = NULL;
		for (size_t i = 0; i < len; i++)
		{
			UInt16 unicode = (UInt16)(wcString[i]);
			if (unicode > NULL)
				manager->SendUnicodeMessage(unicode);
		}
	}
	ImmReleaseContext(hWnd, hIMC);
}

void GameInputManager::GetInputString(const HWND& hWnd)
{
	static DisplayMenuManager* mm = DisplayMenuManager::GetSingleton();
	HIMC hIMC = ImmGetContext(hWnd);
	UINT uLen, uMem;
	uLen = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
	if (uLen)
	{
		std::unique_ptr<TCHAR[]> szCompStr(new TCHAR[uMem = uLen + 1]);
		if (szCompStr)
		{
			szCompStr[uLen] = 0;
			ImmGetCompositionString(hIMC, GCS_COMPSTR, szCompStr.get(), uMem);
			mm->inputContent = szCompStr.get();
		}
	}
	ImmReleaseContext(hWnd, hIMC);
}

void GameInputManager::ShowCursor(bool show)
{
	_MESSAGE("%s mouse cursor", show ? "Show" : "Hide");
	if (show)
		while (::ShowCursor(TRUE) < 0);
	else
		while (::ShowCursor(FALSE) > -1);
}

GameInputManager* GameInputManager::GetSingleton()
{
	static GameInputManager instance;
	return &instance;
}

bool GameInputManager::SendUnicodeMessage(UInt32 wcharCode)
{
	static UIStringHolder* stringHolder = UIStringHolder::GetSingleton();
	static InputManager* mm = InputManager::GetSingleton();
	if (mm->allowTextInput)
	{
		BSUIScaleformData* msgData = (BSUIScaleformData*)UIMessageEx::CreateUIMessageData(stringHolder->bsUIScaleformData);
		msgData->event = m_buffer.emplace_back(wcharCode, IME_CHAR);
		//static GFxCharEvent* charEvent = (GFxCharEvent*)FormHeap_Allocate(sizeof(GFxCharEvent));
		//charEvent->type = GFxEvent::CharEvent;
		//charEvent->wcharCode = wcharCode;
		//charEvent->keyboardIndex = IME_CHAR;
		//msgData->event = &m_buffer.back();
		BSFixedString menuName = stringHolder->topMenu;
		CALL_MEMBER_FN(UIManager::GetSingleton(), AddMessage)(&menuName, UIMessageEx::kMessage_Scaleform, msgData);//How to FormHeap_Free?
		return true;
	}
	return false;
}


