#include "Settings.h"

namespace settings
{
	namespace
	{
		Values g_values;

		std::filesystem::path ThisModule()
		{
			HMODULE self = nullptr;
			wchar_t buf[MAX_PATH]{};
			if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCWSTR>(&ThisModule), &self) &&
				GetModuleFileNameW(self, buf, MAX_PATH)) {
				return std::filesystem::path(buf);
			}
			return {};
		}

		int ReadInt(const std::filesystem::path& a_ini, const wchar_t* a_section, const wchar_t* a_key, int a_default, int a_min, int a_max)
		{
			const int v = static_cast<int>(GetPrivateProfileIntW(a_section, a_key, a_default, a_ini.c_str()));
			return std::clamp(v, a_min, a_max);
		}
	}

	std::filesystem::path PluginFolder()
	{
		return ThisModule().parent_path();
	}

	void Load()
	{
		const auto ini = PluginFolder() / L"SundialWaitMenu.ini";
		Values     v;
		if (std::filesystem::exists(ini)) {
			v.enabled = ReadInt(ini, L"General", L"bEnabled", v.enabled, 0, 1) != 0;
			v.dialSize = ReadInt(ini, L"Dial", L"uSize", v.dialSize, 300, 1000);
			v.titleGap = ReadInt(ini, L"Dial", L"uTitleGap", v.titleGap, 0, 200);
			v.buttonGap = ReadInt(ini, L"Dial", L"uButtonGap", v.buttonGap, 0, 200);
			v.inkText = ReadInt(ini, L"Dial", L"bInkText", v.inkText, 0, 1) != 0;
			v.logLevel = ReadInt(ini, L"Log", L"uLogLevel", v.logLevel, 0, 6);
			g_values = v;
			logger::info("settings: {} read - {}", ini.string(), GetAll().dump());
		} else {
			g_values = v;
			logger::warn("settings: {} not found - the compiled defaults are in use ({})", ini.string(), GetAll().dump());
		}
	}

	const Values& Get()
	{
		return g_values;
	}

	json GetAll()
	{
		const auto& v = g_values;
		return { { "General.bEnabled", v.enabled }, { "Dial.uSize", v.dialSize }, { "Dial.uTitleGap", v.titleGap }, { "Dial.uButtonGap", v.buttonGap },
			{ "Dial.bInkText", v.inkText }, { "Log.uLogLevel", v.logLevel } };
	}
}
