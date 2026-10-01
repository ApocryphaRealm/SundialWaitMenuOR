#include "Dial.h"

#include "Settings.h"
#include "Ue.h"

namespace dial
{
	namespace
	{
		constexpr const wchar_t* kVmClass = L"/Script/Altar.VSleepWaitMenuViewModel";
		constexpr const wchar_t* kMenuClass = L"/Game/UI/Modern/ModalLayer/WBP_ModernMenu_SleepWait.WBP_ModernMenu_SleepWait_C";
		constexpr const char*    kMenuClassName = "WBP_ModernMenu_SleepWait_C";
		constexpr int            kHours = 24;

		// ESlateVisibility
		constexpr std::uint8_t kCollapsed = 1, kHidden = 2, kHitTestInvisible = 3;
		// EHorizontalAlignment / EVerticalAlignment
		constexpr std::uint8_t kFill = 0, kTop = 1, kCentre = 2;
		// #4a3222, the brown ink of the banners, as a LINEAR colour (Slate shows FLinearColor through sRGB)
		constexpr std::array<float, 4> kInk = { 0.0685f, 0.0318f, 0.0160f, 1.0f };

		// ---- what we found ----
		ue::Handle  g_vm, g_menu;
		UE::UClass* g_menuClass = nullptr;
		ULONGLONG   g_vmScanAt = 0, g_menuScanAt = 0, g_classScanAt = 0, g_nameScanAt = 0;
		bool        g_up = false;

		// ---- ours, built inside the menu's background tile ----
		struct Built
		{
			ue::Handle                      menu, overlay, box, boxSlot, stack, face, marks, sun;
			std::array<ue::Handle, kHours> hours;
			std::array<float, kHours>      angle{};
			std::array<bool, kHours>       shown{};
			float                           sunAngle = -1000.0f;
		} g_b;
		int g_builds = 0;
		ULONGLONG g_lastBuildTry = 0;

		ue::Handle g_texFace, g_texHour, g_texMarks, g_texSun;
		bool       g_artFailed = false;

		// ---- the menu's own widgets, found once per menu ----
		struct Parts
		{
			ue::Handle tile, tileImage, sizer, invalidation, dropShadow, main, header, spacerTop, spacerBottom, buttons, slider, sliderBar;
			std::array<ue::Handle, 6> middle;     // Time_Text, Spacer_2, sleep_slider, Spacer_4, Date_Text, Seed_Text
			std::array<ue::Handle, 3> faceTexts;  // Time_Text, Date_Text, Seed_Text
		} g_p;

		// ---- the wait itself ----
		float     g_t0 = -1.0f;        // TimeToWait as the menu came up
		float     g_lastHour = -1.0f;    // the game hour at the last read
		bool      g_counting = false;  // the wait was confirmed: the hours are TimeToWait's
		int       g_shownHours = -1;
		std::atomic<int> g_preview{ -1 };   // set from TestBench's thread
		ULONGLONG g_nextUpdate = 0, g_nextLayout = 0, g_upAt = 0;
		json      g_layout = json::object();
		std::string g_status = "the menu has not been opened yet";

		std::mutex          g_stateLock;
		json                g_state = json::object();
		std::mutex          g_queueLock;
		std::vector<Action> g_queue;

		void Status(std::string a_s)
		{
			if (g_status != a_s) logger::info("dial: {}", a_s);
			g_status = std::move(a_s);
		}

		// ---- reflection, cached per (class, name): ue::Offset walks the property chain with an allocation per field ----
		std::int32_t Off(UE::UStruct* a_struct, const char* a_name)
		{
			static std::unordered_map<std::string, std::int32_t> cache;
			if (!a_struct) return -1;
			std::string key = std::to_string(reinterpret_cast<std::uintptr_t>(a_struct)) + "|" + a_name;
			if (const auto it = cache.find(key); it != cache.end()) return it->second;
			const auto off = ue::Offset(a_struct, a_name);
			cache.emplace(std::move(key), off);
			return off;
		}

		template <class T>
		T* Prop(UE::UObject* a_o, const char* a_name)
		{
			return a_o && a_o->GetClass() ? ue::At<T>(a_o, Off(a_o->GetClass(), a_name)) : nullptr;
		}

		UE::UObject* ObjProp(UE::UObject* a_o, const char* a_name)
		{
			auto** p = Prop<UE::UObject*>(a_o, a_name);
			return p ? *p : nullptr;
		}

		UE::UObject* SlotOf(UE::UObject* a_w) { return ObjProp(a_w, "Slot"); }

		bool IsA(UE::UObject* a_o, UE::UClass* a_class)
		{
			for (auto* c = a_o ? static_cast<UE::UStruct*>(a_o->GetClass()) : nullptr; c; c = c->superStruct) {
				if (c == reinterpret_cast<UE::UStruct*>(a_class)) return true;
			}
			return false;
		}

