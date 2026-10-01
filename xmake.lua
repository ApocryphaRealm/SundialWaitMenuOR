-- Sundial Wait Menu, for The Elder Scrolls IV: Oblivion Remastered (OBSE64 plugin).
-- The game's wait and sleep menu drawn as a round dial: a parchment face of our own, the hours you wait lit along the
-- rim from the current hour, the day's four marks and a sun at the current hour (the owner, 2026-09-30).
-- rule 45: no build-machine paths in any compiled object - set BEFORE includes() so CommonLibOB64's own library
-- target gets it too. /d1trimfile is wrapped in a TABLE so xmake passes it as one quoted argument (Minimap Menu,
-- 2026-09-29). /PDBALTPATH:%_PDB% records only the PDB's file name (it ships beside the DLL).
add_cxflags({"/d1trimfile:$(projectdir)"}, {force = true, expand = false})
add_shflags("/PDBALTPATH:%_PDB%", {force = true})

includes("lib/commonlibob64")

set_project("SundialWaitMenu")
set_version("1.0.0")
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("nlohmann_json")

target("SundialWaitMenu")
    add_rules("commonlibob64.plugin", {
        name = "SundialWaitMenu",
        author = "ApocryphaRealm",
        description = "Sundial Wait Menu: the wait menu as a round dial (Oblivion Remastered)"
    })
    add_packages("nlohmann_json")
    add_syslinks("user32", "shell32", "ole32")
    on_load(function (target)
        target:add("defines", "SWM_VERSION=\"" .. (target:version() or "0.0.0") .. "\"")
    end)
    add_files("src/**.cpp")
    add_headerfiles("src/**.h", "include/**.h")
    add_includedirs("include", "src")
    set_pcxxheader("src/pch.h")
