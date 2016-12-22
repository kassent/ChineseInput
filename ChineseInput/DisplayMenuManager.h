#pragma once
#include <d3dx9core.h>
#include <vector>
#include <string>
using std::string;

typedef std::vector<string> CandidateList;

class DisplayMenuManager
{
public:
	DisplayMenuManager();
	void Render();
	bool Initialize(IDirect3DDevice9* id);
	void OnLostDevice();
	void OnResetDevice();
	static DisplayMenuManager* GetSingleton();

	bool				enableState;
	bool				disableKeyState;
	string				title;
	string				inputContent;
	CandidateList		candidateList;
	UInt32				pageStartIndex;
	UInt32				pageSize;
	UInt32				selectedIndex;
	ID3DXFont*			fontSelected;
	ID3DXFont*			fontNormal;
	ID3DXFont*			fontTitle;
	D3DRECT*			rectLine;
	RECT				rectTitle;
	RECT				rectStatus;
	RECT				rect;
	RECT				rectShadow;
	IDirect3DDevice9*	device;
};
