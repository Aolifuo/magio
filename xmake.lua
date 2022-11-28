set_project("magio")
set_version("0.0.7")

add_rules("mode.debug", "mode.release")
set_languages("cxx20")
set_warnings("all")
add_requires("fmt")

if is_mode("release") then 
    set_optimize("faster")
end

if is_plat("linux") then
    add_requires("liburing")
    add_syslinks("pthread")
end

if is_plat("windows") then 
    add_cxxflags("/EHa")
    add_syslinks("ws2_32")
end

target("magio")
    set_kind("static")
    add_files("src/magio/**.cpp")
    add_includedirs("src", {public = true})
    add_packages("fmt")

--tests
for _, dir in ipairs(os.files("tests/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        set_group("tests")
        add_files(dir)
        add_deps("magio")
        add_packages("fmt", "liburing")
end

--examples
for _, dir in ipairs(os.files("examples/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio")
        add_packages("fmt", "liburing")
end

--xmake project -k compile_commands