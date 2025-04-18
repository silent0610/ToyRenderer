add_rules("mode.debug", "mode.release")

set_config("buildir", "Build")

add_requires("glfw","spdlog","vulkansdk","glfw","tinygltf","imgui","glm","nlohmann_json")

target("MyToyRenderer")
    set_kind("binary") 
    set_languages("c++23")
    add_cxxflags("/std:c++latest")
    add_cxxflags("/utf-8")
    set_policy("build.c++.modules", true)

    add_files("Renderer/Source/*.cpp")
    add_files("Renderer/Source/*.ixx")
    add_headerfiles("Renderer/Source/*.hpp")
    add_includedirs("Renderer/ThirdParty/")
    add_packages("spdlog","vulkansdk","glfw","tinygltf","imgui","glm","nlohmann_json")
    set_runenv("PROJECT_PATH", os.projectdir())
    add_includedirs("Renderer/ThirdParty/KTX/include","Renderer/ThirdParty/KTX/lib","Renderer/ThirdParty/KTX/Bin")
    add_linkdirs("Renderer/ThirdParty/KTX/Bin")
    add_links("ktx")




