add_rules("mode.debug", "mode.release")

-- set_config("mode","debug")
set_config("vs","2026")
-- set_config("vs_toolset","14.44")
set_config("builddir", "Build")
set_policy("build.c++.modules",true)

add_requires("glfw", "spdlog", "vulkansdk", "tinygltf", "imgui", "glm", "nlohmann_json")
target("Engine")
    set_toolchains("msvc")
    set_kind("binary")
    set_languages("c++latest")
    add_files("Engine/Source/**.ixx")
    add_files("Engine/Source/**.cpp")
    add_includedirs("Engine/Source")
    add_extrafiles("Engine/Source/**.hpp")
    add_extrafiles("xmake.lua",".clang-format")   -- 包含 xmake.lua
    add_packages("spdlog", "vulkansdk", "glfw", "tinygltf", "imgui", "glm", "nlohmann_json")
    set_runenv("PROJECT_PATH", os.projectdir())

    after_build(function (target)
        -- $(projectdir) 是项目根目录
        -- target:targetdir() 是 exe 生成的目录 (例如 build/windows/x64/debug)
        
        -- 1. 拷贝 Shaders 文件夹
        os.cp("$(projectdir)/Engine/Asset", target:targetdir())
        
        -- 2. 如果你有 Assets 文件夹，也拷过去
        -- os.cp("$(projectdir)/Assets", target:targetdir())
        
        print("Assets copied to: " .. target:targetdir())
    end)