		// a widget by its name anywhere under a user widget - its tree, panels' slots and nested user widgets' trees
		// (Improved Wheel Menu's ui::FindInTree: GetWidgetFromName found nothing in game, 2026-09-30)
		UE::UObject* Search(UE::UObject* a_w, std::string_view a_name, int a_depth)
		{
			static auto* userWidget = ue::Class(L"/Script/UMG.UserWidget");
			static auto* panelWidget = ue::Class(L"/Script/UMG.PanelWidget");
			static auto* treeClass = ue::Class(L"/Script/UMG.WidgetTree");
			static auto* panelSlot = ue::Class(L"/Script/UMG.PanelSlot");
			if (!a_w || a_depth > 24 || !userWidget || !panelWidget || !treeClass || !panelSlot) return nullptr;
			if (a_depth > 0 && ue::NameOf(a_w) == a_name) return a_w;
			if (IsA(a_w, userWidget)) {
				auto* tree = ue::At<UE::UObject*>(a_w, Off(reinterpret_cast<UE::UStruct*>(userWidget), "WidgetTree"));
				auto* root = tree && *tree ? ue::At<UE::UObject*>(*tree, Off(reinterpret_cast<UE::UStruct*>(treeClass), "RootWidget")) : nullptr;
				if (root && *root) {
					if (ue::NameOf(*root) == a_name) return *root;
					if (auto* found = Search(*root, a_name, a_depth + 1)) return found;
				}
			}
			if (IsA(a_w, panelWidget)) {
				struct RawArray { UE::UObject** data; std::int32_t num; std::int32_t max; };
				const auto* arr = ue::At<RawArray>(a_w, Off(reinterpret_cast<UE::UStruct*>(panelWidget), "Slots"));
				const auto  contentOff = Off(reinterpret_cast<UE::UStruct*>(panelSlot), "Content");
				for (std::int32_t i = 0; arr && arr->data && contentOff >= 0 && i < arr->num && i < 256; ++i) {
					auto* content = arr->data[i] ? *ue::At<UE::UObject*>(arr->data[i], contentOff) : nullptr;
					if (!content) continue;
					if (ue::NameOf(content) == a_name) return content;
					if (auto* found = Search(content, a_name, a_depth + 1)) return found;
				}
			}
			return nullptr;
		}

		UE::UObject* Find(UE::UObject* a_root, std::string_view a_name)
		{
			auto* w = Search(a_root, a_name, 0);
			if (!w) logger::warn("dial: {} has no widget named {}", ue::NameOf(a_root), a_name);
			return w;
		}

		// ---- small reflected calls ----
		void Byte(UE::UObject* a_o, const wchar_t* a_fn, std::uint8_t a_v) { ue::CallFirst(a_o, a_fn, &a_v, sizeof(a_v)); }
		void Float(UE::UObject* a_o, const wchar_t* a_fn, float a_v) { ue::CallFirst(a_o, a_fn, &a_v, sizeof(a_v)); }
		void Vec2(UE::UObject* a_o, const wchar_t* a_fn, double a_x, double a_y)
		{
			const double v[2] = { a_x, a_y };
			ue::CallFirst(a_o, a_fn, v, sizeof(v));
		}

		void Visibility(UE::UObject* a_w, std::uint8_t a_v) { Byte(a_w, L"SetVisibility", a_v); }

		// a float property set through its setter only when it differs (the game's own widgets: read back, never assumed)
		bool FloatIs(UE::UObject* a_w, const char* a_prop, const wchar_t* a_setter, float a_want)
		{
			auto* p = Prop<float>(a_w, a_prop);
			if (!a_w || (p && std::abs(*p - a_want) < 0.01f)) return false;
			Float(a_w, a_setter, a_want);
			return true;
		}

		bool OpacityIs(UE::UObject* a_w, float a_want) { return FloatIs(a_w, "RenderOpacity", L"SetRenderOpacity", a_want); }

		// FMargin (four floats: left, top, right, bottom)
		std::array<float, 4> Padding(UE::UObject* a_slot)
		{
			auto* p = Prop<std::array<float, 4>>(a_slot, "Padding");
			return p ? *p : std::array<float, 4>{};
		}

		bool PaddingIs(UE::UObject* a_slot, const std::array<float, 4>& a_want)
		{
			if (!a_slot) return false;
			const auto had = Padding(a_slot);
			bool same = true;
			for (std::size_t i = 0; i < 4; ++i) same = same && std::abs(had[i] - a_want[i]) < 0.25f;
			if (same) return false;
			ue::CallFirst(a_slot, L"SetPadding", a_want.data(), sizeof(float) * 4);
			return true;
		}

