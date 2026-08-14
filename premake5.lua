newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon .. "/generator.v3.lua")

CreateWorkspace({name = "eightbit"})
	CreateProject({serverside = true})
		IncludeSDKCommon()
		IncludeSDKTier0()
		IncludeSDKTier1()
		IncludeDetouring()
		IncludeScanning()
		IncludeLuaShared()
		IncludeHelpersExtended()

		links("opus")
		includedirs("opus/include")

		filter({"platforms:x86_64"})
			libdirs {"opus/lib64"}

		filter({"platforms:x86"})
			libdirs {"opus/lib32"}

		filter("system:windows")
			links("ws2_32")

		filter("system:linux")
			-- Die CI baut auf einer neueren Toolchain als die Runtime typischer Server.
			-- Dynamisch gelinkt traegt das Modul dadurch eine GLIBCXX-Version ein, die die
			-- libstdc++ des Servers nicht kennt, und dlopen scheitert mit
			-- "version GLIBCXX_3.4.32 not found".
			-- --exclude-libs haelt die einkompilierten libstdc++/opus-Symbole aus der
			-- dynamischen Symboltabelle heraus, damit sie nicht mit denen der Engine kollidieren.
			linkoptions({"-static-libstdc++", "-static-libgcc", "-Wl,--exclude-libs,ALL"})
