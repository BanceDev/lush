workspace("lush")
configurations({ "Debug", "Release" })

project("lush")
kind("ConsoleApp")
language("C")
targetdir("bin/%{cfg.buildcfg}/lush")

local lua_inc_path = "/usr/include"
links({ "lua5.4" })

-- Add all necessary include directories
includedirs({
    lua_inc_path,
    "lib/hashmap",
    "lib/compat53/c-api"
})

libdirs({ "/usr/lib" })

-- Compile all necessary source files, including the compat library
files({
    "src/**.h",
    "src/**.c",
    "lib/hashmap/hashmap.h",
    "lib/hashmap/hashmap.c",
    "lib/compat53/c-api/compat-5.3.h",
    "lib/compat53/c-api/compat-5.3.c"
})

defines({ 'LUSH_VERSION="0.3.2"', 'COMPAT53_PREFIX=""' })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")
