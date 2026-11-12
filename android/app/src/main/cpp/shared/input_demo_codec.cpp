#include "input_demo_codec.h"

#include <algorithm>
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
	try {
		// The pointer convenience overload buffers the entire checkpoint
		picosha2::hash256_one_by_one hash;
		for (size_t offset = 0; offset < data_size;) {
			const size_t count = std::min<size_t>(65536, data_size - offset);
			hash.process(data + offset, data + offset + count);
			offset += count;
		}
		hash.finish();
		*hex = picosha2::get_hash_hex_string(hash);
	} catch (const std::exception &exception) {
		return fail(error, std::string("sha256 failed: ") + exception.what());
	}
	return true;
}
