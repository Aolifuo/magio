package("liburing")

    set_homepage("https://github.com/axboe/liburing")
    set_description("liburing provides helpers to setup and teardown io_uring instances")

    add_urls("https://github.com/axboe/liburing/archive/refs/tags/liburing-$(version).tar.gz",
             "https://github.com/axboe/liburing.git")
    add_versions("2.3", "60b367dbdc6f2b0418a6e0cd203ee0049d9d629a36706fcf91dfb9428bae23c8")

    -- liburing doesn't support building as a shared lib
    add_configs("shared", {description = "Build shared library.", default = false, type = "boolean", readonly = true})

    on_install("linux", function (package)
        local cflags
        if package:config("pic") ~= false then
            cflags = "-fPIC"
        end
        import("package.tools.autoconf").install(package, {}, {makeconfigs = {CFLAGS = cflags}})
    end)

    on_test(function (package)
        assert(package:has_cfuncs("io_uring_submit", {includes = "liburing.h"}))
    end)