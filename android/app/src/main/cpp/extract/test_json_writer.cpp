#include "json_writer.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

static std::string render_json_string(const char *value)
{
	FILE *stream = tmpfile();
	if (!stream) throw std::runtime_error("tmpfile failed");
	if (json_write_string(stream, value) < 0 || fflush(stream) != 0 ||
	    fseek(stream, 0, SEEK_END) != 0) {
		fclose(stream);
		throw std::runtime_error("JSON write failed");
	}
	long length = ftell(stream);
	if (length < 0 || fseek(stream, 0, SEEK_SET) != 0) {
		fclose(stream);
		throw std::runtime_error("temporary stream seek failed");
	}
	std::string output(static_cast<size_t>(length), '\0');
	if (length > 0 && fread(&output[0], 1, output.size(), stream) != output.size()) {
		fclose(stream);
		throw std::runtime_error("temporary stream read failed");
	}
	fclose(stream);
	return output;
}

static void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}

int main()
{
	try {
		std::string input = "quote\" backslash\\ controls\b\f\n\r\t";
		for (unsigned char control = 1; control < 0x20; control++)
			input.push_back(static_cast<char>(control));
		input += " non-ASCII \xc3\xa9 malformed ";
		input.push_back(static_cast<char>(0xc3));
		input.push_back('(');
		input.push_back(static_cast<char>(0xff));

		std::string encoded = render_json_string(input.c_str());
		nlohmann::json parsed = nlohmann::json::parse(encoded);
		std::string expected = "quote\" backslash\\ controls\b\f\n\r\t";
		for (unsigned char control = 1; control < 0x20; control++)
			expected.push_back(static_cast<char>(control));
		expected += " non-ASCII \xc3\xa9 malformed \xc3\x83(\xc3\xbf";

		require(parsed.is_string(), "encoded value is not a JSON string");
		require(parsed.get<std::string>() == expected, "strict JSON round trip changed text");
		require(encoded.find("\\\"") != std::string::npos, "quote was not escaped");
		require(encoded.find("\\\\") != std::string::npos, "backslash was not escaped");
		require(encoded.find("\\n") != std::string::npos, "newline was not escaped");
		require(encoded.find("\\t") != std::string::npos, "tab was not escaped");
		require(encoded.find("\\u0001") != std::string::npos, "control byte was not escaped");
		require(encoded.find("\\u00c3") != std::string::npos,
		        "malformed UTF-8 lead byte was not encoded");
		require(encoded.find("\\u00ff") != std::string::npos,
		        "malformed UTF-8 byte was not encoded");

		nlohmann::json null_value = nlohmann::json::parse(render_json_string(NULL));
		require(null_value.is_null(), "null input was not encoded as JSON null");
	} catch (const std::exception &error) {
		fprintf(stderr, "test_json_writer: %s\n", error.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
