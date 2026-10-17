#ifndef DXX_METADATA_JSON_HPP
#define DXX_METADATA_JSON_HPP
#include <string>

template <class Json>
std::string metadata_json_inline(const Json &value)
{
	if (!value.is_object() && !value.is_array()) return value.dump(-1, ' ', false, Json::error_handler_t::replace);
	std::string out = value.is_object() ? "{" : "[";
	bool first = true;
	for (auto it = value.begin(); it != value.end(); ++it) {
		if (!first) out += ", ";
		first = false;
		if (value.is_object()) out += metadata_json_inline(Json(it.key())) + ": ";
		out += metadata_json_inline(it.value());
	}
	return out + (value.is_object() ? "}" : "]");
}

// Match normal two-space JSON formatting, except matcen records are single rows
// Keep this layout in sync with MissionMetadataProjection.formatMatcenRows
// JSON values still use the library encoder for escaping and invalid UTF-8
template <class Json>
std::string metadata_json_dump(const Json &value, unsigned depth = 0, bool matcen = false, bool rows = false)
{
	const auto dump = [](const Json &item) { return item.dump(-1, ' ', false, Json::error_handler_t::replace); };
	if ((!value.is_object() && !value.is_array()) || value.empty())
		return dump(value);
	if (rows) return metadata_json_inline(value);
	const std::string indent(depth * 2, ' '), child_indent((depth + 1) * 2, ' ');
	std::string out = value.is_object() ? "{\n" : "[\n";
	bool first = true;
	for (auto it = value.begin(); it != value.end(); ++it) {
		if (!first) out += ",\n";
		first = false;
		out += child_indent;
		if (value.is_object()) out += dump(Json(it.key())) + ": ";
		const bool block = value.is_object() && it.key() == "matcen_regression";
		// In the block, arrays retain one row per element; scalar/count objects
		// stay inline so diagnostics remain readable without repeated scaffolding
		out += metadata_json_dump(it.value(), depth + 1, block || (matcen && it.value().is_array()), matcen && (value.is_array() || !it.value().is_array()));
	}
	return out + "\n" + indent + (value.is_object() ? "}" : "]");
}
#endif
