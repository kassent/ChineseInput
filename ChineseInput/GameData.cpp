#include "GameData.h"

void* UIMessageEx::CreateUIMessageData(const BSFixedString &name)
{
	typedef void * (*_CreateUIMessageData)(const BSFixedString &name);
	const _CreateUIMessageData CreateUIMessageData = (_CreateUIMessageData)0x00547A00;
	return CreateUIMessageData(name);
}

