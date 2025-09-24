
project "YAML_CPP"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
		
		"include/**.h"
	}

	externalincludedirs
	{
		"include"
	}

	files
	{
		"include/**.h",
		"src/**.cpp"
	}

	filter "configurations:Debug"
		runtime "Debug"
		if _ACTION == "vs2022" then
				buildoptions "/MTd"
		end
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		if _ACTION == "vs2022" then
				buildoptions "/MT"
		end
		optimize "on"

	filter "configurations:Dist"
		runtime "Release"
		if _ACTION == "vs2022" then
				buildoptions "/MT"
		end
		optimize "on"
        symbols "off" 
