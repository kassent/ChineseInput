#include "DisplayMenuManager.h"
#include "skse/GameMenus.h"

#define TextColorNormal D3DCOLOR_XRGB(180, 180 , 180)
#define TextShadowColorNormal D3DCOLOR_XRGB(50, 50, 50)
#define TextColorSelected D3DCOLOR_XRGB(255, 255, 255)
#define TextShadowColorSelected D3DCOLOR_XRGB(50, 50, 50)
#define TextColorEditing D3DCOLOR_XRGB(255, 100, 50)
#define TextShadowColorEditing D3DCOLOR_XRGB(50, 50, 50)
#define TextFont "Arial"
#define TextSizeNormal 22
#define MaxRows 10
#define RowSpace 4
#define TitleRectX 60
#define TitleRectY 80
#define TitleRectSize 300
#define MenuRectX 65
#define MenuRectY 120
#define MenuRectSize 250
#define MenuRectSizeExtra 50

DisplayMenuManager* DisplayMenuManager::GetSingleton()
{
	static DisplayMenuManager instance;
	return &instance;
}

DisplayMenuManager::DisplayMenuManager()
{
	_MESSAGE(__FUNCTION__);
	enableState = disableKeyState = false;
	title = IMEINFO;
	rectTitle = { TitleRectX, TitleRectY, TitleRectX + TitleRectSize, TitleRectY + TextSizeNormal };
	rectLine = new D3DRECT{ TitleRectX, TitleRectY + TextSizeNormal + 2, TitleRectX + TitleRectSize, TitleRectY + TextSizeNormal + 4 };
	SetRect(&rectStatus, rectLine->x1, rectLine->y1 + 4, rectLine->x2, rectLine->y2 + TextSizeNormal);
}

bool DisplayMenuManager::Initialize(IDirect3DDevice9* id)
{
	device = id;
	D3DXCreateFont(device, TextSizeNormal, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, "¿¬Ìå", &fontNormal);
	D3DXCreateFont(device, TextSizeNormal, 0, FW_BOLD, 1, false, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, "¿¬Ìå", &fontSelected);
	D3DXCreateFont(device, TextSizeNormal, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, "¿¬Ìå", &fontTitle);
	return true;
}

void DisplayMenuManager::OnLostDevice()
{
	fontSelected->OnLostDevice();
	fontNormal->OnLostDevice();
	fontTitle->OnLostDevice();
}

void DisplayMenuManager::OnResetDevice()
{
	fontSelected->OnResetDevice();
	fontNormal->OnResetDevice();
	fontTitle->OnResetDevice();
}

void DisplayMenuManager::Render()
{
	static InputManager* im = InputManager::GetSingleton();
	if (!im->allowTextInput)
		return;
	if (enableState) 
	{
		const char* text = NULL;
		text = inputContent.c_str();
		if (!strlen(text))
			return;
		fontNormal->DrawTextA(NULL, text, -1, &rectTitle, DT_LEFT, TextColorNormal);
		device->Clear(1, rectLine, D3DCLEAR_TARGET, TextColorNormal, 0, NULL);
		fontNormal->DrawTextA(NULL, title.c_str(), -1, &rectStatus, DT_LEFT, TextColorNormal);
		SetRect(&rect, MenuRectX + MenuRectSize * 0, MenuRectY, MenuRectX + MenuRectSize * 1, MenuRectY + TextSizeNormal);
		size_t drawLen = (pageSize < MaxRows) ? pageSize : MaxRows;
		for (size_t i = 0; i < drawLen; i++)
		{
			text = candidateList[i].c_str();
			rect.top += TextSizeNormal + RowSpace;
			rect.bottom += TextSizeNormal + RowSpace;
			SetRect(&rectShadow, rect.left + 1, rect.top + 1, rect.right + 1, rect.bottom + 1);
			if (selectedIndex != (i + pageStartIndex))
			{
				fontNormal->DrawTextA(NULL, text, -1, &rectShadow, DT_LEFT, TextShadowColorNormal);
				fontNormal->DrawTextA(NULL, text, -1, &rect, DT_LEFT, TextColorNormal);
			}
			else
			{
				fontNormal->DrawTextA(NULL, text, -1, &rectShadow, DT_LEFT, TextShadowColorSelected);
				fontNormal->DrawTextA(NULL, text, -1, &rect, DT_LEFT, TextColorSelected);
			}
		}
	}
}


