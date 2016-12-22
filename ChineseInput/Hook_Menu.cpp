#include "Hook_Menu.h"
#include "skse/SafeWrite.h"
#include "GameInputManager.h"
#include "DisplayMenuManager.h"
#include <map>
#include <algorithm>

class CustomMenuEventHandler : public BSTEventSink<MenuOpenCloseEvent>
{
public:
	virtual EventResult ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher);

	static CustomMenuEventHandler* GetSingleton();

	static std::map<UInt32, UInt32> menuHandlers;
};

std::map<UInt32, UInt32> CustomMenuEventHandler::menuHandlers;

CustomMenuEventHandler* CustomMenuEventHandler::GetSingleton()
{
	static CustomMenuEventHandler instance;
	return &instance;
}

UInt32 __fastcall Hooked_ProcessMessage(IMenu*, void*, UIMessageEx*);

EventResult CustomMenuEventHandler::ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher)
{
	static MenuManager* mm = MenuManager::GetSingleton();
	std::string menuName = evn->menuName.data;
	if ((menuName == "Cursor Menu") || (menuName == "HUD Menu") || (menuName == "Main Menu") || (menuName == "Loading Menu") || (menuName == "Fader Menu"))//Loading Menu  //Fader Menu
		return kEvent_Continue;
	if (evn->opening)
	{
		if (mm->IsMenuOpen(&(evn->menuName)))
		{
			IMenu* menu = mm->GetMenu(&(evn->menuName));
			if (menu)
			{
				UInt32 handler = *(UInt32*)menu;
				auto it = menuHandlers.find(handler);
				if (it == menuHandlers.end())
				{
					UInt32 fn = *(*(UInt32**)menu + 0x4);
					menuHandlers.insert(std::map<UInt32, UInt32>::value_type(handler, fn));
					SafeWrite32((UInt32)(*(UInt32**)menu + 0x4), (UInt32)Hooked_ProcessMessage);
				}
			}
		}
	}
	else
	{
		static InputManager* inputManager = InputManager::GetSingleton();
		if (!inputManager->allowTextInput)
		{
			static DisplayMenuManager* menu = DisplayMenuManager::GetSingleton();
			menu->enableState = false;
			menu->candidateList.clear();
			menu->inputContent = "";
		}
	}
	return kEvent_Continue;
}

typedef UInt32(__fastcall *_ProcessMessage)(IMenu* menu, void* unk, UIMessageEx* message);

UInt32 __fastcall Hooked_ProcessMessage(IMenu* menu, void* unk, UIMessageEx* message)
{
	static MenuManager* mm = MenuManager::GetSingleton();
	static DisplayMenuManager* manager = DisplayMenuManager::GetSingleton();
	if (message->type == UIMessageEx::kMessage_Open){}
		//_MESSAGE("OPEN MESSAGE RECEIVED: (name=%s, view=%p)", message->name.data, menu->view);
	else if (message->type == UIMessageEx::kMessage_Close){}
		//_MESSAGE("CLOSE MESSAGE RECEIVED: (name=%s)", message->name.data);
	else if (message->type == UIMessageEx::kMessage_Scaleform)
	{
		if (menu->view && message->data)
		{
			BSUIScaleformData* scaleformData = (BSUIScaleformData*)message->data;
			GFxEvent* event = scaleformData->event;
			if (event->type == GFxEvent::KeyDown && manager->disableKeyState)
			{
				static std::vector<UInt8> invalidKey{0x0D, 0x08, 0x6C, 0x25, 0x26, 0x27, 0x28};
				GFxKeyEvent* key = (GFxKeyEvent*)event;
				//_MESSAGE("keyCode = %08X, KkeyboardIndex = %d, asciiCode = %d, wcharCode = %d, specialKeysState = 0x%02X", key->keyCode, key->keyboardIndex, key->asciiCode, key->wcharCode, key->specialKeysState.states);
				if (std::find(invalidKey.begin(), invalidKey.end(), key->keyCode) != invalidKey.end())
					return NULL;
			}
			else if (event->type == GFxEvent::CharEvent)
			{
				GFxCharEvent* charEvent = (GFxCharEvent*)event;
				//_MESSAGE("wchar = %08X, KkeyboardIndex = %d", charEvent->wcharCode, charEvent->keyboardIndex);
				if ((!InputManager::GetSingleton()->allowTextInput) || (!charEvent->keyboardIndex) || charEvent->wcharCode < 0x20)
					return NULL;
				if (charEvent->keyboardIndex)
					charEvent->keyboardIndex = NULL;
			}
		}
	}
	auto it = CustomMenuEventHandler::menuHandlers.find(*(UInt32*)menu);
	if (it != CustomMenuEventHandler::menuHandlers.end())
	{
		_ProcessMessage ProcessMessage = (_ProcessMessage)(it->second);
		return ProcessMessage(menu, NULL, message);
	}
	_MESSAGE("Error:can't find menu's process function in map container...");
	return NULL;
}

