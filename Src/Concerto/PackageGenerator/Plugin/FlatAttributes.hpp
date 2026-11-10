//
// Created by arthur
//

#ifndef CONCERTO_PKGGENERATOR_FLATATTRIBUTES_HPP
#define CONCERTO_PKGGENERATOR_FLATATTRIBUTES_HPP

#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Concerto/PackageGenerator/Defines.hpp"

namespace cct
{
	using FlatAttributes = std::vector<std::pair<std::string, std::string>>;

	namespace Detail
	{
		inline void FlattenAttributes(const TomlAttributes& value, const std::string& prefix, FlatAttributes& out)
		{
			if (value.is_table())
			{
				for (const auto& [key, child] : value.as_table())
					FlattenAttributes(child, prefix.empty() ? key : prefix + "." + key, out);
				return;
			}
			if (prefix.empty())
				return;
			if (value.is_string())
				out.emplace_back(prefix, value.as_string());
			else if (value.is_floating())
			{
				// Shortest round-trip form; "%g" would keep only 6 significant digits.
				std::array<char, 64> buffer = {};
				const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value.as_floating());
				if (error == std::errc())
					out.emplace_back(prefix, std::string(buffer.data(), end));
			}
			else if (value.is_integer())
				out.emplace_back(prefix, std::to_string(value.as_integer()));
			else if (value.is_boolean())
				out.emplace_back(prefix, value.as_boolean() ? "true" : "false");
		}
	} // namespace Detail

	/// Flattens a TOML table into key/value pairs sorted by key, nested tables as dotted keys.
	[[nodiscard]] inline FlatAttributes FlattenAttributes(const TomlAttributes& attributes)
	{
		FlatAttributes out;
		if (attributes.is_table())
			Detail::FlattenAttributes(attributes, "", out);
		std::ranges::sort(out, {}, &FlatAttributes::value_type::first);
		return out;
	}
} // namespace cct

#endif // CONCERTO_PKGGENERATOR_FLATATTRIBUTES_HPP
