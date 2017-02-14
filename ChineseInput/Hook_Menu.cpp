#include "Hook_Menu.h"
#include "skse/SafeWrite.h"
#include "GameInputManager.h"
#include "DisplayMenuManager.h"

typedef UInt32(__fastcall *_ProcessMessage)(IMenu* menu, void* unk, UIMessageEx* message);
_ProcessMessage ProcessMessage = (_ProcessMessage)*(reinterpret_cast<UInt32*>(0x010E4BD4) + 0x4);

UInt32 __fastcall Hooked_ProcessMessage(IMenu* menu, void* unk, UIMessageEx* message)
{
	static DisplayMenuManager* manager = DisplayMenuManager::GetSingleton();
	if (message->type == UIMessageEx::kMessage_Scaleform)
	{
		if (menu->view && message->data)
		{
			BSUIScaleformData* scaleformData = (BSUIScaleformData*)message->data;
			GFxEvent* event = scaleformData->event;
			if (event->type == GFxEvent::KeyDown && manager->disableKeyState)
			{
				static std::vector<UInt8> invalidKeys{0x0D, 0x08, 0x6C, 0x25, 0x26, 0x27, 0x28};
				GFxKeyEvent* key = (GFxKeyEvent*)event;
				for (auto& element : invalidKeys)
				{ 
					if (element == key->keyCode) 
						return NULL; 
				}
			}
			else if (event->type == GFxEvent::CharEvent)
			{
				GFxCharEvent* charEvent = (GFxCharEvent*)event;
				if ((!InputManager::GetSingleton()->allowTextInput) || (!charEvent->keyboardIndex) || charEvent->wcharCode < 0x20)
					return NULL;
				if (charEvent->keyboardIndex)
					charEvent->keyboardIndex = NULL;
			}
		}
	}
	return ProcessMessage(menu, NULL, message);
}

void __stdcall ProcessAllowTextInput(bool increase)
{
	static InputManager* inputManager = InputManager::GetSingleton();
	static GameInputManager* manager = GameInputManager::GetSingleton();
	UInt8 currentCount = inputManager->allowTextInput;
	if ((increase) && (currentCount == 0))
		::PostMessage(manager->m_hWnd, WM_IME_SETSTATE, NULL, 1);
	else if ((!increase) && (currentCount == 1))
	{
		::PostMessage(manager->m_hWnd, WM_IME_SETSTATE, NULL, 0);
		static DisplayMenuManager* menu = DisplayMenuManager::GetSingleton();
		menu->enableState = false;
		menu->candidateList.clear();
		menu->inputContent.clear();
	}
}

void __fastcall Hooked_AllowTextInput(void* unk1, void* unk2, GFxFunctionHandler::Args* args)
{
	ASSERT(args->numArgs >= 1);
	bool enable = args->args[0].GetBool();
	InputManager* inputManager = InputManager::GetSingleton();
	if (inputManager)
	{
		ProcessAllowTextInput(enable);
		inputManager->AllowTextInput(enable);
	}
}

void __stdcall CheckFunctionHandler(GFxFunctionHandler* callback)
{
	static char* className = "class SKSEScaleform_AllowTextInput";
	static bool result = false;
	if (!result && strcmp(typeid(*callback).name(), className) == 0)
	{
		SafeWrite32((UInt32)(*(UInt32**)callback + 0x1), (UInt32)Hooked_AllowTextInput);
		result = true;
	}
}

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

void Hook_TextInput_Commit()
{
	//Fix ShowRaceMenu...
	SafeWrite8(0x0088740A, 0x00);
	//Fix EnchanmentMenu...
	SafeWrite8(0x008522B7, 0x00);
	//*((GFxLoader **)0x01B2E9B0);
	//call TESV.00A60670  bool GFxLoader::CheckString(char*);
	WriteRelJump(0x00A6068C, (UInt32)Hooked_CheckString);

	SafeWrite8(0x008868C0, 0xC3); _MESSAGE("PrecacheCharGen disabled...");
	SafeWrite8(0x00886B50, 0xC3); _MESSAGE("PrecacheCharGenClear disabled...");
	SafeWrite8(0x010CD560, 0x00); _MESSAGE("Intro disabled...");

	//Hook window mode...
	SafeWrite32(0x012CF5F8, 0x00);
	SafeWrite32(0x012CF604, 0x00);

	SafeWrite32(0x0069D832, WS_POPUP);
	SafeWrite32(0x0069D877, WS_POPUP | WS_VISIBLE);
	_MESSAGE("Borderless window enabled...");

	WriteRelJump(kCreateFunctionHook_Ent, (UInt32)Hooked_CreateFunction);

	WriteRelJump(kCustomAllowTextInputHook_Ent, (UInt32)Hooked_CustomAllowTextInput);

	SafeWrite32(0x010E4BD4 + 0x10, (UInt32)Hooked_ProcessMessage);
	_MESSAGE("Hook virtual function table of top menu...");
}

