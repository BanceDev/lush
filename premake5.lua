workspace("lush")
configurations({ "Debug", "Release" })

-- lush project
project("lush")
kind("ConsoleApp")
language("C")
targetdir("bin/%{cfg.buildcfg}/lush")

files({
	"src/**.h",
	"src/**.c",
})
defines({ 'LUSH_VERSION="0.0.1"' })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")