		bool AlignIs(UE::UObject* a_slot, std::uint8_t a_h, std::uint8_t a_v)
		{
			if (!a_slot) return false;
			auto* h = Prop<std::uint8_t>(a_slot, "HorizontalAlignment");
			auto* v = Prop<std::uint8_t>(a_slot, "VerticalAlignment");
			bool changed = false;
			if (a_h != 0xFF && (!h || *h != a_h)) {
				Byte(a_slot, L"SetHorizontalAlignment", a_h);
				changed = true;
			}
			if (a_v != 0xFF && (!v || *v != a_v)) {
				Byte(a_slot, L"SetVerticalAlignment", a_v);
				changed = true;
			}
			return changed;
		}

		// the laid-out height a widget asks for, its slot's padding included
		double Height(UE::UObject* a_w)
		{
			if (!a_w) return 0.0;
			ue::Call c(a_w, L"GetDesiredSize");
			if (!c || !c.Run()) return 0.0;
			double h = 0.0;
			if (c.Size("ReturnValue") == 16) {
				h = c.Get<std::array<double, 2>>("ReturnValue")[1];
			} else if (c.Size("ReturnValue") == 8) {
				h = c.Get<std::array<float, 2>>("ReturnValue")[1];
			}
			const auto pad = Padding(SlotOf(a_w));
			return h + pad[1] + pad[3];
		}

		bool SpacerIs(UE::UObject* a_spacer, double a_height)
		{
			auto* size = Prop<std::array<double, 2>>(a_spacer, "Size");
			if (!a_spacer || (size && std::abs((*size)[1] - a_height) < 0.5)) return false;
			Vec2(a_spacer, L"SetSize", size ? (*size)[0] : 1.0, a_height);
			return true;
		}

		bool SizeBoxIs(UE::UObject* a_box, double a_w, double a_h)
		{
			bool changed = false;
			changed |= FloatIs(a_box, "WidthOverride", L"SetWidthOverride", static_cast<float>(a_w));
			changed |= FloatIs(a_box, "HeightOverride", L"SetHeightOverride", static_cast<float>(a_h));
			return changed;
		}

		bool ColourIs(UE::UObject* a_userWidget, const std::array<float, 4>& a_want)
		{
			auto* c = Prop<std::array<float, 4>>(a_userWidget, "ColorAndOpacity");
			if (!a_userWidget) return false;
			if (c) {
				bool same = true;
				for (std::size_t i = 0; i < 4; ++i) same = same && std::abs((*c)[i] - a_want[i]) < 0.002f;
				if (same) return false;
			}
			ue::CallFirst(a_userWidget, L"SetColorAndOpacity", a_want.data(), sizeof(float) * 4);
			return true;
		}

		// ---- the art: our own PNGs (tools/gen-art.py), imported at run time ----
		UE::UObject* Import(ue::Handle& a_cache, const wchar_t* a_file)
		{
			if (auto* t = a_cache.Get()) return t;
			const auto path = settings::PluginFolder() / L"SundialWaitMenu" / a_file;
			static auto* lib = ue::Class(L"/Script/Engine.KismetRenderingLibrary");
			ue::Call c(lib ? lib->GetDefaultObject(false) : nullptr, L"ImportFileAsTexture2D");
			void* file = c.At("Filename");
			auto* pc = ue::PlayerController();
			UE::UObject* tex = nullptr;
			std::error_code ec;
			if (c && file && pc && std::filesystem::exists(path, ec)) {
				c.Set("WorldContextObject", pc);
				auto* s = new (file) UE::FString(path.wstring().c_str());
				tex = c.RunGuarded() ? c.Get<UE::UObject*>("ReturnValue") : nullptr;   // a world-context call: fault-guarded
				s->~FString();
			}
			if (!tex) {
				logger::warn("dial: {} could not be imported ({}{})", path.filename().string(), path.string(),
					std::filesystem::exists(path, ec) ? (pc ? "" : ", no player controller yet") : ", missing");
				return nullptr;
			}
			a_cache.Set(tex);
			logger::info("dial: {} imported ({})", path.filename().string(), ue::NameOf(tex));
			return tex;
		}

		bool Art()
		{
			if (g_artFailed) return false;
			const bool ok = Import(g_texFace, L"Face.png") && Import(g_texHour, L"Hour.png") && Import(g_texMarks, L"Marks.png") &&
			                Import(g_texSun, L"Sun.png");
			if (!ok) {
				g_artFailed = true;   // asked again only on request (sundial.drive reloadArt): the menu stays the game's own
				Status("the dial's pictures are missing - the game's own wait menu is left as it is");
			}
			return ok;
		}

