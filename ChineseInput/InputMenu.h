#pragma once
#include <d3dx9core.h>
#include <vector>
#include <string>
#include "common/ICriticalSection.h"

#define MAX_CANDLIST 10

class InputMenu
{
public:
	typedef std::vector<std::string> CandidateList;

	InputMenu();

	void NextFrame();

	bool Initialize(IDirect3DDevice9* id);

	void OnResetDevice();
	void OnLostDevice();

	volatile UInt32			enableState;
	volatile UInt32			disableKeyState;
	volatile UInt32			pageStartIndex;
	volatile UInt32			pageSize;
	volatile UInt32			selectedIndex;

	std::string				stateInfo;
	std::string				inputContent;

	CandidateList			candidateList;

	IDirect3DDevice9*		device;
	HWND					pHandle;

	ID3DXFont*				fontSelected;
	ID3DXFont*				fontNormal;
	ID3DXFont*				fontTitle;

	D3DRECT*				rectLine;

	RECT					rectTitle;
	RECT					rectStatus;
	RECT					rect;
	RECT					rectShadow;
	long					screenLength;


	static InputMenu* GetSingleton();
};


extern ICriticalSection g_criticalSection;