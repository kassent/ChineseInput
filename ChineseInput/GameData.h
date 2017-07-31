#pragma once

#include "skse/GameMenus.h"
#include "GFxEvent.h"

class BSUIScaleformData : public IUIMessageData
{
public:
	virtual ~BSUIScaleformData() {}		// 00897320

										//void		** _vtbl;		// 00 - 0110DF70
										//UInt8[4]     unk;
	GFxEvent	* event;				// 08
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
