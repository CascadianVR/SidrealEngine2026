project "Engine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    files { 
        "Source/**.h", 
        "Source/**.cpp",
        "../Dependencies/tinygltf/**.cpp"
    }

    defines { "GLFW_STATIC" }

    includedirs
    {
        "Source",
        "../Dependencies/glfw/include",
        "../Dependencies/SDL2/include",
        "../Dependencies/glm",
        "../Dependencies/glm/gtc",
        "../Dependencies/VulkanMemoryAllocator/include",
        "../Dependencies/tinygltf",
        "%{os.getenv('VULKAN_SDK')}/Include"
    }

    libdirs
    {
        "../Dependencies/slang/lib",
        "../Dependencies/glfw",
        "../Dependencies/SDL2/lib/x64",
        "%{os.getenv('VULKAN_SDK')}/Lib"
    }

    links {
        "glfw3",
        "slang",
        "vulkan-1",
        "SDL2"
    }

    defines
    {
        "SDL_MAIN_HANDLED",
        "SDL_STATIC"
    }

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"
        defines { }

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Dist"
        defines { "DIST" }
        runtime "Release"
        optimize "On"
        symbols "Off"