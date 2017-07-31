#include "skse/SafeWrite.h"
#include "skse/PluginAPI.h"
#include "skse/GameThreads.h"
#include "skse/PapyrusUI.h"

#include "Hooks.h"
#include "Thread.h"
#include "InputUtil.h"
#include "HookUtil.h"
#include "InputMenu.h"
#include "Cicero.h"
#include "GameData.h"
#include "Settings.h"

#include <d3d9.h>
#include <dinput.h>
#include <memory>

#define WM_IME_SETSTATE 0x0655

extern  SKSETaskInterface*	g_task;


LRESULT CALLBACK CustomWindowProc_Hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto pInputManager = InputManager::GetSingleton();
	auto pInputMenu = InputMenu::GetSingleton();
	auto pCicero = CiceroInputMethod::GetSingleton();

	switch (uMsg)
	{
	case WM_ACTIVATE:
	{
		if (Settings::iDoubleCursorFix)
		{
			(LOWORD(wParam) > 0) ? InputUtil::ShowCursorEx(false) : InputUtil::ShowCursorEx(true);
		}
		break;
	}
	case WM_IME_NOTIFY:
	{
		switch (wParam)
		{
		case IMN_OPENCANDIDATE:
		case IMN_SETCANDIDATEPOS:
		case IMN_CHANGECANDIDATE:
			InterlockedExchange(&pInputMenu->enableState, 1);
			if (pInputManager->allowTextInput && !pCicero->m_ciceroState)
			{
				InputUtil::GetCandidateList(hWnd);
			}
		}
		return 0;
	}
	case WM_IME_STARTCOMPOSITION:
	{
		if (pInputManager->allowTextInput)
		{
			InterlockedExchange(&pInputMenu->enableState, 1);
			InterlockedExchange(&pInputMenu->disableKeyState, 1);

			if (!pCicero->m_ciceroState)
			{
				auto f = [=](UInt32 time)->bool {std::this_thread::sleep_for(std::chrono::milliseconds(time)); InputUtil::GetCandidateList(hWnd); return true; };
				really_async(f, 200);
				really_async(f, 300);
			}
		}
		return 0;
	}
	case WM_IME_COMPOSITION:
	{
		if (pInputManager->allowTextInput)
		{
			if (lParam & CS_INSERTCHAR) {}
			if (lParam & GCS_CURSORPOS) {}
			if (lParam & GCS_COMPSTR)
				InputUtil::GetInputString(hWnd);
			if (lParam & GCS_RESULTSTR)
				InputUtil::GetResultString(hWnd);
		}
		return 0;
	}
	case WM_IME_ENDCOMPOSITION:
	{
		InterlockedExchange(&pInputMenu->enableState, 0);

		g_criticalSection.Enter();
		pInputMenu->candidateList.clear();
		pInputMenu->inputContent.clear();
		g_criticalSection.Leave();

		if (pInputManager->allowTextInput)
		{
			auto f = [=](UInt32 time)->bool {
				std::this_thread::sleep_for(std::chrono::milliseconds(time));
				if (!InterlockedCompareExchange(&pInputMenu->enableState, pInputMenu->enableState, 2))
				{
					InterlockedExchange(&pInputMenu->disableKeyState, 0);
				}
				return true; };
			really_async(f, 150);
		}
		return 0;
	}
	case WM_IME_SETSTATE:
		if (lParam)
			ImmAssociateContextEx(pInputMenu->pHandle, NULL, IACE_DEFAULT);
		else
			ImmAssociateContextEx(pInputMenu->pHandle, NULL, NULL);
		break;
	case WM_CHAR:
	{
		if (pInputManager->allowTextInput)
		{
			InputUtil::SendUnicodeMessage(wParam);
		}
		return 0;
	}
	case WM_IME_SETCONTEXT:
		return DefWindowProc(hWnd, WM_IME_SETCONTEXT, wParam, NULL);
	}
	return ((WNDPROC)0x006940F0)(hWnd, uMsg, wParam, lParam);
}


void __stdcall ProcessAllowTextInput(bool increase)
{
	auto pInputManager = InputManager::GetSingleton();
	auto pInputMenu = InputMenu::GetSingleton();

	UInt8 currentCount = pInputManager->allowTextInput;

	if ((!increase) && (currentCount == 1))
	{
		PostMessage(pInputMenu->pHandle, WM_IME_SETSTATE, NULL, 0);

		g_criticalSection.Enter();
		pInputMenu->candidateList.clear();
		pInputMenu->inputContent.clear();
		g_criticalSection.Leave();

		InterlockedExchange(&pInputMenu->enableState, 0);
	}
	else if ((increase) && (currentCount == 0))
	{
		PostMessage(pInputMenu->pHandle, WM_IME_SETSTATE, NULL, 1);
	}
}

