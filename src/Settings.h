#pragma once

// SundialWaitMenu.ini beside the plugin, read once at load. The compiled defaults are the shipped INI's values (rule 16).

namespace settings
{
	struct Values
	{
		bool enabled = true;    // [General] bEnabled: 0 leaves the game's wait menu exactly as it is
		int  dialSize = 680;    // [Dial] uSize: the dial's diameter, rim and marks included, in the menu's own units (1080p pixels)
		int  titleGap = 10;     // [Dial] uTitleGap: between the menu's title and the top of the dial
		int  buttonGap = 14;    // [Dial] uButtonGap: between the bottom of the dial and the menu's buttons
		bool inkText = true;    // [Dial] bInkText: the hours, the time and the date on the face in brown ink (the game's light text is hard to read on paper)
		int  logLevel = 2;      // [Log] uLogLevel: 0 trace, 1 debug, 2 info, 3 warnings, 4 errors
	};

	void Load();
	const Values& Get();
	json GetAll();
	std::filesystem::path PluginFolder();   // ...\OblivionRemastered\Binaries\Win64\OBSE\Plugins
}
