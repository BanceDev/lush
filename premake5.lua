workspace("lush")
configurations({ "Debug", "Release" })

-- lush project
project("lush")
kind("ConsoleApp")
language("C")
targetdir("bin/%{cfg.buildcfg}/lush")

local lua_inc_path = "/usr/include"
local lua_lib_path = "/usr/lib"

if os.findlib("lua5.4") then
	lua_inc_path = "/usr/include/lua5.4"
	lua_lib_path = "/usr/lib/5.4"
	links({ "lua5.4" })
else
	links({ "lua" })
end

includedirs({ 
	lua_inc_path, 
	"lib/hashmap",
    "lib/compat53/c-api"
})
libdirs({ lua_lib_path })

files({
	"src/**.h",
	"src/**.c",
	"lib/hashmap/**.h",
	"lib/hashmap/**.c",
    "lib/compat53/c-api/compat-5.3.h",
    "lib/compat53/c-api/compat-5.3.c",
    "lib/compat53/lbitlib.c",
    "lib/compat53/liolib.c",
    "lib/compat53/lstrlib.c",
    "lib/compat53/ltablib.c",
    "lib/compat53/lutf8lib.c"
})
defines({ 'LUSH_VERSION="0.3.2"', 'COMPAT53_PREFIX=""' })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")