		bool Brush(UE::UObject* a_image, UE::UObject* a_texture)
		{
			if (!a_image || !a_texture) return false;
			ue::Call c(a_image, L"SetBrushFromTexture");
			c.Set("Texture", a_texture);
			c.Set("bMatchSize", false);
			return c.Run();
		}

		// a new image filling a_overlay, drawn but never taking input
		UE::UObject* NewLayer(UE::UObject* a_tree, UE::UObject* a_overlay, const std::wstring& a_name, UE::UObject* a_texture)
		{
			static auto* imageClass = ue::Class(L"/Script/UMG.Image");
			auto* img = imageClass ? UE::NewObject<UE::UObject>(a_tree, imageClass, UE::FName(a_name.c_str())) : nullptr;
			if (!img) return nullptr;
			ue::Call add(a_overlay, L"AddChildToOverlay");
			add.Set("Content", img);
			if (!add.Run() || !add.Get<UE::UObject*>("ReturnValue")) return nullptr;
			AlignIs(add.Get<UE::UObject*>("ReturnValue"), kFill, kFill);
			Brush(img, a_texture);
			Vec2(img, L"SetRenderTransformPivot", 0.5, 0.5);
			Visibility(img, kHitTestInvisible);
			return img;
		}

		// ---- the menu's parts ----
		bool FindParts(UE::UObject* a_menu)
		{
			g_p = {};
			auto* tile = Find(a_menu, "BackgroundImage");
			auto* main = Find(a_menu, "Main_VerticalBox");
			if (!tile || !main) return false;
			g_p.tile.Set(tile);
			g_p.main.Set(main);
			g_p.tileImage.Set(Find(tile, "Image"));
			g_p.sizer.Set(Find(tile, "Sizer"));
			g_p.invalidation.Set(Find(tile, "InvalidationBox"));
			g_p.dropShadow.Set(Find(a_menu, "DropShadow"));
			g_p.header.Set(Find(a_menu, "Header_HorizontalBox"));
			g_p.spacerTop.Set(Find(a_menu, "Spacer_3"));
			g_p.spacerBottom.Set(Find(a_menu, "Spacer_5"));
			g_p.buttons.Set(Find(a_menu, "sleep_buttons_horizontal_box"));
			auto* slider = Find(a_menu, "sleep_slider");
			g_p.slider.Set(slider);
			g_p.sliderBar.Set(slider ? Find(slider, "Slider") : nullptr);
			const char* middle[6] = { "Time_Text", "Spacer_2", "sleep_slider", "Spacer_4", "Date_Text", "Seed_Text" };
			for (std::size_t i = 0; i < 6; ++i) g_p.middle[i].Set(Find(a_menu, middle[i]));
			g_p.faceTexts[0] = g_p.middle[0];
			g_p.faceTexts[1] = g_p.middle[4];
			g_p.faceTexts[2] = g_p.middle[5];
			return g_p.sizer.Get() && g_p.header.Get() && g_p.spacerTop.Get() && g_p.spacerBottom.Get() && g_p.buttons.Get();
		}