void __fastcall Hooked_AllowTextInput(void* unk1, void* unk2, GFxFunctionHandler::Args* args)
{
	ASSERT(args->numArgs >= 1);

	bool enable = args->args[0].GetBool();
	auto pInputManager = InputManager::GetSingleton();

	if (pInputManager)
	{
		ProcessAllowTextInput(enable);
		pInputManager->AllowTextInput(enable);
	}
}

void __stdcall CheckFunctionHandler(GFxFunctionHandler* callback)
{
	char* className = "class SKSEScaleform_AllowTextInput";
	static bool result = false;
	if (!result && strcmp(typeid(*callback).name(), className) == 0)
	{
		SafeWrite32((UInt32)(*(UInt32**)callback + 0x1), (UInt32)Hooked_AllowTextInput);
		result = true;
	}
}


struct UIManagerEx
{
	typedef void(__thiscall UIManagerEx::*FnAddUIMessage)(BSFixedString& strData, UInt32 msgID, void* objData);
	static FnAddUIMessage fnAddUIMessage;

	void AddUIMessage_Hook(BSFixedString& strData, UInt32 msgID, void* objData)
	{
		if (msgID == UIMessageEx::kMessage_Scaleform && objData != nullptr)
		{
			BSUIScaleformData* scaleformData = reinterpret_cast<BSUIScaleformData*>(objData);
			GFxEvent* event = scaleformData->event;
			if (event->type == GFxEvent::KeyDown && InputManager::GetSingleton()->allowTextInput)
			{
				InputMenu* mm = InputMenu::GetSingleton();
				GFxKeyEvent* key = static_cast<GFxKeyEvent*>(event);
#ifdef _DEBUG
				_MESSAGE("[SCALEFORM]    EVENT: KEYDOWN    TARGET: %s    DATA: %p    KEYDATA: %p    KEYCODE: %d", strData.data, objData, key, key->keyCode);
				_MESSAGE("");
#endif // DEBUG
				if (InterlockedCompareExchange(&mm->disableKeyState, mm->disableKeyState, 2))
				{
					static std::vector<UInt8> invalidKeys{ 0x0D, 0x08, 0x6C, 0x25, 0x26, 0x27, 0x28, 0x1B };
					for (auto& element : invalidKeys)
					{
						if (element == key->keyCode)
						{
							FormHeap_Free(objData);
							return;
						}
					}
				}
			}
			else if (event->type == GFxEvent::CharEvent)
			{
				GFxCharEvent* charEvent = static_cast<GFxCharEvent*>(event);
#ifdef _DEBUG
				_MESSAGE("[SCALEFORM]    EVENT: CHAR	   TARGET: %s    DATA: %p    CHARDATA: %p    KEYBOARDINDEX: %d    WCHARCODE: %d", strData.data, objData, charEvent, charEvent->keyboardIndex, charEvent->wcharCode);
				_MESSAGE("");
#endif // _DEBUG
				FormHeap_Free(objData);
				return;
			}
		}

		UIManager* ui = UIManager::GetSingleton();
		if (ui != nullptr)
		{
			CALL_MEMBER_FN(ui, AddMessage)(&strData, UIMessageEx::kMessage_Scaleform, objData);
		}
	}

	static void InitHook()
	{
		fnAddUIMessage = HookUtil::WriteRelCall(0x00A5C261, &AddUIMessage_Hook);
	}
};
UIManagerEx::FnAddUIMessage UIManagerEx::fnAddUIMessage = nullptr;




class Main
{
public:
	typedef Main*(__thiscall Main::*FnConstructor)(HWND, HINSTANCE);
	static FnConstructor fnConstructor;

	Main* Constructor_Hook(HWND hWnd, HINSTANCE	hInstance)
	{
		auto pInputMenu = InputMenu::GetSingleton();

		pInputMenu->pHandle = hWnd;

		CiceroInputMethod::GetSingleton()->SetupSinks();
		ImmAssociateContextEx(hWnd, 0, 0);

		return (this->*fnConstructor)(hWnd, hInstance);
	}

	static void InitHook()
	{
		fnConstructor = HookUtil::WriteRelCall(0x0069D89F, &Constructor_Hook);
	}
};
Main::FnConstructor		Main::fnConstructor = nullptr;




