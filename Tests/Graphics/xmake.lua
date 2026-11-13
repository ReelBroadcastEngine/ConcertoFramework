if has_config("tests") and has_config("graphics") then
    target("concerto-graphics-tests", function()
        set_kind("object")
        set_languages("cxx20")
        set_warnings("allextra")
        if is_mode("debug") then
            set_symbols("debug")
        end
        add_files("**.cpp")
        add_includedirs(".", { public = false })
        add_packages("catch2")
        add_deps("concerto-rhi-module", { public = true })
        add_cxxflags("cl::/wd4251")
    end)
end