		// ---- building the dial into the menu's background tile ----
		bool Build(UE::UObject* a_menu)
		{
			const ULONGLONG now = GetTickCount64();
			if (g_lastBuildTry && now - g_lastBuildTry < 1000) return false;   // rule 17: tried again, at most every second
			g_lastBuildTry = now;
			if (!FindParts(a_menu)) {
				Status("the menu's parts were not all found - the game's own wait menu is left as it is");
				return false;
			}
			if (!Art()) return false;
			static auto* overlayClass = ue::Class(L"/Script/UMG.Overlay");
			static auto* sizeBoxClass = ue::Class(L"/Script/UMG.SizeBox");
			auto* overlay = Find(g_p.tile.Get(), "Overlay_0");   // inside the tile's size box, behind Main_VerticalBox
			auto* tree = overlay ? overlay->GetOuter() : nullptr;
			if (!overlay || !tree || !overlayClass || !sizeBoxClass) {
				Status("the background tile has no overlay to draw in - the game's own wait menu is left as it is");
				return false;
			}
			++g_builds;
			const std::wstring n = std::to_wstring(g_builds);
			g_b = {};
			auto* box = UE::NewObject<UE::UObject>(tree, sizeBoxClass, UE::FName((L"SundialBox" + n).c_str()));
			auto* stack = UE::NewObject<UE::UObject>(tree, overlayClass, UE::FName((L"SundialStack" + n).c_str()));
			if (!box || !stack) return false;
			ue::Call addBox(overlay, L"AddChildToOverlay");
			addBox.Set("Content", box);
			auto* boxSlot = addBox.Run() ? addBox.Get<UE::UObject*>("ReturnValue") : nullptr;
			ue::Call addStack(box, L"AddChild");
			addStack.Set("Content", stack);
			if (!boxSlot || !addStack.Run()) return false;
			AlignIs(boxSlot, kCentre, kTop);
			Visibility(box, kHitTestInvisible);

			auto* face = NewLayer(tree, stack, L"SundialFace" + n, g_texFace.Get());
			for (int k = 0; k < kHours; ++k) {
				auto* h = NewLayer(tree, stack, L"SundialHour" + n + L"_" + std::to_wstring(k), g_texHour.Get());
				g_b.hours[static_cast<std::size_t>(k)].Set(h);
				g_b.angle[static_cast<std::size_t>(k)] = -1000.0f;
				g_b.shown[static_cast<std::size_t>(k)] = true;
			}
			auto* marks = NewLayer(tree, stack, L"SundialMarks" + n, g_texMarks.Get());
			auto* sun = NewLayer(tree, stack, L"SundialSun" + n, g_texSun.Get());
			g_b.menu.Set(a_menu);
			g_b.overlay.Set(overlay);
			g_b.box.Set(box);
			g_b.boxSlot.Set(boxSlot);
			g_b.stack.Set(stack);
			g_b.face.Set(face);
			g_b.marks.Set(marks);
			g_b.sun.Set(sun);
			g_b.sunAngle = -1000.0f;
			g_shownHours = -1;
			g_layout = json::object();

			// the tile caches what it draws; ours changes as the hours pass, so it draws live
			if (auto* inv = g_p.invalidation.Get()) {
				const bool no = false;
				ue::CallFirst(inv, L"SetCanCache", &no, sizeof(no));
			}
			auto* sizer = g_p.sizer.Get();
			const auto tilePad = Padding(SlotOf(g_p.tile.Get()));
			const auto mainPad = Padding(SlotOf(g_p.main.Get()));
			logger::info("dial: built into {} (build {}): the tile's size box had {:.0f} x {:.0f}, min {:.0f} x {:.0f}, max {:.0f} x {:.0f}; the tile's padding {:.0f}/{:.0f}/{:.0f}/{:.0f}, "
						 "the texts' padding {:.0f}/{:.0f}/{:.0f}/{:.0f}",
				ue::NameOf(a_menu), g_builds, Prop<float>(sizer, "WidthOverride") ? *Prop<float>(sizer, "WidthOverride") : -1.0f,
				Prop<float>(sizer, "HeightOverride") ? *Prop<float>(sizer, "HeightOverride") : -1.0f,
				Prop<float>(sizer, "MinDesiredWidth") ? *Prop<float>(sizer, "MinDesiredWidth") : -1.0f,
				Prop<float>(sizer, "MinDesiredHeight") ? *Prop<float>(sizer, "MinDesiredHeight") : -1.0f,
				Prop<float>(sizer, "MaxDesiredWidth") ? *Prop<float>(sizer, "MaxDesiredWidth") : -1.0f,
				Prop<float>(sizer, "MaxDesiredHeight") ? *Prop<float>(sizer, "MaxDesiredHeight") : -1.0f, tilePad[0], tilePad[1], tilePad[2], tilePad[3],
				mainPad[0], mainPad[1], mainPad[2], mainPad[3]);
			return face && marks && sun;
		}

		bool StillBuilt(UE::UObject* a_menu)
		{
			if (g_b.menu.Get() != a_menu || !g_b.box.Get() || !g_b.stack.Get() || !g_p.tile.Get() || !g_p.main.Get()) return false;
			auto* parent = ObjProp(g_b.boxSlot.Get(), "Parent");
			return parent && parent == g_b.overlay.Get();
		}

