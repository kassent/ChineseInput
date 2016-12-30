#pragma once
#include "skse/GameMenus.h"
#include "skse/NiRenderer.h"
#include <dinput.h>
#include <queue>
#include "GFxEvent.h"
#define WM_IME_SETSTATE 0x0655
#define NO_INPUT	0
#define DIRECT_INPUT	1
#define IME_INPUT	2
#define INVALID_CODE 0xFFFFFFFF
#define IME_CHAR 0x1
#define DEFAULT_KEYBOARD 0x04090409

class BSUIScaleformData : public IUIMessageData
{
public:
	virtual ~BSUIScaleformData() {}		// 00897320

	//void		** _vtbl;		// 00 - 0110DF70
	GFxEvent	* event;		// 08
};
STATIC_ASSERT(sizeof(BSUIScaleformData) == 0x0C);

class UIMessageEx
{
public:
	enum Type
	{
		kMessage_Refresh = 0,				// 0 used after ShowAllMapMarkers
		kMessage_Open,						// 1
		kMessage_PreviouslyKnownAsClose,	// 2
		kMessage_Close,						// 3
		kMessage_Unk04,
		kMessage_Unk05,
		kMessage_Scaleform,					// 6 BSUIScaleformData
		kMessage_Message					// 7 BSUIMessageData
	};

	static void* CreateUIMessageData(const BSFixedString &name);

	//@members
	BSFixedString		name;		// 00
	Type				type;		// 04
	IUIMessageData		* data;		// 08 - something with a virtual destructor
	UInt8				isPooled;	// 0C
	UInt8				pad0D[3];	// 0D
};

class GameInputManager
{
public:
	typedef HWND(WINAPI* CreateWindowExA)(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
	typedef IDirect3D9* (WINAPI* Direct3DCreate9)(UINT SDKVersion);
	typedef std::queue<UInt32> CharQueue;
	enum
	{
		kFlag_AllowTextInput = 1,
		kFlag_AllowProcessCharMessage = 1 << 1
	};
	//@functions:	
	GameInputManager();
	~GameInputManager();

	bool InstallDriverHooks();
	void ShowCursor(bool show);
	bool SendUnicodeMessage(UInt32 wcharCode);

	static GameInputManager* GetSingleton();
	static void GetCandidateList(const HWND& hWnd);
	static void GetResultString(const HWND& hWnd);
	static void GetInputString(const HWND& hWnd);

	//@members:	
	HWND						m_hWnd;
	HIMC						m_hIMC;
	CharQueue					m_charQueue;
	CreateWindowExA				m_newCreateWindowExA;
	CreateWindowExA				m_oldCreateWindowExA;
	Direct3DCreate9				m_oldDirect3DCreate9;
	Direct3DCreate9				m_newDirect3DCreate9;
	WNDPROC						m_oldWndProc;
	WNDPROC						m_newWndProc;
};

class  BSWin32KeyboardDevice
{
public:
	UInt32					unk00;
	UInt32					unk04;
	UInt32					unk08;
	UInt32					unk0C;
	UInt32					unk10;
	UInt32					unk14;
	UInt32					unk18;
	UInt32					unk1C;
	UInt32					unk20;
	UInt32					unk24;
	IDirectInputDevice8*	inputDevice;					// 028 - IDirectInputDevice8 *
	UInt32					pad02C[(0x0F4 - 0x02C) >> 2];	// 02C
	UInt8					curState[0x100];				// 0F4
	UInt8					prevState[0x100];				// 1F4
};
STATIC_ASSERT(sizeof(BSWin32KeyboardDevice) == 0x2F4);
STATIC_ASSERT(offsetof(BSWin32KeyboardDevice, curState) == 0x0F4);


//InputDeviceManager* instance = *((InputDeviceManager**)0x01B2E724);
// 44
class InputDeviceManager
{
public:
	UInt32					unk00;
	UInt32					unk04;
	UInt32					unk08;
	UInt32					unk0C;
	UInt32					unk10;
	UInt32					unk14;
	UInt32					unk18;
	UInt32					unk1C;
	UInt32					unk20;
	UInt32					unk24;
	UInt32					unk28;
	UInt32					unk2C;
	UInt32					unk30;
	BSWin32KeyboardDevice	* keyboard;		// 034 - BSWin32KeyboardDevice
	void					* mouse;		// 038 - BSWin32MouseDevice
	void					* gamepad;		// 03C - BSWin32GamepadDevice
	bool					unk40;			// 040 - init'd true
	bool					unk41;			// 041 - init'd false

	void SetKeyboardCooperativeLevel(DWORD b)
	{
		keyboard->inputDevice->SetCooperativeLevel(GameInputManager::GetSingleton()->m_hWnd, b);
	}

	static InputDeviceManager* GetSingleton()
	{
		return *((InputDeviceManager**)0x01B2E724);
	}
};
STATIC_ASSERT(offsetof(InputDeviceManager, keyboard) == 0x34);
STATIC_ASSERT(sizeof(InputDeviceManager) == 0x44);








