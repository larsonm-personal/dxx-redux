// Compare installed source-owned records, not a narrowed intermediate stock table
void require_owned_light_records(const xl_compat::Inventory &source)
{
	require(Num_static_lights == 0, "XL runtime retained a stock light table that could apply twice");
	require(xl_light_index_count() == static_cast<int>(source.light_indices.size()) &&
	            xl_light_delta_count() == static_cast<int>(source.delta_lights.size()),
	        "Owned XL light table counts differ");
	for (int i = 0; i < xl_light_index_count(); ++i) {
		xl_light_index actual{};
		const auto &expected = source.light_indices[i];
		require(xl_light_index_get(i, &actual) && actual.segment == expected[0] && actual.side == expected[1] &&
		            actual.count == expected[2] && actual.first == expected[3],
		        "Owned XL light index narrowed or changed");
	}
	for (int i = 0; i < xl_light_delta_count(); ++i) {
		xl_light_delta actual{};
		const auto &expected = source.delta_lights[i];
		require(xl_light_delta_get(i, &actual) && actual.segment == expected[0] && actual.side == expected[1], "Owned XL delta target differs");
		for (int corner = 0; corner < 4; ++corner)
			require(actual.light[corner] == expected[corner + 3], "Owned XL delta contribution differs");
	}
}
