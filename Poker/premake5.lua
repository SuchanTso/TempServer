project "Poker"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir  ("../bin/"..outputdir.."/%{prj.name}")
	objdir  ("../bin-int/"..outputdir.."/%{prj.name}")
    
	files { "src/**.h", "src/**.cpp"}

	defines
	{
		"YAML_CPP_STATIC_DEFINE",
		"TSO_ENABLE_ASSERTS"
	}

    externalincludedirs
	{
        "%{wks.location}/TempServer/third_party/spdlog/include",
		"%{wks.location}/TempServer/src",
		"%{wks.location}/TempServer/third_party/yaml-cpp/include",
		"%{wks.location}/TempServer/third_party/readerwriterqueue",
		"%{wks.location}/%{prj.name}/src",


	}

	links { "YAML_CPP",
		"pthread",
		 "TempServer" }

		filter { "configurations:Debug"}
		defines { "DEBUG"}
		symbols "On"
		if _ACTION == "vs2022" then
			buildoptions "/MTd"
		end

	filter { "configurations:Release"}
		defines { "NDEBUG" }
		optimize "On"
		if _ACTION == "vs2022" then
			buildoptions "/MT"
		end

	filter { "configurations:Dist"}
		defines { "NDEBUG"}
		optimize "On"
		if _ACTION == "vs2022" then
			buildoptions "/MT"
		end

	

    filter  "system:windows" 
	    systemversion "latest"
		 defines { "SERVER_PLATFORM_WINDOWS"}
		 			
		links
		{
		    -- windows needed libs for mono
			"Ws2_32.lib",
			"Bcrypt.lib",
			"Version.lib",
			"Winmm.lib"
		}

		

	filter "system:macosx"
		defines{
				"SERVER_PLATFORM_MACOSX",

		}
		