class RenderingDevice
{
public:
	using FnReset = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS *);
	using FnPresent = HRESULT(__stdcall*)(IDirect3DDevice9*, CONST RECT *, CONST RECT *, HWND, CONST RGNDATA *);

	static FnReset			fnReset;
	static FnPresent		fnPresent;

	__declspec(nothrow) static HRESULT __stdcall Reset_Hook(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS * pPresentationParameters)
	{
		InputMenu::GetSingleton()->OnLostDevice();
		HRESULT hr = fnReset(pDevice, pPresentationParameters);

		if (SUCCEEDED(hr))
		{
			InputMenu::GetSingleton()->OnResetDevice();
		}
		return hr;
	}

	__declspec(nothrow) static HRESULT __stdcall Present_Hook(IDirect3DDevice9* pDevice, CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
	{
		InputMenu::GetSingleton()->NextFrame();
		return fnPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}


	static void InitHook(IDirect3DDevice9* pDevice)
	{
		if (pDevice != nullptr)
		{
			UInt32 fnResetResult = HookUtil::SafeWrite32(*reinterpret_cast<UInt32*>(pDevice) + 0x04 * 0x10, reinterpret_cast<UInt32>(Reset_Hook));
			if (fnResetResult != reinterpret_cast<UInt32>(Reset_Hook))
			{
				fnReset = reinterpret_cast<FnReset>(fnResetResult);
			}
			UInt32 fnPresentResult = HookUtil::SafeWrite32(*reinterpret_cast<UInt32*>(pDevice) + 0x04 * 0x11, reinterpret_cast<UInt32>(Present_Hook));
			if (fnPresentResult != reinterpret_cast<UInt32>(Present_Hook))
			{
				fnPresent = reinterpret_cast<FnPresent>(fnPresentResult);
			}
		}
	}
};
RenderingDevice::FnReset		RenderingDevice::fnReset = nullptr;
RenderingDevice::FnPresent		RenderingDevice::fnPresent = nullptr;




void __stdcall RedirectDeviceFunction(IDirect3DDevice9* pDevice)
{
	if (pDevice != nullptr)
	{
		InputMenu::GetSingleton()->Initialize(pDevice);
		RenderingDevice::InitHook(pDevice);
	}
}



class MenuManagerEx : public MenuManager
{
public:
	bool StopDrawingWorld_Hook()
	{
		typedef bool(__fastcall* StopDrawingWorld)(MenuManagerEx*, void*);
		bool result = ((StopDrawingWorld)0x00A5C7D0)(this, nullptr);
		void* pRenderer = *((void**)0x01B913F0);
		if (!result && pRenderer != nullptr)
		{
			IDirect3DDevice9* pDevice = *reinterpret_cast<IDirect3DDevice9**>((UInt32)pRenderer + 0x38);
			if (pDevice != nullptr)
			{
				HRESULT hr = pDevice->TestCooperativeLevel();
				if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET)
				{
					result = true;
				}
			}
		}
		return result;
	}

	static void InitHook()
	{
		HookUtil::WriteRelCall(0x0069C997, &StopDrawingWorld_Hook);
		HookUtil::WriteRelCall(0x0069CC8E, &StopDrawingWorld_Hook);
		HookUtil::WriteRelCall(0x0069D07B, &StopDrawingWorld_Hook);
	}
};




__declspec(naked) void Hooked_CheckString()
{
	__asm
	{
		pop eax
		mov eax, 0x1
		pop edi
		pop esi
		pop ebp
		pop ebx
		retn 0x4
	}
}




UInt32 kOriginalAllowTextInputHook_Ent;
UInt32 kOriginalAllowTextInputHook_Ret;

__declspec(naked) void Hooked_OriginalAllowTextInput()
{
	__asm
	{
		mov al, byte ptr[esi + 0x98]
		pushfd
		pushad
		push ebx
		call ProcessAllowTextInput
		popad
		popfd
		jmp [kOriginalAllowTextInputHook_Ret]
	}
}




const UInt32 kCustomAllowTextInputHook_Ent = 0x00A66B40;
const UInt32 kCustomAllowTextInputHook_Ret = 0x00A66B45;

__declspec(naked) void Hooked_CustomAllowTextInput()
{
	__asm
	{
		cmp byte ptr[esp + 0x4], 0x0
		mov eax, [esp + 0x4]
		pushfd
		pushad
		push eax
		call ProcessAllowTextInput
		popad
		popfd
		jmp [kCustomAllowTextInputHook_Ret]
	}
}




