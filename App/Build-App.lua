project "App"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    debugdir "%{wks.location}"

    files { 
        "Source/**.h", 
        "Source/**.cpp"
    }

    includedirs
    {
        "Source",
        "../Engine/Source",
        "../Dependencies",
        "../Dependencies/glfw/include",
        "../Dependencies/sdl2/include",
        "../Dependencies/glm",
        "../Dependencies/glm/gtc",
        "../Dependencies/VulkanMemoryAllocator/include",
        "../Dependencies/tinygltf",
        "%{os.getenv('VULKAN_SDK')}/Include"
    }


    links
    {
        "Engine"
    }

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    postbuildcommands {
        'if not exist "%{cfg.targetdir}\\..\\Resources" mkdir "%{cfg.targetdir}\\..\\Resources"',         -- Make resource folder if it does not exist
        'xcopy "%{prj.location}\\..\\Resources\\*" "%{cfg.targetdir}\\..\\Resources\\" /Q /E /Y /I > nul', -- Copy all resources to build folder
        '{COPY} "../Dependencies/SDL2/lib/x64/SDL2.dll" "%{cfg.targetdir}"' -- Copy SDL2.dll to app folder
    }
    
    filter "system:windows"
        systemversion "latest"
        defines { "WINDOWS" }

    filter "configurations:Debug"
        kind "ConsoleApp"
        runtime "Debug"
        symbols "On"
        defines { "DEBUG" }

    filter "configurations:Release"
        kind "ConsoleApp"
        runtime "Release"
        optimize "On"
        symbols "On"
        defines { "RELEASE" }

    filter "configurations:Dist"
        kind "WindowedApp"
        entrypoint "mainCRTStartup"
        runtime "Release"
        optimize "On"
        symbols "Off"
        defines { "DIST" }