		// ---- the layout: the title above the dial, the hours / time / date centred on its face, the buttons below ----
		void Layout()
		{
			const auto& s = settings::Get();
			const double D = s.dialSize;

			// the game's flat panel art out of the way; the slider stays where it is, working, unseen
			OpacityIs(g_p.dropShadow.Get(), 0.0f);
			OpacityIs(g_p.tileImage.Get(), 0.0f);
			OpacityIs(g_p.slider.Get(), 0.0f);
			if (s.inkText) {
				for (auto& t : g_p.faceTexts) ColourIs(t.Get(), kInk);
			}

			const double hh = Height(g_p.header.Get());
			double m = 0.0;
			for (auto& w : g_p.middle) m += Height(w.Get());
			const double hb = Height(g_p.buttons.Get());
			if (hh <= 0.0 || m <= 0.0 || hb <= 0.0) return;   // not laid out yet: next time
			const auto padTop = Padding(SlotOf(g_p.spacerTop.Get()));
			const auto padBottom = Padding(SlotOf(g_p.spacerBottom.Get()));
			const double dialTop = hh + s.titleGap;
			const double s3 = std::max(0.0, dialTop + D / 2 - m / 2 - hh - padTop[1] - padTop[3]);
			const double s5 = std::max(0.0, D / 2 + s.buttonGap - m / 2 - padBottom[1] - padBottom[3]);
			const double total = dialTop + D + s.buttonGap + hb;

			// the texts' box from the top; the tile as tall as all of it, the dial at the top of the tile below the title
			const auto mainPad = Padding(SlotOf(g_p.main.Get()));
			const auto tilePad = Padding(SlotOf(g_p.tile.Get()));
			const double offset = std::max(0.0, static_cast<double>(mainPad[1]) - tilePad[1]);
			bool changed = false;
			changed |= AlignIs(SlotOf(g_p.main.Get()), 0xFF, kTop);
			changed |= AlignIs(SlotOf(g_p.tile.Get()), 0xFF, kTop);
			changed |= SpacerIs(g_p.spacerTop.Get(), s3);
			changed |= SpacerIs(g_p.spacerBottom.Get(), s5);
			changed |= SizeBoxIs(g_p.sizer.Get(), D, offset + total);
			changed |= SizeBoxIs(g_b.box.Get(), D, D);
			changed |= PaddingIs(g_b.boxSlot.Get(), { 0.0f, static_cast<float>(offset + dialTop), 0.0f, 0.0f });

			const json now = { { "dial", D }, { "title", std::lround(hh) }, { "middle", std::lround(m) }, { "buttons", std::lround(hb) }, { "spacerAbove", std::lround(s3) },
				{ "spacerBelow", std::lround(s5) }, { "tileHeight", std::lround(offset + total) }, { "dialTop", std::lround(offset + dialTop) } };
			if (now != g_layout) {
				if (g_layout.empty()) {
					logger::info("dial: laid out - {}", now.dump());
				} else {
					logger::debug("dial: laid out again - {}", now.dump());
				}
				g_layout = now;
			} else if (changed) {
				logger::debug("dial: the menu put a value of its own back - set again");
			}
		}

		// ---- the hours ----
		float GameHour()
		{
			if (auto* cal = RE::Calendar::GetSingleton(); cal && cal->gameHour) {
				const float h = cal->gameHour->value;
				if (h >= 0.0f && h <= 24.0f) return h;
			}
			if (auto* g = RE::TESForm::LookupByID<RE::TESGlobal>(0x38)) {   // GameHour
				if (g->value >= 0.0f && g->value <= 24.0f) return g->value;
			}
			return -1.0f;
		}

		int SliderHours()
		{
			auto* bar = g_p.sliderBar.Get();
			auto* v = Prop<float>(bar, "Value");
			auto* lo = Prop<float>(bar, "MinValue");
			auto* hi = Prop<float>(bar, "MaxValue");
			if (!v || !lo || !hi) return -1;
			int hours = -1;
			if (*hi - *lo >= 2.0f) {
				hours = static_cast<int>(std::lround(*v));
			} else if (*hi > *lo) {
				hours = static_cast<int>(std::lround(1.0f + (*v - *lo) / (*hi - *lo) * 23.0f));   // a 0-1 slider over the menu's 1-24
			}
			static float logged = -1000.0f;
			if (*v != logged) {
				if (logged < -999.0f) logger::info("dial: the hours slider reads {:.3f} of {:.0f}-{:.0f} -> {} hours", *v, *lo, *hi, hours);
				else logger::debug("dial: the hours slider reads {:.3f} -> {} hours", *v, hours);
				logged = *v;
			}
			return std::clamp(hours, 0, kHours);
		}

