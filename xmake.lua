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

includes("src", "examples")

--xmake project -k compile_commands