#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/PluginManager.h"
#include "GameInputManager.h"
#include "Hook_Menu.h"

#include <shlobj.h>	// CSIDL_MYCODUMENTS

#define REQUIRE_SKSE_VERSION 0x01070030

IDebugLog		g_Log;
const char*		kLogPath = "\\My Games\\Skyrim\\Logs\\ChineseInput.log";

const char*		PLUGIN_NAME = "ChineseInput";
const UInt32	VERSION_MAJOR = 2;
const UInt32	VERSION_MINOR = 4;
const UInt32	VERSION_PATCH = 1;

PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

SKSEScaleformInterface*			g_scaleform = NULL;
SKSEMessagingInterface*			g_messageInterface = NULL;

void InputLoadedCallback(SKSEMessagingInterface::Message* msg)
{
	if (msg->type == SKSEMessagingInterface::kMessage_InputLoaded)
	{
		InputDeviceManager::GetSingleton()->SetKeyboardCooperativeLevel(DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY);
		_MESSAGE("Input loaded...");
	}
}
// ======================================================
// Initialization
// ======================================================

extern "C"
{

	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		g_Log.OpenRelative(CSIDL_MYDOCUMENTS, kLogPath);
		g_Log.SetPrintLevel(IDebugLog::kLevel_Error);

#ifdef _DEBUG
		g_Log.SetLogLevel(IDebugLog::kLevel_DebugMessage);
#else
		g_Log.SetLogLevel(IDebugLog::kLevel_Message);
#endif

		_MESSAGE("%s %i.%i.%i by Kassent", PLUGIN_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = VERSION_MAJOR;

		// store plugin handle so we can identify ourselves later
		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor)
		{
			_ERROR("loaded in editor, marking as incompatible");

			return false;
		}

		else if (skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
		{
			_ERROR("unsupported runtime version %08X", skse->runtimeVersion);

			return false;
		}

		else if (skse->skseVersion != REQUIRE_SKSE_VERSION)
		{
			_ERROR("unsupported skse version %08X, require SKSE V1.7.3", skse->skseVersion);

			return false;
		}

		g_messageInterface = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
		if (!g_messageInterface)
		{
			_ERROR("Couldn't Get Messaging Interface");

			return false;
		}
		if (g_messageInterface->interfaceVersion < SKSEMessagingInterface::kInterfaceVersion)
		{
			_ERROR("messaging interface too old (%d expected %d)", g_messageInterface->interfaceVersion, SKSEMessagingInterface::kInterfaceVersion);

			return false;
		}

		// supported runtime version
		return true;
	}


	bool SKSEPlugin_Load(const SKSEInterface * skse)
	{
		// register callback for SKSE messaging interface
		GameInputManager::GetSingleton()->InstallDriverHooks();

		//Hook_DirectInput_Commit();

		Hook_TextInput_Commit();
		
		g_messageInterface->RegisterListener(g_pluginHandle, "SKSE", InputLoadedCallback);

		return true;
	}

};