		void Update(UE::UObject* a_vm)
		{
			const float hour = GameHour();
			auto*       tw = Prop<float>(a_vm, "TimeToWait");
			if (hour < 0.0f || !tw) return;
			// Waiting or still choosing? Two signs, either enough: the game hour JUMPS (a wait moves it an hour a second;
			// the clock running while the menu is open moves it well under a minute between two reads), or TimeToWait moves
			// away from what it held once the menu had settled (it takes the chosen hours at the confirm, then counts down).
			// Until then the hours are the slider's. If the slider writes TimeToWait as it moves, the two agree anyway.
			const ULONGLONG nowMs = GetTickCount64();
			if (nowMs - g_upAt < 500 || g_t0 < 0.0f) {
				g_t0 = *tw;   // settling: the menu may set its own value a frame or two after it comes up
			}
			if (!g_counting) {
				const float jump = g_lastHour < 0.0f ? 0.0f : std::fmod(hour - g_lastHour + 24.0f, 24.0f);
				const bool  jumped = jump > 0.5f && jump < 23.5f;
				const bool  moved = nowMs - g_upAt >= 500 && std::abs(*tw - g_t0) > 0.01f;
				if (jumped || moved) {
					g_counting = true;
					logger::info("dial: the wait has begun ({}) - TimeToWait {:.0f}, the hour {:.2f}", jumped ? "the hour jumped" : "TimeToWait moved", *tw, hour);
				}
			}
			g_lastHour = hour;
			int hours = g_counting ? static_cast<int>(std::lround(*tw)) : SliderHours();
			if (const int p = g_preview.load(); p >= 0) hours = p;
			hours = std::clamp(hours, 0, kHours);

			// noon at the top, clockwise: hour h is at (h - 12) * 15 degrees; the hours lit run from now
			const float a0 = (hour - 12.0f) * 15.0f;
			for (int k = 0; k < kHours; ++k) {
				const auto u = static_cast<std::size_t>(k);
				auto*      img = g_b.hours[u].Get();
				if (!img) continue;
				const bool show = k < hours;
				if (show != g_b.shown[u]) {
					Visibility(img, show ? kHitTestInvisible : kCollapsed);
					g_b.shown[u] = show;
				}
				const float a = std::fmod(a0 + 15.0f * k + 720.0f, 360.0f);
				if (show && std::abs(a - g_b.angle[u]) > 0.05f) {
					Float(img, L"SetRenderTransformAngle", a);
					g_b.angle[u] = a;
				}
			}
			const float sa = std::fmod(a0 + 720.0f, 360.0f);
			if (std::abs(sa - g_b.sunAngle) > 0.05f) {
				Float(g_b.sun.Get(), L"SetRenderTransformAngle", sa);
				g_b.sunAngle = sa;
			}
			if (hours != g_shownHours) {
				logger::debug("dial: {} hours on the rim from {:.2f} ({})", hours, hour, g_counting ? "waiting" : "choosing");
				g_shownHours = hours;
			}
		}

		// ---- finding the menu ----
		UE::UClass* MenuClass()
		{
			if (g_menuClass && ue::IsLive(g_menuClass)) return g_menuClass;
			g_menuClass = nullptr;
			const ULONGLONG now = GetTickCount64();
			if (now - g_classScanAt >= 1000) {
				g_classScanAt = now;
				g_menuClass = UE::StaticFindObject<UE::UClass>(nullptr, nullptr, kMenuClass);
			}
			if (!g_menuClass && now - g_nameScanAt >= 5000) {   // by its name, if the path ever differs (rule 17: at most every 5 s)
				g_nameScanAt = now;
				auto* arr = UE::FUObjectArray::GetSingleton();
				if (arr) {
					arr->LockInternalArray();
					const std::int32_t n = arr->GetObjectArrayNum();
					for (std::int32_t i = 0; i < n && !g_menuClass; ++i) {
						auto* item = arr->IndexToObject(i);
						auto* o = item ? reinterpret_cast<UE::UObject*>(item->object) : nullptr;
						if (o && o->GetClass() && ue::NameOf(o->GetClass()) == "WidgetBlueprintGeneratedClass" && ue::NameOf(o) == kMenuClassName) {
							g_menuClass = static_cast<UE::UClass*>(o);
						}
					}
					arr->UnlockInternalArray();
				}
			}
			if (g_menuClass) logger::info("dial: the wait menu's class is {}", ue::NameOf(g_menuClass));
			return g_menuClass;
		}

		// the wait menu is a CommonUI activatable widget: the game's layer stack shows it through Slate, so its UMG Slot stays
		// empty while it is up (read in game 2026-10-01: Slot null on the live WBP_ModernMenu_SleepWait_C) - bIsActive is
		// what says it is the menu in use; a Slot still counts if a future build parents it in UMG
		bool Placed(UE::UObject* a_menu)
		{
			auto* active = Prop<bool>(a_menu, "bIsActive");
			return a_menu && (SlotOf(a_menu) || (active && *active));
		}

		bool OnScreen(UE::UObject* a_menu)
		{
			auto* v = Prop<std::uint8_t>(a_menu, "Visibility");
			auto* o = Prop<float>(a_menu, "RenderOpacity");
			return Placed(a_menu) && (!v || (*v != kCollapsed && *v != kHidden)) && (!o || *o > 0.05f);
		}

