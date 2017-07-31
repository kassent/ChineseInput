#pragma once

namespace InputUtil
{
	void Convert(const char* strIn, char* strOut, int sourceCodepage, int targetCodepage);

	bool SendUnicodeMessage(UInt32 wcharCode);

	void GetCandidateList(const HWND& hWnd);

	void GetResultString(const HWND& hWnd);

	void GetInputString(const HWND& hWnd);

	void ShowCursorEx(bool show);

}
