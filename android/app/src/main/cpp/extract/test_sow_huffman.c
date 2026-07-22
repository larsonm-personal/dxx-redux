#include <stdio.h>
#include <string.h>

int sow_test_huff_make_table(const unsigned char *bitlen, int nchar, int tablebits);
int sow_test_read_pt_len(const unsigned char *data, unsigned int size, int nn);
int sow_test_decode_block(const unsigned char *data, unsigned int size);

static int expect(int condition, const char *message)
{
	if (condition)
		return 0;
	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

int main(void)
{
	unsigned char lengths[512];
	int failures = 0;

	memset(lengths, 0, sizeof(lengths));
	failures += expect(sow_test_huff_make_table(lengths, 1, 12) == 0,
	                   "zero-bit degenerate table");

	for (int i = 0; i < 15; i++)
		lengths[i] = (unsigned char) (i + 1);
	lengths[15] = 16;
	lengths[16] = 16;
	failures += expect(sow_test_huff_make_table(lengths, 17, 12) == 0,
	                   "complete table containing 16-bit codes");

	lengths[16] = 17;
	failures += expect(sow_test_huff_make_table(lengths, 17, 12) < 0,
	                   "17-bit code length");
	lengths[16] = 20;
	failures += expect(sow_test_huff_make_table(lengths, 17, 12) < 0,
	                   "maximum unary code length");
	for (int length = 17; length <= 255; length++) {
		lengths[16] = (unsigned char) length;
		if (sow_test_huff_make_table(lengths, 17, 12) >= 0) {
			failures += expect(0, "invalid code-length range");
			break;
		}
	}
	failures += expect(sow_test_huff_make_table(lengths, 512, 12) < 0,
	                   "oversized symbol count");

	{
		const unsigned char oversized_count[] = { 0xA0, 0x00 };
		const unsigned char invalid_selector[] = { 0x04, 0xC0 };
		const unsigned char unary_length[] = { 0x0F, 0xFF, 0xFF };
		const unsigned char malformed_block[] = { 0x00, 0x01, 0x0F, 0xFF, 0xFF };

		failures += expect(sow_test_read_pt_len(oversized_count,
		                                        sizeof(oversized_count), 19) < 0,
		                   "oversized decoded symbol count");
		failures += expect(sow_test_read_pt_len(invalid_selector,
		                                        sizeof(invalid_selector), 19) < 0,
		                   "out-of-range degenerate symbol");
		failures += expect(sow_test_read_pt_len(unary_length,
		                                        sizeof(unary_length), 19) < 0,
		                   "all-ones unary extension");
		failures += expect(sow_test_decode_block(malformed_block,
		                                         sizeof(malformed_block)) < 0,
		                   "malformed table failure propagation");
	}

	if (failures != 0)
		return 1;
	printf("SOW Huffman validation tests passed\n");
	return 0;
}
