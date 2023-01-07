set_toolchains("clang")
set_config("cxxflags", "-stdlib=libc++")
set_config("ldflags", "-stdlib=libc++")

set_project("magio")
set_version("0.0.9")

add_rules("mode.debug", "mode.release")
set_languages("cxx20")
set_warnings("all")

if is_mode("release") then 
    set_optimize("faster")
end

if is_plat("linux") then
    add_syslinks("pthread")
    add_requires("liburing")
    add_packages("liburing")
end

if is_plat("windows") then 
    add_syslinks("ws2_32")
end

add_requires("fmt")
add_packages("fmt")

add_defines("MAGIO_USE_CORO")

target("magio-v3")
    set_kind("static")
    add_files("src/magio-v3/core/**.cpp", "src/magio-v3/net/**.cpp")
    add_includedirs("src", {public = true})

-- v3 examples
for _, dir in ipairs(os.files("examples/v3/*.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio-v3")
end

--dev
for _, dir in ipairs(os.files("dev/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio-v3")
end

--xmake project -k compile_commands