project "TempServer"
    kind "StaticLib"
    language "C++"
	staticruntime "off"
	cppdialect "C++17"
	targetdir ("../bin/" .. outputdir .. "/%{prj.name}") 
	objdir   ("../bin-int/" .. outputdir .. "/%{prj.name}") 
	
    pchheader "Spch.h"
    pchsource "src/Spch.cpp"

	defines
	{
	    "_CRT_SECURE_NO_WARNINGS", 
		"YAML_CPP_STATIC_DEFINE"
	}
	
	files
	{
		"src/**.h",
		"src/**.cpp",
		"third_party/readerwriterqueue/readerwriterqueue.h",
		"third_party/readerwriterqueue/atomicops.h"
	}

	includedirs
	{
		"third_party/spdlog/include",
		"src/Server",
		"src",
		"third_party/readerwriterqueue"

	}


	filter { "configurations:Debug" }
			defines { "SERVER_DEBUG"}
			symbols "On"
			runtime "Debug" -- ����ʱ���ӵ�dll��debug���͵�	
			if _ACTION == "vs2022" then
				buildoptions "/MTd"
			end
			-- in VS2019 that is Additional Library Directories
			
			
			--links
			--{
				--"spirv-cross-cored.lib",
				--"spirv-cross-glsld.lib",
				--"SPIRV-Toolsd.lib",
				--"libmono-static-sgen.lib"
			--}
		

		filter { "configurations:Release"}
			defines { "SERVER_RELEASE"}
			optimize "On"
			runtime "Release" -- ����ʱ���ӵ�dll��release���͵�
		if _ACTION == "vs2022" then
			buildoptions "/MT"
		end
		-- in VS2019 that is Additional Library Directories
		
		
		


		filter { "configurations:Dist"}
			defines { "SERVER_DIST"}
			optimize "On"
		if _ACTION == "vs2022" then
			buildoptions "/MT"
		end

	filter  "system:windows" 
	    systemversion "latest"
		defines {"SERVER_PLATFORM_WINDOWS", "SERVER_ENABLE_ASSERTS" }
		
		postbuildcommands
		{
		    -- "copy default.config bin\\project.config"
			-- copy from relative path to ... ע�������COPYǰ��û��%
		    ("{COPY} %{cfg.buildtarget.relpath} \"../bin/" ..outputdir.."/Sandbox/\"")
		}
		links
		{
		    -- windows needed libs for mono
			"Ws2_32.lib",
			"Bcrypt.lib",
			"Version.lib",
			"Winmm.lib"
		}

		







