set_project("magio")
set_version("0.0.3")

add_rules("mode.debug", "mode.release")
add_cxxflags("/EHa")
set_languages("cxx20")
set_warnings("all")

if is_mode("release") then 
    set_optimize("faster")
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
end

--examples
for _, dir in ipairs(os.files("examples/**.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio")
end