const UInt32 kCreateFunctionHook_Ent = 0x00922DC0;
const UInt32 kCreateFunctionHook_Ret = 0x00922DC5;

__declspec(naked) void Hooked_CreateFunction()
{
	__asm
	{
		push ecx
		mov ecx, [esp + 0xC]
		pushad
		push ecx
		call CheckFunctionHandler
		popad
		pop ecx
		sub esp, 0x1C
		push ebx
		push ebp
		jmp [kCreateFunctionHook_Ret]
	}
}




const UInt32 kRendererInitialize_Ent = 0x00CD8C6B;
const UInt32 kRendererInitialize_Ret = 0x00CD8C71;
const UInt32 kRendererInitialize_Jmp = 0x00CD8C94;

__declspec(naked) void RendererInitialize_Hook()
{
	__asm
	{
		mov eax, dword ptr [esi + 0x38]
		push eax
		call RedirectDeviceFunction
		cmp dword ptr[esi + 0x38], 0
		jz NODEVICE
		jmp [kRendererInitialize_Jmp]

	NODEVICE:
		jmp [kRendererInitialize_Ret]
	}
}



void Hook_TextInput_Commit()
{
	UIManagerEx::InitHook();
	Main::InitHook();

	HookUtil::SafeWrite32(0x0069D7E7, (UInt32)CustomWindowProc_Hook);

	//Fix ShowRaceMenu...
	SafeWrite8(0x0088740A, 0x00);
	//Fix EnchanmentMenu...
	SafeWrite8(0x008522B7, 0x00);
	//*((GFxLoader **)0x01B2E9B0);

	//call TESV.00A60670  bool GFxLoader::CheckString(char*);
	WriteRelJump(0x00A6068C, (UInt32)Hooked_CheckString);


	SafeWrite8(0x00A6B0AD, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY);  _MESSAGE("[CI] Non-exclusive input mode...");

	WriteRelJump(kCreateFunctionHook_Ent, (UInt32)Hooked_CreateFunction);

	WriteRelJump(kCustomAllowTextInputHook_Ent, (UInt32)Hooked_CustomAllowTextInput);

	WriteRelJump(kRendererInitialize_Ent, (UInt32)RendererInitialize_Hook);
	SafeWrite8(kRendererInitialize_Ent + 5, 0x90);

	_MESSAGE("[CI] Write D3D device hooks...");

	constexpr char* configFile = "Data\\SKSE\\Plugins\\ChineseInput.ini";
	constexpr char* featureSection = "Features";

	if (GetPrivateProfileInt(featureSection, "iFixPrecacheCharGen", 1, configFile))
	{
		SafeWrite8(0x008868CB, 0xEB); _MESSAGE("[CI] PrecacheCharGen disabled...");
		SafeWrite16(0x00886B57, 0x68EB); _MESSAGE("[CI] PrecacheCharGenClear disabled...");
	}

	if (GetPrivateProfileInt(featureSection, "iSkipBethesdaIntroMovie", 1, configFile))
	{
		SafeWrite32(0x01272B38, 0x00); _MESSAGE("[CI] Intro disabled...");
	}

	if (GetPrivateProfileInt(featureSection, "iRemoveFogOnLoadingMenu", 1, configFile))
	{
		SafeWrite32(0x0087C370, 0x00); _MESSAGE("[CI] Fog disabled...");
	}

	if (GetPrivateProfileInt(featureSection, "iEnableBorderlessWindow", 1, configFile))
	{
		//Hook window mode...
		SafeWrite32(0x012CF5F8, 0x00); //X
		SafeWrite32(0x012CF604, 0x00); //Y
		SafeWrite32(0x0069D832, WS_POPUP);
		SafeWrite32(0x0069D877, WS_POPUP);

		_MESSAGE("[CI] Borderless window enabled...");
	}

	constexpr char* settingSection = "Settings";
	Settings::iOffsetX = GetPrivateProfileInt(settingSection, "iOffsetX", 360, configFile);
	Settings::iOffsetY = GetPrivateProfileInt(settingSection, "iOffsetY", 80, configFile);
	Settings::iDoubleCursorFix = GetPrivateProfileInt(settingSection, "iDoubleCursorFix", 1, configFile);

	char sResult_u[100];
	GetPrivateProfileString(settingSection, "sFontName", "»ªÎÄ¿¬Ìå", sResult_u, 100, configFile);
	char sResult_a[100];
	InputUtil::Convert(sResult_u, sResult_a, CP_UTF8, CP_ACP);

	Settings::sFontName = sResult_a;
}














































