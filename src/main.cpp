// Sundial Wait Menu (Oblivion Remastered, OBSE64) - entry point.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Dial.h"
#include "Settings.h"
#include "Tick.h"
#include "Tool.h"

namespace
{
	// every frame, on the game thread (the PeekMessageW tick - it runs while a menu has the game paused, and through a wait)
	void OnFrame()
	{
		static bool          toolRegistered = false;
		static std::uint64_t n = 0;
		if (!toolRegistered && ++n % 60 == 0) {
			toolRegistered = tool::Register();   // TestBench is a test-only plugin: asked again until it answers
		}
		dial::Tick();
	}

	void OnMessage(OBSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) return;
		if (a_msg->type == OBSE::MessagingInterface::kPostLoad) {
			if (!tool::Register()) logger::debug("TestBench not loaded yet - the driving tools are tried again shortly");
			tick::Install(&OnFrame);
		}
	}

	// the previous launch's log, kept as SundialWaitMenu.prev.log before OBSE::Init truncates it
	void KeepPreviousLog()
	{
		PWSTR docs = nullptr;
		if (FAILED(::SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) || !docs) {
			return;
		}
		const std::filesystem::path dir = std::filesystem::path(docs) / L"My Games" / L"Oblivion Remastered" / L"OBSE" / L"Logs";
		::CoTaskMemFree(docs);
		std::error_code ec;
		if (std::filesystem::exists(dir / L"SundialWaitMenu.log", ec)) {
			std::filesystem::copy_file(dir / L"SundialWaitMenu.log", dir / L"SundialWaitMenu.prev.log", std::filesystem::copy_options::overwrite_existing, ec);
		}
	}
}

OBSE_PLUGIN_LOAD(const OBSE::LoadInterface* a_obse)
{
	KeepPreviousLog();
	OBSE::Init(a_obse);
	settings::Load();
	{
		const auto level = static_cast<spdlog::level::level_enum>(std::clamp(settings::Get().logLevel, 0, 6));
		logger::set_level(level, level);
	}
	logger::info("Sundial Wait Menu {} (Oblivion Remastered). Log level {} - set [Log] uLogLevel=1 in SundialWaitMenu.ini for more detail "
				 "when reporting a problem.", SWM_VERSION, settings::Get().logLevel);
	if (!settings::Get().enabled) {
		logger::info("[General] bEnabled=0 - the game's own wait menu is left as it is");
	}

	if (auto* messaging = OBSE::GetMessagingInterface()) {
		if (!messaging->RegisterListener(&OnMessage)) {
			logger::error("OBSE messaging refused the listener - Sundial Wait Menu will not start");
		}
	} else {
		logger::error("OBSE messaging interface missing - Sundial Wait Menu will not start");
	}
	return true;
}