		UE::UObject* Menu(UE::UObject* a_vm)
		{
			auto* vis = Prop<bool>(a_vm, "bVisible");
			if (!vis || !*vis) return nullptr;
			auto* m = g_menu.Get();
			if (m && Placed(m)) return m;
			const ULONGLONG now = GetTickCount64();
			if (now - g_menuScanAt < 1000) return nullptr;   // the object array scan: at most once a second, only while the view model says it is up
			g_menuScanAt = now;
			auto* cls = MenuClass();
			if (!cls) return nullptr;
			auto* arr = UE::FUObjectArray::GetSingleton();
			UE::UObject* found = nullptr;
			if (arr) {
				arr->LockInternalArray();
				const std::int32_t n = arr->GetObjectArrayNum();
				for (std::int32_t i = 0; i < n && !found; ++i) {
					auto* item = arr->IndexToObject(i);
					auto* o = item ? reinterpret_cast<UE::UObject*>(item->object) : nullptr;
					// a template (class default or archetype, flags 0x30) is never the menu on screen
					if (o && o->GetClass() == cls && (static_cast<std::int32_t>(o->objectFlags) & 0x30) == 0 && Placed(o)) found = o;
				}
				arr->UnlockInternalArray();
			}
			if (found && found != m) logger::info("dial: the wait menu is {}", ue::NameOf(found));
			g_menu.Set(found);
			return found;
		}

		void Publish(UE::UObject* a_vm)
		{
			json j = { { "status", g_status }, { "up", g_up }, { "builds", g_builds }, { "layout", g_layout }, { "counting", g_counting },
				{ "hoursOnRim", g_shownHours }, { "preview", g_preview.load() }, { "gameHour", GameHour() }, { "viewModel", a_vm ? ue::NameOf(a_vm) : "not found" },
				{ "menu", g_menu.Get() ? ue::NameOf(g_menu.Get()) : "not found" }, { "art", { { "face", static_cast<bool>(g_texFace.Get()) },
					{ "hour", static_cast<bool>(g_texHour.Get()) }, { "marks", static_cast<bool>(g_texMarks.Get()) }, { "sun", static_cast<bool>(g_texSun.Get()) } } } };
			if (a_vm) {
				auto* tw = Prop<float>(a_vm, "TimeToWait");
				auto* sleep = Prop<bool>(a_vm, "bIsSleep");
				auto* waiting = Prop<bool>(a_vm, "bIsWaitingInProgress");
				j["timeToWait"] = tw ? *tw : -1.0f;
				j["isSleep"] = sleep && *sleep;
				j["waitingInProgress"] = waiting && *waiting;
			}
			std::scoped_lock l(g_stateLock);
			g_state = std::move(j);
		}
	}

	void Tick()
	{
		if (!settings::Get().enabled || !ue::SelfCheck()) return;
		{
			std::vector<Action> q;
			{
				std::scoped_lock l(g_queueLock);
				q.swap(g_queue);
			}
			for (const auto a : q) {
				if (a == Action::kReapply) {
					g_b = {};
					g_lastBuildTry = 0;
					logger::info("dial: rebuild requested");
				} else if (a == Action::kReloadArt) {
					g_texFace = {}, g_texHour = {}, g_texMarks = {}, g_texSun = {};
					g_artFailed = false;
					g_b = {};
					g_lastBuildTry = 0;
					logger::info("dial: the pictures are imported again");
				}
			}
		}

		const ULONGLONG now = GetTickCount64();
		if (now < g_nextUpdate) return;
		g_nextUpdate = now + (g_up ? 100 : 250);   // 10 a second while the menu is up, 4 otherwise

		auto* vm = g_vm.Get();
		if (!vm && now - g_vmScanAt >= 2000) {
			g_vmScanAt = now;
			vm = ue::FirstOf(ue::Class(kVmClass));
			g_vm.Set(vm);
			if (vm) logger::info("dial: the wait menu's view model is {}", ue::NameOf(vm));
		}
		auto*      menu = vm ? Menu(vm) : nullptr;
		const bool up = menu && OnScreen(menu);
		if (up != g_up) {
			g_up = up;
			if (up) {
				g_upAt = now;
				g_t0 = -1.0f;
				g_lastHour = -1.0f;
				g_counting = false;
				g_shownHours = -1;
				auto* sleep = Prop<bool>(vm, "bIsSleep");
				logger::info("dial: the {} menu is up", sleep && *sleep ? "sleep" : "wait");
			} else {
				logger::info("dial: the menu is gone");
			}
		}
		if (up) {
			if (!StillBuilt(menu) && !Build(menu)) {
				Publish(vm);
				return;
			}
			if (now >= g_nextLayout) {
				g_nextLayout = now + 250;
				Layout();
			}
			Update(vm);
			Status(std::format("drawn in {}", ue::NameOf(menu)));
		}
		Publish(vm);
	}

	json State()
	{
		std::scoped_lock l(g_stateLock);
		return g_state;
	}

	void Queue(Action a_action)
	{
		std::scoped_lock l(g_queueLock);
		g_queue.push_back(a_action);
	}

	void PreviewHours(int a_hours)
	{
		g_preview.store(std::clamp(a_hours, -1, kHours));
	}
}
