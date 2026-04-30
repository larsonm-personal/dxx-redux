#include "input_demo_codec.h"

#include <exception>

#include "base64.h"
#include "picosha2.h"

namespace
{

static bool fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

} // namespace

bool input_demo_base64_encode(const unsigned char *data,
                              size_t data_size,
                              std::string *encoded,
                              std::string *error)
{
	if (!encoded)
		return fail(error, "missing base64 encode output");
	if (data_size && !data)
		return fail(error, "missing base64 encode input");
	if (!data_size) {
		encoded->clear();
		return true;
	}
	*encoded = base64_encode(data, data_size, false);
	return true;
}

bool input_demo_base64_decode(const std::string &encoded,
                              std::vector<uint8_t> *decoded,
                              std::string *error)
{
	if (!decoded)
		return fail(error, "missing base64 decode output");
	try {
		const std::string decoded_text = base64_decode(encoded, false);
		decoded->assign(decoded_text.begin(), decoded_text.end());
	} catch (const std::exception &exception) {
		return fail(error, std::string("base64 decode failed: ") + exception.what());
	}
	return true;
}

bool input_demo_sha256_hex(const unsigned char *data,
                           size_t data_size,
                           std::string *hex,
                           std::string *error)
{
	if (!hex)
		return fail(error, "missing sha256 output");
	if (data_size && !data)
		return fail(error, "missing sha256 input");
	if (!data_size) {
		*hex = picosha2::hash256_hex_string(std::string());
		return true;
	}
	*hex = picosha2::hash256_hex_string(data, data + data_size);
	return true;
}