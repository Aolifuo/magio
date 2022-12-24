set_project("magio")
set_version("0.0.7")

--set_config("cxx", "clang-cl")
add_rules("mode.debug", "mode.release")
set_languages("cxx20")
set_warnings("all")
add_requires("fmt")
add_packages("fmt")

if is_mode("release") then 
    set_optimize("faster")
end

if is_plat("linux") then
    add_requires("liburing")
    add_syslinks("pthread")
    add_packages("liburing")
end

if is_plat("windows") then 
    add_cxxflags("/EHa")
    add_syslinks("ws2_32")
end

target("magio")
    set_kind("static")
    add_files("src/magio/**.cpp")
    add_includedirs("src", {public = true})

target("magio-v3")
    set_kind("static")
    add_files("src/magio-v3/**.cpp")
    add_includedirs("src", {public = true})

--tests
for _, dir in ipairs(os.files("tests/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        set_group("tests")
        add_files(dir)
        add_deps("magio")
end

--examples
for _, dir in ipairs(os.files("examples/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio")
end

--dev
for _, dir in ipairs(os.files("dev/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio-v3")
end

--xmake project -k compile_commands