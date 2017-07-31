#pragma comment (lib, "imm32.lib")

#include "InputUtil.h"
#include "RingBuffer.h"
#include "InputMenu.h"
#include "GameData.h"

#include "imm.h"

#include <memory>


RingBuffer<GFxCharEvent, 200>				g_inputBuffer;

namespace InputUtil
{

	void Convert(const char* strIn, char* strOut, int sourceCodepage, int targetCodepage)
	{
		int len = lstrlen(strIn);
		int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strIn, -1, NULL, 0);
		wchar_t* pUnicode;
		pUnicode = new wchar_t[unicodeLen + 1];
		memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
		MultiByteToWideChar(sourceCodepage, 0, strIn, -1, (LPWSTR)pUnicode, unicodeLen);
		BYTE * pTargetData = NULL;
		int targetLen = WideCharToMultiByte(targetCodepage, 0, (LPWSTR)pUnicode, -1, (char *)pTargetData, 0, NULL, NULL);
		pTargetData = new BYTE[targetLen + 1];
		memset(pTargetData, 0, targetLen + 1);
		WideCharToMultiByte(targetCodepage, 0, (LPWSTR)pUnicode, -1, (char *)pTargetData, targetLen, NULL, NULL);
		lstrcpy(strOut, (char*)pTargetData);
		delete pUnicode;
		delete pTargetData;
	}

	bool SendUnicodeMessage(UInt32 wcharCode)
	{
		if (wcharCode == 96 || wcharCode == 183)
			return false;
		UIStringHolder* stringHolder = UIStringHolder::GetSingleton();
		InputManager* mm = InputManager::GetSingleton();

		if (mm->allowTextInput)
		{
			BSUIScaleformData* msgData = (BSUIScaleformData*)UIMessageEx::CreateUIMessageData(stringHolder->bsUIScaleformData);
			msgData->event = g_inputBuffer.emplace_back(wcharCode, 0);
			BSFixedString menuName = stringHolder->topMenu;
			CALL_MEMBER_FN(UIManager::GetSingleton(), AddMessage)(&menuName, UIMessageEx::kMessage_Scaleform, msgData);
			return true;
		}
		return false;
	}


	void GetCandidateList(const HWND& hWnd)
	{
		InputMenu* mm = InputMenu::GetSingleton();
		if (!InterlockedCompareExchange(&mm->enableState, mm->enableState, 2))
			return;
		HIMC hIMC = ImmGetContext(hWnd);
		if (hIMC == nullptr)
			return;
		DWORD dwBufLen = ImmGetCandidateList(hIMC, 0, NULL, 0);
		if (!dwBufLen)
		{
			ImmReleaseContext(hWnd, hIMC);
			return;
		}
		std::unique_ptr<CANDIDATELIST, void(__cdecl*)(void*)> pList(reinterpret_cast<LPCANDIDATELIST>(FormHeap_Allocate(dwBufLen)), FormHeap_Free);
		if (!pList)
		{
			ImmReleaseContext(hWnd, hIMC);
			return;
		}
		ImmGetCandidateList(hIMC, 0, pList.get(), dwBufLen);
		if (pList->dwStyle != IME_CAND_CODE)
		{
			char buffer[0x8];

			InterlockedExchange(&mm->selectedIndex, pList->dwSelection);
			InterlockedExchange(&mm->pageStartIndex, pList->dwPageStart);

			g_criticalSection.Enter();

			mm->candidateList.clear();
			for (size_t i = 0; i < pList->dwCount && i < pList->dwPageSize && i < MAX_CANDLIST; i++)
			{
				sprintf_s(buffer, "%d.", i + 1);
				TCHAR* pSubStr = (TCHAR*)((BYTE*)(pList.get()) + pList->dwOffset[i + pList->dwPageStart]);
				std::string temp(buffer);
				temp += pSubStr;
				mm->candidateList.push_back(temp);
			}

			g_criticalSection.Leave();
		}
		ImmReleaseContext(hWnd, hIMC);
	}


	void GetResultString(const HWND& hWnd)
	{
		HIMC hIMC = ImmGetContext(hWnd);
		if (hIMC)
		{
			DWORD bufferSize = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0);
			if (bufferSize)
			{
				bufferSize += sizeof(WCHAR);
				std::unique_ptr<WCHAR[], void(__cdecl*)(void*)> wCharBuffer(reinterpret_cast<WCHAR*>(FormHeap_Allocate(bufferSize)), FormHeap_Free);
				ZeroMemory(wCharBuffer.get(), bufferSize);
				ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, wCharBuffer.get(), bufferSize);
				size_t len = bufferSize / sizeof(WCHAR);

				for (size_t i = 0; i < len; ++i)
				{
					UInt16 unicode = (UInt16)(wCharBuffer[i]);
					if (unicode > 0)
					{
						SendUnicodeMessage(unicode);
					}
				}
			}
		}
		ImmReleaseContext(hWnd, hIMC);
	}


	void GetInputString(const HWND& hWnd)
	{
		HIMC hIMC = ImmGetContext(hWnd);
		UINT uLen, uMem;
		uLen = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
		if (uLen)
		{
			std::unique_ptr<TCHAR[], void(__cdecl*)(void*)> szCompStr(reinterpret_cast<TCHAR*>(FormHeap_Allocate(uMem = uLen + 1)), FormHeap_Free);

			if (szCompStr)
			{
				szCompStr[uLen] = 0;
				ImmGetCompositionString(hIMC, GCS_COMPSTR, szCompStr.get(), uMem);

				InputMenu* mm = InputMenu::GetSingleton();
				g_criticalSection.Enter();
				mm->inputContent = szCompStr.get();
				g_criticalSection.Leave();
			}
		}
		ImmReleaseContext(hWnd, hIMC);
	}


	void ShowCursorEx(bool show)
	{
		if (show)
			while (::ShowCursor(TRUE) < 0);
		else
			while (::ShowCursor(FALSE) > -1);
	}
}