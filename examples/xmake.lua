for _, dir in ipairs(os.files("v3/*.cpp")) do
    target(path.basename(dir))
        set_kind("binary")
        add_files(dir)
        add_deps("magio-v3")
        add_packages("fmt", {links = {}})
end