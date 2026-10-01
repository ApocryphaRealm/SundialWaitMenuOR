#include "Tool.h"

#include "Dial.h"
#include "Settings.h"
#include "TestBenchAPI.h"

namespace tool
{
	namespace
	{
		TestBenchAPI::ITestBenchInterface001* g_tb = nullptr;

		void Write(void* a_sink, TestBenchAPI::WriteFn a_write, const json& a_j) { a_write(a_sink, a_j.dump().c_str()); }

		void StatusTool(void*, const char*, void* a_sink, TestBenchAPI::WriteFn a_write)
		{
			Write(a_sink, a_write, { { "ok", true }, { "version", SWM_VERSION }, { "dial", dial::State() }, { "settings", settings::GetAll() } });
		}

		void DriveTool(void*, const char* a_args, void* a_sink, TestBenchAPI::WriteFn a_write)
		{
			json args = json::parse(a_args ? a_args : "{}", nullptr, false);
			if (args.is_discarded()) {
				Write(a_sink, a_write, { { "ok", false }, { "error", "args are not JSON" } });
				return;
			}
			const std::string op = args.value("op", "");
			if (op == "reapply") {
				dial::Queue(dial::Action::kReapply);
				Write(a_sink, a_write, { { "ok", true }, { "queued", op }, { "note", "applied on the next game tick; read sundial.status" } });
			} else if (op == "reloadArt") {
				dial::Queue(dial::Action::kReloadArt);
				Write(a_sink, a_write, { { "ok", true }, { "queued", op } });
			} else if (op == "hours") {
				const int h = args.value("value", -1);
				dial::PreviewHours(h);
				Write(a_sink, a_write, { { "ok", true }, { "preview", h }, { "note", "-1 returns the rim to the menu's own hours" } });
			} else {
				Write(a_sink, a_write, { { "ok", false }, { "error", "op: reapply | reloadArt | hours{value: -1..24}" } });
			}
		}
	}

	bool Register()
	{
		if (g_tb) return true;
		HMODULE tb = ::GetModuleHandleW(L"TestBench.dll");
		auto    get = tb ? reinterpret_cast<void* (*)(unsigned)>(::GetProcAddress(tb, "TestBench_GetInterface")) : nullptr;
		g_tb = get ? static_cast<TestBenchAPI::ITestBenchInterface001*>(get(1)) : nullptr;
		if (!g_tb) return false;
		g_tb->RegisterTool("sundial.status",
			R"({"description":"Sundial Wait Menu: the live state - the view model and menu found, the dial built, its layout (heights in menu units), the hours on the rim, waiting or choosing, the game hour, the pictures, settings.","inputSchema":{"type":"object","properties":{}},"readOnly":true})",
			&StatusTool, nullptr);
		g_tb->RegisterTool("sundial.drive",
			R"({"description":"Sundial Wait Menu - drive it for testing. op: reapply (build and lay out again) | reloadArt (import the PNGs again) | hours {value: 0-24 shows that many hours on the rim, -1 the menu's own}.","inputSchema":{"type":"object","properties":{"op":{"type":"string"},"value":{"type":"integer"}}},"readOnly":false})",
			&DriveTool, nullptr);
		logger::info("TestBench tools registered: sundial.status, sundial.drive");
		return true;
	}
}
