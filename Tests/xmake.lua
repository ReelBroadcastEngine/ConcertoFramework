if is_mode("coverage") then
	if not is_plat("windows") then
		add_links("gcov")
	end
end

if has_config("tests") then
    add_requires("catch2")
	includes("Core/xmake.lua")
	includes("Reflection/xmake.lua")
	includes("Graphics/xmake.lua")

	target("concerto-tests", function()
		set_kind("binary")
		add_files("./main.cpp")
		add_packages("catch2", {public = true})
		add_deps("concerto-core-tests", {public = true})
		if has_config("reflection") then
			add_deps("concerto-reflection-tests", {public = true})
		end
		if has_config("graphics") then
			add_deps("concerto-graphics-tests", {public = true})
			add_rpathdirs("$ORIGIN")
		end
	end)
end