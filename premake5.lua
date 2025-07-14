workspace "TempServer"
    architecture "x64"
    configurations { "Debug", "Release", "Dist" }
	startproject "Server"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- group "third_party"
-- include "TsoEngine/third_party/GLFW"
-- include "TsoEngine/third_party/Glad"
-- include "TsoEngine/third_party/imgui"
-- include "TsoEngine/third_party/yaml-cpp"
-- include "TsoEngine/third_party/box2d"
-- include "TsoEngine/third_party/msdf-atlas-gen"
-- group ""



group "Server"
include "TempServer"
group ""

group "Misc"
include "Poker"
group ""