set_toolchains("clang")
set_config("cxxflags", "-stdlib=libc++")
set_config("ldflags", "-stdlib=libc++")

set_project("magio")
set_version("0.1.0")

add_repositories("my-repo deps")
add_rules("mode.debug", "mode.release")

set_languages("cxx20")
set_warnings("all")

if is_mode("release") then 
    set_optimize("faster")
end

if is_plat("linux") then
    add_syslinks("pthread")
    add_requires("liburing")
end

if is_plat("windows") then 
    add_syslinks("ws2_32")
end

add_requires("fmt")
add_defines("MAGIO_USE_CORO")

target("magio-v3")
    set_kind("static")
    add_files("src/magio-v3/core/**.cpp", "src/magio-v3/net/**.cpp")
    add_includedirs("src", {public = true})
    add_packages("liburing")
    add_packages("fmt")

-- v3 examples
for _, dir in ipairs(os.files("examples/v3/*.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio-v3")
        add_packages("fmt", {links = {}})
end

--dev
for _, dir in ipairs(os.files("dev/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio-v3")
        add_packages("fmt", {links = {}})
end

--xmake project -k compile_commands