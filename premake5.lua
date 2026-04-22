-- 引入 Qt 模块
require "premake-qt/qt"
local qt = premake.extensions.qt

workspace "YuiStream"
    architecture "x64"
    startproject "YuiStream"
    configurations { "Debug", "Release" }
    flags { "MultiProcessorCompile" }

    buildoptions { "/Zc:__cplusplus" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        symbols "On"
    filter {}

outputdir = "%{cfg.buildcfg}-x64"

-- Depend
-- FFmpeg
FFmpegDir    = "vendor/ffmpeg"
-- SDL2
SDL2Dir      = "vendor/SDL2"
-- GLAD
GLADDir      = "vendor/glad"

group "Dependencies"

project "Glad"
    location "vendor/glad"
    kind "StaticLib"
    language "C"
    staticruntime "Off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir    ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "%{GLADDir}/include/glad/glad.h",
        "%{GLADDir}/include/KHR/khrplatform.h",
        "%{GLADDir}/src/glad.c"
    }

    includedirs {
        "%{GLADDir}/include"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter {}

group ""

project "YuiStream"
    location "."
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir    ("bin/int/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp"
    }

    includedirs {
        "src",
        "%{GLADDir}/include",
        "%{FFmpegDir}/include",
        "%{SDL2Dir}/include"
    }

    links {
        "Glad",
        "opengl32"
    }

    defines {
        "PLATFORM_WINDOWS",
        "_CRT_SECURE_NO_WARNINGS"
    }

    -- FFmpeg 库链接
    libdirs {
        "%{FFmpegDir}/lib",
        "%{SDL2Dir}/lib/x64"
    }

    links {
        -- FFmpeg
        "avcodec",
        "avformat",
        "avutil",
        "swresample",
        "swscale",
        -- SDL2
        "SDL2",
        "SDL2main"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }
        links { "user32", "gdi32", "shell32" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"
        debugdir "%{cfg.targetdir}"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter {}

    -- Qt6
    --[[
    qt.enable()
    qtuseexternalinclude(true)

    local qtPath = os.getenv("QT6_DIR") or os.getenv("QT_DIR")
    if not qtPath then
        error("QT6_DIR (or QT_DIR) is NULL, please set Qt path")
    end

    qtpath(qtPath)
    qtmodules { "core", "gui", "widgets" }
    qtprefix "Qt6"

    filter "configurations:Debug"
        qtsuffix "d"

    filter {}
    --]]


    filter "system:windows"
        postbuildcommands {
            -- FFmpeg DLLs
            '{COPY} "%{wks.location}vendor/ffmpeg/bin/*.dll" "%{cfg.targetdir}"',
            -- SDL2 DLL
            '{COPY} "%{wks.location}vendor/SDL2/lib/x64/SDL2.dll" "%{cfg.targetdir}"',
            -- Shaders
            '{MKDIR} "%{cfg.targetdir}/assets/shaders"',
            '{COPY} "%{wks.location}assets/shaders/*.*" "%{cfg.targetdir}/assets/shaders"'
        }

    filter {}
