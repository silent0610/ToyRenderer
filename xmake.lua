add_rules("mode.debug", "mode.release")

-- set_config("mode","debug")
set_config("builddir", "Build")
set_policy("build.c++.modules",true)
add_requires("glfw", "spdlog", "vulkansdk", "tinygltf v3.0.0", "imgui", "glm", "nlohmann_json", "stb")

-- CUDA WideBVH SDF (vendored NVIDIA cuBQL).
-- CUDA 13.1 + default MSVC 14.51 crashes cudafe++; compile .cu with host cl 14.38 via a custom rule
-- because xmake's -ccbin= path-with-spaces breaks nvcc's argument parsing.
rule("cubql_nvcc")
    set_extensions(".cu")
    on_build_file(function (target, sourcefile, opt)
        import("core.project.depend")
        import("utils.progress")
        local objectfile = target:objectfile(sourcefile)
        os.mkdir(path.directory(objectfile))
        local bat = path.join(path.directory(objectfile), path.basename(sourcefile) .. "_nvcc.bat")
        local lines = {
            "@echo off",
            'call "E:\\pf\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" -vcvars_ver=14.38 >nul || exit /b 1',
            string.format(
                '"E:\\pf\\Cuda\\13.1\\bin\\nvcc.exe" -c -O3 -std=c++17 --extended-lambda --use_fast_math -rdc=false -allow-unsupported-compiler -ccbin "E:\\pf\\Microsoft Visual Studio\\18\\Community\\VC\\Tools\\MSVC\\14.38.33130\\bin\\Hostx64\\x64" -Xcompiler "/Zc:preprocessor" -Xcompiler "/EHsc" -Xcompiler "/MD" -m64 -gencode arch=compute_75,code=sm_75 -DNDEBUG -I"%s" -I"%s" -o "%s" "%s"',
                path.absolute("Renderer/Source/Function/CubqlBvh"),
                path.absolute("Renderer/ThirdParty/cuBQL"),
                path.absolute(objectfile),
                path.absolute(sourcefile)),
            "exit /b %ERRORLEVEL%"
        }
        io.writefile(bat, table.concat(lines, "\r\n") .. "\r\n")

        depend.on_changed(function ()
            progress.show(opt.progress, "${color.build.object}compiling.cuda %s", sourcefile)
            os.execv(bat, {})
        end, {dependfile = target:dependfile(objectfile), files = sourcefile, values = lines})
        table.insert(target:objectfiles(), objectfile)
    end)

target("CubqlBvh")
    set_kind("static")
    set_languages("c++17")
    add_rules("cubql_nvcc")
    add_files("Renderer/Source/Function/CubqlBvh/CubqlBvhGpu.cu")
    add_headerfiles("Renderer/Source/Function/CubqlBvh/CubqlBvhGpu.h")
    add_includedirs("Renderer/Source/Function/CubqlBvh", {public = true})
    add_includedirs("Renderer/ThirdParty/cuBQL", {public = true})
    add_linkdirs("E:/pf/Cuda/13.1/lib/x64", {public = true})
    add_syslinks("cudart", {public = true})

target("MyToyRenderer")
    set_kind("binary")
    set_languages("c++latest")
    add_deps("CubqlBvh")
    add_files("Renderer/Source/**.cpp")
    add_files("Renderer/Source/**.ixx")
    add_extrafiles("Renderer/shader/**.hlsl","Config.json5")
    add_extrafiles("xmake.lua", ".clang-format")   -- 包含 xmake.lua
    add_includedirs("Renderer/ThirdParty/")
    add_includedirs("Renderer/Source/Function/CubqlBvh")
    add_packages("spdlog", "vulkansdk", "glfw", "tinygltf", "imgui", "glm", "nlohmann_json", "stb")
    add_defines(string.format('PROJECT_ROOT=R"(%s)"', os.scriptdir():gsub("\\", "/")))
    add_includedirs("Renderer/ThirdParty/KTX/include", "Renderer/ThirdParty/KTX/lib", "Renderer/ThirdParty/KTX/Bin")
    add_linkdirs("Renderer/ThirdParty/KTX/Bin")
    add_links("ktx")

    after_build(function (target)
        import("core.project.depend")
        local dlls = os.files("Renderer/ThirdParty/KTX/Bin/*.dll")
        depend.on_changed(function ()
            print("Copying KTX DLL...")
            for _, file in ipairs(dlls) do
                print("  -> copying %s", path.filename(file))
                os.cp(file, target:targetdir())
            end
            print("Post-build copy finished.")
        end, {dependfile = target:dependfile("ktx.dlls"), files = dlls, values = {target:targetdir()}})
    end)
