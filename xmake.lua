set_toolchains("clang")
set_config("cxxflags", "-stdlib=libc++ -fcoroutines-ts")
set_config("ldflags", "-lc++")

set_project("magio")
set_version("0.0.3")

add_rules("mode.debug", "mode.release")
--add_cxxflags("/EHa", {force = true})
set_languages("cxx20")
set_warnings("all")

add_requires("fmt")

if is_mode("release") then 
    set_optimize("faster")
end

if is_plat("linux") then
    add_syslinks("pthread")
end

target("magio")
    set_kind("static")
    add_files("src/magio/**.cpp")
    add_includedirs("src", {public = true})

--tests
for _, dir in ipairs(os.files("tests/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        set_group("tests")
        add_files(dir)
        add_deps("magio")
        add_packages("fmt")
end

--examples
for _, dir in ipairs(os.files("examples/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio")
end

--xmake project -k compile_commands