void CALLBACK PreProcessAllowTextInputFlag(bool increase)
{
	static const InputManager* inputManager = InputManager::GetSingleton();
	UInt8 currentCount = inputManager->allowTextInput;
	//if ((increase) && (!currentCount))
		//GameInputManager::GetSingleton()->SetInputMethodState(true);
	//else if ((!increase) && (currentCount == 1))
		//GameInputManager::GetSingleton()->SetInputMethodState(false);
}

UInt32 kSKSEAllowTextInputHook_Ent;
UInt32 kSKSEAllowTextInputHook_Ret;

__declspec(naked) void Hooked_SKSEAllowTextInput()
{
	__asm
	{
		mov al, byte ptr[esi + 0x98]
		pushfd
		pushad
		push ebx
		call PreProcessAllowTextInputFlag
		popad
		popfd
		jmp [kSKSEAllowTextInputHook_Ret]
	}
}

const UInt32 kBSAllowTextInputHook_Ent = 0x00A66B40;
const UInt32 kBSAllowTextInputHook_Ret = 0x00A66B45;

__declspec(naked) void Hooked_BSAllowTextInput()
{
	__asm
	{
		cmp byte ptr[esp + 0x4], 0x0
		mov eax, [esp + 0x4]
		pushfd
		pushad
		push eax
		call PreProcessAllowTextInputFlag
		popad
		popfd
		jmp [kBSAllowTextInputHook_Ret]
	}
}


void Hook_TextInput_Commit()
{
	//Fix ShowRaceMenu...
	SafeWrite8(0x0088740A, 0x00);
	//Fix EnchanmentMenu...
	SafeWrite8(0x008522B7, 0x00);

	SafeWrite8(0x008868C0, 0xC3); _MESSAGE("PrecacheCharGen disabled...");
	SafeWrite8(0x00886B50, 0xC3); _MESSAGE("PrecacheCharGenClear disabled...");
	SafeWrite8(0x010CD560, 0x00); _MESSAGE("Intro disabled...");

	//*((GFxLoader **)0x01B2E9B0);
	//call TESV.00A60670  bool GFxLoader::CheckString(char*);
	//Hook window mode...
	SafeWrite32(0x012CF5F8, 0);
	SafeWrite32(0x012CF604, 0);

	SafeWrite32(0x0069D832, WS_POPUP);
	SafeWrite32(0x0069D877, WS_POPUP | WS_VISIBLE);
	_MESSAGE("Borderless window enabled...");
	/*
	UInt32 moduleHandle = (UInt32)GetModuleHandle("skse_1_9_32.dll");
	kSKSEAllowTextInputHook_Ent = moduleHandle + 0x5D3E0;
	kSKSEAllowTextInputHook_Ret = kSKSEAllowTextInputHook_Ent + 0x6;
	WriteRelJump(kSKSEAllowTextInputHook_Ent, (UInt32)Hooked_SKSEAllowTextInput);
	SafeWrite8(kSKSEAllowTextInputHook_Ent + 0x5, 0x90);
	WriteRelJump(kBSAllowTextInputHook_Ent, (UInt32)Hooked_BSAllowTextInput);
	*/
	MenuManager* mm = MenuManager::GetSingleton();
	if (mm)
	{
		EventDispatcher<MenuOpenCloseEvent>* eventDispatcher = mm->MenuOpenCloseEventDispatcher();
		eventDispatcher->AddEventSink(CustomMenuEventHandler::GetSingleton());
	}
	_MESSAGE("Add menu event sink...");
}

