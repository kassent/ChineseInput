#include "InputMenu.h"
#include "Settings.h"
#include "skse/GameMenus.h"

#define TextColorNormal D3DCOLOR_XRGB(255, 255, 255)
#define TextColorBackground D3DCOLOR_XRGB(0, 255 , 0)
#define TextShadowColorNormal D3DCOLOR_XRGB(50, 50, 50)
#define TextColorSelected D3DCOLOR_XRGB(0, 255, 0)
#define TextShadowColorSelected D3DCOLOR_XRGB(50, 50, 50)
#define TextColorEditing D3DCOLOR_XRGB(255, 100, 50)
#define TextShadowColorEditing D3DCOLOR_XRGB(50, 50, 50)
#define TextFont "»ªÎÄ¿¬Ìå"
#define TextSizeNormal 22
#define MaxRows 10
#define RowSpace 4
#define TitleRectX 60
#define TitleRectY 80
#define TitleRectSize 300
#define MenuRectX 65
#define MenuRectY 120
#define MenuRectSize 280
#define MenuRectSizeExtra 50

ICriticalSection g_criticalSection;


InputMenu* InputMenu::GetSingleton()
{
	static InputMenu instance;
	return &instance;
}

InputMenu::InputMenu()
{
	_MESSAGE("[CI] Screen width: %d", *(UInt32*)0x012CF5C8);
	screenLength = *(UInt32*)0x012CF5C8;
	enableState = disableKeyState = false;
	rectTitle = { screenLength - Settings::iOffsetX, Settings::iOffsetY, screenLength - Settings::iOffsetX + TitleRectSize, Settings::iOffsetY + TextSizeNormal };
	rectLine = new D3DRECT{ screenLength - Settings::iOffsetX, Settings::iOffsetY + TextSizeNormal + 2, screenLength - Settings::iOffsetX + TitleRectSize, Settings::iOffsetY + TextSizeNormal + 4 };
	SetRect(&rectStatus, rectLine->x1, rectLine->y1 + 4, rectLine->x2, rectLine->y2 + TextSizeNormal);

}

bool InputMenu::Initialize(IDirect3DDevice9* id)
{
	device = id;

	D3DXCreateFont(device, TextSizeNormal, 0, FW_BOLD, 1, false, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, Settings::sFontName.c_str(), &fontNormal);
	D3DXCreateFont(device, TextSizeNormal, 0, FW_BOLD, 1, false, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, Settings::sFontName.c_str(), &fontSelected);
	D3DXCreateFont(device, TextSizeNormal, 0, FW_NORMAL, 1, false, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, Settings::sFontName.c_str(), &fontTitle);

	return true;
}

void InputMenu::OnLostDevice()
{
	fontSelected->OnLostDevice();
	fontNormal->OnLostDevice();
	fontTitle->OnLostDevice();
}

void InputMenu::OnResetDevice()
{
	fontSelected->OnResetDevice();
	fontNormal->OnResetDevice();
	fontTitle->OnResetDevice();
}

void InputMenu::NextFrame()
{
	InputManager* im = InputManager::GetSingleton();

	if (im->allowTextInput && InterlockedCompareExchange(&enableState, enableState, 2))
	{
		UInt32	iSelectedIndex = InterlockedCompareExchange(&selectedIndex, selectedIndex, -1);
		UInt32	iPageStartIndex = InterlockedCompareExchange(&pageStartIndex, pageStartIndex, -1);
		g_criticalSection.Enter();
		const char* text = inputContent.c_str();

		if (strlen(text))
		{
			fontNormal->DrawTextA(nullptr, text, -1, &rectTitle, DT_LEFT, TextColorNormal);
			device->Clear(1, rectLine, D3DCLEAR_TARGET, TextColorNormal, 0, 0);
			fontNormal->DrawTextA(nullptr, stateInfo.c_str(), -1, &rectStatus, DT_LEFT, TextColorNormal);
			SetRect(&rect, screenLength - (Settings::iOffsetX - 5) + MenuRectSize * 0, Settings::iOffsetY + 40, screenLength - (Settings::iOffsetX - 5) + MenuRectSize * 1, Settings::iOffsetY + 40 + TextSizeNormal);

			for (size_t i = 0; i < candidateList.size(); ++i)
			{
				text = candidateList[i].c_str();
				rect.top += TextSizeNormal + RowSpace;
				rect.bottom += TextSizeNormal + RowSpace;
				SetRect(&rectShadow, rect.left + 1, rect.top + 1, rect.right + 1, rect.bottom + 1);

				if (iSelectedIndex != (i + iPageStartIndex))
				{
					fontNormal->DrawTextA(nullptr, text, -1, &rectShadow, DT_LEFT, TextShadowColorNormal);
					fontNormal->DrawTextA(nullptr, text, -1, &rect, DT_LEFT, TextColorNormal);
				}
				else
				{
					fontNormal->DrawTextA(nullptr, text, -1, &rectShadow, DT_LEFT, TextShadowColorSelected);
					fontNormal->DrawTextA(nullptr, text, -1, &rect, DT_LEFT, TextColorSelected);
				}
			}
		}
		g_criticalSection.Leave();
	}
}


