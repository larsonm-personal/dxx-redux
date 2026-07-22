/*
 * sti2_extract.c - STi2 installer archive listing and extraction.
 *
 * The method 13, method 14, and method 15 decompressors in this file are
 * derived primarily from XADMaster's StuffIt support, especially
 * XADStuffIt13Handle, XADStuffIt14Handle, XADStuffItArsenicHandle, BWT, and
 * XADPrefixCode, which are distributed under LGPL-2.1.
 * https://github.com/ashang/unar
 */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#define close_fd(fd)               _close(fd)
#define mkdir_one(path, mode)      _mkdir(path)
#define open_fd(path, flags, mode) _open(path, flags, mode)
#define write_fd(fd, buf, n)       _write(fd, buf, (unsigned int) (n))
#define O_BINARY                   _O_BINARY
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define close_fd(fd)               close(fd)
#define mkdir_one(path, mode)      mkdir((path), (mode))
#define open_fd(path, flags, mode) open((path), (flags), (mode))
#define write_fd(fd, buf, n)       write(fd, buf, (n))
#define O_BINARY                   0
#endif

#include "sti2_extract.h"

#include "extract_limits.h"

#define STI2_ARCHIVE_HEADER_SIZE 22u
#define STI2_FILE_HEADER_SIZE    112u
#define STI2_HEADER_SIGNATURE    0x724c6175u

#define STI2_ENCRYPTED_FLAG            0x80u
#define STI2_START_FOLDER              0x20u
#define STI2_END_FOLDER                0x21u
#define STI2_FOLDER_CONTAINS_ENCRYPTED 0x10u
#define STI2_FOLDER_MASK               ((unsigned int) (~(STI2_ENCRYPTED_FLAG | STI2_FOLDER_CONTAINS_ENCRYPTED)) & 0xffu)
#define STI2_COMPRESSION_MASK          0x0fu

#define SITFH_COMPRMETHOD 0u
#define SITFH_COMPDMETHOD 1u
#define SITFH_FNAMESIZE   2u
#define SITFH_FNAME       3u
#define SITFH_PARENTOFFS  58u
#define SITFH_FTYPE       66u
#define SITFH_CREATOR     70u
#define SITFH_FNDRFLAGS   74u
#define SITFH_RSRCLENGTH  84u
#define SITFH_DATALENGTH  88u
#define SITFH_COMPRLENGTH 92u
#define SITFH_COMPDLENGTH 96u
#define SITFH_HDRCRC      110u

#define STI2_MAX_DEPTH        32
#define STI2_METHOD13_SYMBOLS 321
#define STI2_METHOD13_WINDOW  65536u
#define STI2_CODE_TABLE_BITS  10
#define STI2_CODE_TABLE_SIZE  (1 << STI2_CODE_TABLE_BITS)
#define STI2_CODE_MAX_NODES   2048
#define STI2_OPEN_BRANCH      (-1)
#define STI2_LEAF_BASE        (-3)

typedef struct {
	unsigned int offset;
	unsigned int parent_offset;
	unsigned int resource_uncompressed_size;
	unsigned int data_uncompressed_size;
	unsigned int resource_compressed_size;
	unsigned int data_compressed_size;
	unsigned int resource_method;
	unsigned int data_method;
	unsigned int file_type;
	unsigned int creator;
	unsigned int finder_flags;
	char name[STI2_NAME_LEN];
	int is_directory;
} sti2_header_t;

typedef struct {
	int left;
	int right;
} sti2_code_node_t;

typedef struct {
	int length;
	int value;
} sti2_code_table_entry_t;

typedef struct {
	sti2_code_node_t nodes[STI2_CODE_MAX_NODES];
	sti2_code_table_entry_t table[STI2_CODE_TABLE_SIZE];
	int node_count;
	int min_length;
	int max_length;
	int table_bits;
} sti2_code_t;

typedef struct {
	const unsigned char *data;
	size_t size;
	size_t byte_pos;
	unsigned long long bitbuf;
	int bit_count;
} sti2_bit_reader_t;

typedef struct {
	sti2_bit_reader_t br;
	sti2_code_t first_code;
	sti2_code_t second_code;
	sti2_code_t offset_code;
	sti2_code_t *current_code;
} sti2_method13_decoder_t;

typedef struct {
	const unsigned char *data;
	size_t size;
	size_t byte_pos;
	int bit_pos;
} sti2_msb_bit_reader_t;

typedef struct {
	int symbol;
	int frequency;
} sti2_arithmetic_symbol_t;

typedef struct {
	int total_frequency;
	int increment;
	int frequency_limit;
	int num_symbols;
	sti2_arithmetic_symbol_t symbols[128];
} sti2_arithmetic_model_t;

typedef struct {
	sti2_msb_bit_reader_t *input;
	int range;
	int code;
} sti2_arithmetic_decoder_t;

typedef struct {
	int table[256];
} sti2_mtf_state_t;

typedef struct {
	sti2_arithmetic_model_t initial_model;
	sti2_arithmetic_model_t selector_model;
	sti2_arithmetic_model_t mtf_model[7];
	sti2_arithmetic_decoder_t decoder;
	sti2_mtf_state_t mtf;
	int block_bits;
	int block_size;
	unsigned char *block;
	uint32_t *transform;
	int end_of_blocks;
	int num_bytes;
	int byte_count;
	int transform_index;
	int randomized;
	int rand_count;
	int rand_index;
	int repeat;
	int count;
	int last;
} sti2_method15_decoder_t;

// clang-format off
static const unsigned char first_code_lengths_1[STI2_METHOD13_SYMBOLS] ={ 4, 5, 7, 8, 8, 9, 9, 9, 9, 7, 9, 9, 9, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 9, 9, 10, 10, 9, 10, 9, 9, 5, 9, 9, 9, 9, 10, 9, 9, 9, 9, 9, 9, 9, 9, 7, 9, 9, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8, 9, 9, 8, 8, 9, 9, 9, 9, 9, 9, 9, 7, 8, 9, 7, 9, 9, 7, 7, 9, 9, 9, 9, 10, 9, 10, 10, 10, 9, 9, 9, 5, 9, 8, 7, 5, 9, 8, 8, 7, 9, 9, 8, 8, 5, 5, 7, 10, 5, 8, 5, 8, 9, 9, 9, 9, 9, 10, 9, 9, 10, 9, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 9, 10, 9, 5, 6, 5, 5, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 9, 9, 9, 10, 9, 10, 9, 10, 9, 10, 9, 10, 10, 10, 9, 10, 9, 10, 10, 9, 9, 9, 6, 9, 9, 10, 9, 5,  };

static const unsigned char second_code_lengths_1[STI2_METHOD13_SYMBOLS] ={ 4, 5, 6, 6, 7, 7, 6, 7, 7, 7, 6, 8, 7, 8, 8, 8, 8, 9, 6, 9, 8, 9, 8, 9, 9, 9, 8, 10, 5, 9, 7, 9, 6, 9, 8, 10, 9, 10, 8, 8, 9, 9, 7, 9, 8, 9, 8, 9, 8, 8, 6, 9, 9, 8, 8, 9, 9, 10, 8, 9, 9, 10, 8, 10, 8, 8, 8, 8, 8, 9, 7, 10, 6, 9, 9, 11, 7, 8, 8, 9, 8, 10, 7, 8, 6, 9, 10, 9, 9, 10, 8, 11, 9, 11, 9, 10, 9, 8, 9, 8, 8, 8, 8, 10, 9, 9, 10, 10, 8, 9, 8, 8, 8, 11, 9, 8, 8, 9, 9, 10, 8, 11, 10, 10, 8, 10, 9, 10, 8, 9, 9, 11, 9, 11, 9, 10, 10, 11, 10, 12, 9, 12, 10, 11, 10, 11, 9, 10, 10, 11, 10, 11, 10, 11, 10, 11, 10, 10, 10, 9, 9, 9, 8, 7, 6, 8, 11, 11, 9, 12, 10, 12, 9, 11, 11, 11, 10, 12, 11, 11, 10, 12, 10, 11, 10, 10, 10, 11, 10, 11, 11, 11, 9, 12, 10, 12, 11, 12, 10, 11, 10, 12, 11, 12, 11, 12, 11, 12, 10, 12, 11, 12, 11, 11, 10, 12, 10, 11, 10, 12, 10, 12, 10, 12, 10, 11, 11, 11, 10, 11, 11, 11, 10, 12, 11, 12, 10, 10, 11, 11, 9, 12, 11, 12, 10, 11, 10, 12, 10, 11, 10, 12, 10, 11, 10, 7, 5, 4, 6, 6, 7, 7, 7, 8, 8, 7, 7, 6, 8, 6, 7, 7, 9, 8, 9, 9, 10, 11, 11, 11, 12, 11, 10, 11, 12, 11, 12, 11, 12, 12, 12, 12, 11, 12, 12, 11, 12, 11, 12, 11, 13, 11, 12, 10, 13, 10, 14, 14, 13, 14, 15, 14, 16, 15, 15, 18, 18, 18, 9, 18, 8,  };

static const unsigned char offset_code_lengths_1[11] ={ 5, 6, 3, 3, 3, 3, 3, 3, 3, 4, 6,  };

static const unsigned char first_code_lengths_2[STI2_METHOD13_SYMBOLS] ={ 4, 7, 7, 8, 7, 8, 8, 8, 8, 7, 8, 7, 8, 7, 9, 8, 8, 8, 9, 9, 9, 9, 10, 10, 9, 10, 10, 10, 10, 10, 9, 9, 5, 9, 8, 9, 9, 11, 10, 9, 8, 9, 9, 9, 8, 9, 7, 8, 8, 8, 9, 9, 9, 9, 9, 10, 9, 9, 9, 10, 9, 9, 10, 9, 8, 8, 7, 7, 7, 8, 8, 9, 8, 8, 9, 9, 8, 8, 7, 8, 7, 10, 8, 7, 7, 9, 9, 9, 9, 10, 10, 11, 11, 11, 10, 9, 8, 6, 8, 7, 7, 5, 7, 7, 7, 6, 9, 8, 6, 7, 6, 6, 7, 9, 6, 6, 6, 7, 8, 8, 8, 8, 9, 10, 9, 10, 9, 9, 8, 9, 10, 10, 9, 10, 10, 9, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 11, 10, 10, 10, 10, 10, 10, 10, 11, 10, 11, 10, 10, 9, 11, 10, 10, 10, 10, 10, 10, 9, 9, 10, 11, 10, 11, 10, 11, 10, 12, 10, 11, 10, 12, 11, 12, 10, 12, 10, 11, 10, 11, 11, 11, 9, 10, 11, 11, 11, 12, 12, 10, 10, 10, 11, 11, 10, 11, 10, 10, 9, 11, 10, 11, 10, 11, 11, 11, 10, 11, 11, 12, 11, 11, 10, 10, 10, 11, 10, 10, 11, 11, 12, 10, 10, 11, 11, 12, 11, 11, 10, 11, 9, 12, 10, 11, 11, 11, 10, 11, 10, 11, 10, 11, 9, 10, 9, 7, 3, 5, 6, 6, 7, 7, 8, 8, 8, 9, 9, 9, 11, 10, 10, 10, 12, 13, 11, 12, 12, 11, 13, 12, 12, 11, 12, 12, 13, 12, 14, 13, 14, 13, 15, 13, 14, 15, 15, 14, 13, 15, 15, 14, 15, 14, 15, 15, 14, 15, 13, 13, 14, 15, 15, 14, 14, 16, 16, 15, 15, 15, 12, 15, 10,  };

static const unsigned char second_code_lengths_2[STI2_METHOD13_SYMBOLS] ={ 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 7, 8, 7, 7, 7, 8, 8, 8, 8, 9, 8, 9, 8, 9, 9, 9, 7, 9, 8, 8, 6, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 8, 8, 8, 8, 9, 8, 9, 8, 9, 9, 10, 8, 10, 8, 9, 9, 8, 8, 8, 7, 8, 8, 9, 8, 9, 7, 9, 8, 10, 8, 9, 8, 9, 8, 9, 8, 8, 8, 9, 9, 9, 9, 10, 9, 11, 9, 10, 9, 10, 8, 8, 8, 9, 8, 8, 8, 9, 9, 8, 9, 10, 8, 9, 8, 8, 8, 11, 8, 7, 8, 9, 9, 9, 9, 10, 9, 10, 9, 10, 9, 8, 8, 9, 9, 10, 9, 10, 9, 10, 8, 10, 9, 10, 9, 11, 10, 11, 9, 11, 10, 10, 10, 11, 9, 11, 9, 10, 9, 11, 9, 11, 10, 10, 9, 10, 9, 9, 8, 10, 9, 11, 9, 9, 9, 11, 10, 11, 9, 11, 9, 11, 9, 11, 10, 11, 10, 11, 10, 11, 9, 10, 10, 11, 10, 10, 8, 10, 9, 10, 10, 11, 9, 11, 9, 10, 10, 11, 9, 10, 10, 9, 9, 10, 9, 10, 9, 10, 9, 10, 9, 11, 9, 11, 10, 10, 9, 10, 9, 11, 9, 11, 9, 11, 9, 10, 9, 11, 9, 11, 9, 11, 9, 10, 8, 11, 9, 10, 9, 10, 9, 10, 8, 10, 8, 9, 8, 9, 8, 7, 4, 4, 5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 7, 8, 8, 9, 9, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 12, 11, 11, 12, 12, 11, 12, 12, 11, 12, 12, 12, 12, 12, 12, 11, 12, 11, 13, 12, 13, 12, 13, 14, 14, 14, 15, 13, 14, 13, 14, 18, 18, 17, 7, 16, 9,  };

static const unsigned char offset_code_lengths_2[13] ={ 5, 6, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 6,  };

static const unsigned char first_code_lengths_3[STI2_METHOD13_SYMBOLS] ={ 6, 6, 6, 6, 6, 9, 8, 8, 4, 9, 8, 9, 8, 9, 9, 9, 8, 9, 9, 10, 8, 10, 10, 10, 9, 10, 10, 10, 9, 10, 10, 9, 9, 9, 8, 10, 9, 10, 9, 10, 9, 10, 9, 10, 9, 9, 8, 9, 8, 9, 9, 9, 10, 10, 10, 10, 9, 9, 9, 10, 9, 10, 9, 9, 7, 8, 8, 9, 8, 9, 9, 9, 8, 9, 9, 10, 9, 9, 8, 9, 8, 9, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 9, 8, 8, 9, 8, 9, 7, 8, 8, 9, 8, 10, 10, 8, 9, 8, 8, 8, 10, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 9, 7, 9, 9, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 9, 8, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 9, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 9, 9, 9, 10, 10, 10, 10, 10, 10, 9, 9, 10, 9, 9, 8, 9, 8, 9, 4, 6, 6, 6, 7, 8, 8, 9, 9, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 7, 10, 10, 10, 7, 10, 10, 7, 7, 7, 7, 7, 6, 7, 10, 7, 7, 10, 7, 7, 7, 6, 7, 6, 6, 7, 7, 6, 6, 9, 6, 9, 10, 6, 10,  };

static const unsigned char second_code_lengths_3[STI2_METHOD13_SYMBOLS] ={ 5, 6, 6, 6, 6, 7, 7, 7, 6, 8, 7, 8, 7, 9, 8, 8, 7, 7, 8, 9, 9, 9, 9, 10, 8, 9, 9, 10, 8, 10, 9, 8, 6, 10, 8, 10, 8, 10, 9, 9, 9, 9, 9, 10, 9, 9, 8, 9, 8, 9, 8, 9, 9, 10, 9, 10, 9, 9, 8, 10, 9, 11, 10, 8, 8, 8, 8, 9, 7, 9, 9, 10, 8, 9, 8, 11, 9, 10, 9, 10, 8, 9, 9, 9, 9, 8, 9, 9, 10, 10, 10, 12, 10, 11, 10, 10, 8, 9, 9, 9, 8, 9, 8, 8, 10, 9, 10, 11, 8, 10, 9, 9, 8, 12, 8, 9, 9, 9, 9, 8, 9, 10, 9, 12, 10, 10, 10, 8, 7, 11, 10, 9, 10, 11, 9, 11, 7, 11, 10, 12, 10, 12, 10, 11, 9, 11, 9, 12, 10, 12, 10, 12, 10, 9, 11, 12, 10, 12, 10, 11, 9, 10, 9, 10, 9, 11, 11, 12, 9, 10, 8, 12, 11, 12, 9, 12, 10, 12, 10, 13, 10, 12, 10, 12, 10, 12, 10, 9, 10, 12, 10, 9, 8, 11, 10, 12, 10, 12, 10, 12, 10, 11, 10, 12, 8, 12, 10, 11, 10, 10, 10, 12, 9, 11, 10, 12, 10, 12, 11, 12, 10, 9, 10, 12, 9, 10, 10, 12, 10, 11, 10, 11, 10, 12, 8, 12, 9, 12, 8, 12, 8, 11, 10, 11, 10, 11, 9, 10, 8, 10, 9, 9, 8, 9, 8, 7, 4, 3, 5, 5, 6, 5, 6, 6, 7, 7, 8, 8, 8, 7, 7, 7, 9, 8, 9, 9, 11, 9, 11, 9, 8, 9, 9, 11, 12, 11, 12, 12, 13, 13, 12, 13, 14, 13, 14, 13, 14, 13, 13, 13, 12, 13, 13, 12, 13, 13, 14, 14, 13, 13, 14, 14, 14, 14, 15, 18, 17, 18, 8, 16, 10,  };

static const unsigned char offset_code_lengths_3[14] ={ 6, 7, 4, 4, 3, 3, 3, 3, 3, 4, 4, 4, 5, 7,  };

static const unsigned char first_code_lengths_4[STI2_METHOD13_SYMBOLS] ={ 2, 6, 6, 7, 7, 8, 7, 8, 7, 8, 8, 9, 8, 9, 9, 9, 8, 8, 9, 9, 9, 10, 10, 9, 8, 10, 9, 10, 9, 10, 9, 9, 6, 9, 8, 9, 9, 10, 9, 9, 9, 10, 9, 9, 9, 9, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 9, 7, 7, 8, 8, 8, 8, 9, 9, 7, 8, 9, 10, 8, 8, 7, 8, 8, 10, 8, 8, 8, 9, 8, 9, 9, 10, 9, 11, 10, 11, 9, 9, 8, 7, 9, 8, 8, 6, 8, 8, 8, 7, 10, 9, 7, 8, 7, 7, 8, 10, 7, 7, 7, 8, 9, 9, 9, 9, 10, 11, 9, 11, 10, 9, 7, 9, 10, 10, 10, 11, 11, 10, 10, 11, 10, 10, 10, 11, 11, 10, 9, 10, 10, 11, 10, 11, 10, 11, 10, 10, 10, 11, 10, 11, 10, 10, 9, 10, 10, 11, 10, 10, 10, 10, 9, 10, 10, 10, 10, 11, 10, 11, 10, 11, 10, 11, 11, 11, 10, 12, 10, 11, 10, 11, 10, 11, 11, 10, 8, 10, 10, 11, 10, 11, 11, 11, 10, 11, 10, 11, 10, 11, 11, 11, 9, 10, 11, 11, 10, 11, 11, 11, 10, 11, 11, 11, 10, 10, 10, 10, 10, 11, 10, 10, 11, 11, 10, 10, 9, 11, 10, 10, 11, 11, 10, 10, 10, 11, 10, 10, 10, 10, 10, 10, 9, 11, 10, 10, 8, 10, 8, 6, 5, 6, 6, 7, 7, 8, 8, 8, 9, 10, 11, 10, 10, 11, 11, 12, 12, 10, 11, 12, 12, 12, 12, 13, 13, 13, 13, 13, 12, 13, 13, 15, 14, 12, 14, 15, 16, 12, 12, 13, 15, 14, 16, 15, 17, 18, 15, 17, 16, 15, 15, 15, 15, 13, 13, 10, 14, 12, 13, 17, 17, 18, 10, 17, 4,  };

static const unsigned char second_code_lengths_4[STI2_METHOD13_SYMBOLS] ={ 4, 5, 6, 6, 6, 6, 7, 7, 6, 7, 7, 9, 6, 8, 8, 7, 7, 8, 8, 8, 6, 9, 8, 8, 7, 9, 8, 9, 8, 9, 8, 9, 6, 9, 8, 9, 8, 10, 9, 9, 8, 10, 8, 10, 8, 9, 8, 9, 8, 8, 7, 9, 9, 9, 9, 9, 8, 10, 9, 10, 9, 10, 9, 8, 7, 8, 9, 9, 8, 9, 9, 9, 7, 10, 9, 10, 9, 9, 8, 9, 8, 9, 8, 8, 8, 9, 9, 10, 9, 9, 8, 11, 9, 11, 10, 10, 8, 8, 10, 8, 8, 9, 9, 9, 10, 9, 10, 11, 9, 9, 9, 9, 8, 9, 8, 8, 8, 10, 10, 9, 9, 8, 10, 11, 10, 11, 11, 9, 8, 9, 10, 11, 9, 10, 11, 11, 9, 12, 10, 10, 10, 12, 11, 11, 9, 11, 11, 12, 9, 11, 9, 10, 10, 10, 10, 12, 9, 11, 10, 11, 9, 11, 11, 11, 10, 11, 11, 12, 9, 10, 10, 12, 11, 11, 10, 11, 9, 11, 10, 11, 10, 11, 9, 11, 11, 9, 8, 11, 10, 11, 11, 10, 7, 12, 11, 11, 11, 11, 11, 12, 10, 12, 11, 13, 11, 10, 12, 11, 10, 11, 10, 11, 10, 11, 11, 11, 10, 12, 11, 11, 10, 11, 10, 10, 10, 11, 10, 12, 11, 12, 10, 11, 9, 11, 10, 11, 10, 11, 10, 12, 9, 11, 11, 11, 9, 11, 10, 10, 9, 11, 10, 10, 9, 10, 9, 7, 4, 5, 5, 5, 6, 6, 7, 6, 8, 7, 8, 9, 9, 7, 8, 8, 10, 9, 10, 10, 12, 10, 11, 11, 11, 11, 10, 11, 12, 11, 11, 11, 11, 11, 13, 12, 11, 12, 13, 12, 12, 12, 13, 11, 9, 12, 13, 7, 13, 11, 13, 11, 10, 11, 13, 15, 15, 12, 14, 15, 15, 15, 6, 15, 5,  };

static const unsigned char offset_code_lengths_4[11] ={ 3, 6, 5, 4, 2, 3, 3, 3, 4, 4, 6,  };

static const unsigned char first_code_lengths_5[STI2_METHOD13_SYMBOLS] ={ 7, 9, 9, 9, 9, 9, 9, 9, 9, 8, 9, 9, 9, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 9, 10, 9, 10, 9, 10, 9, 9, 5, 9, 7, 9, 9, 9, 9, 9, 7, 7, 7, 9, 7, 7, 8, 7, 8, 8, 7, 7, 9, 9, 9, 9, 7, 7, 7, 9, 9, 9, 9, 9, 9, 7, 9, 7, 7, 7, 7, 9, 9, 7, 9, 9, 7, 7, 7, 7, 7, 9, 7, 8, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 7, 8, 7, 7, 7, 8, 8, 6, 7, 9, 7, 7, 8, 7, 5, 6, 9, 5, 7, 5, 6, 7, 7, 9, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 9, 10, 10, 10, 9, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 9, 10, 10, 10, 9, 9, 10, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 9, 10, 10, 10, 9, 9, 9, 10, 10, 10, 10, 10, 9, 10, 9, 10, 10, 9, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 10, 10, 10, 10, 10, 9, 10, 9, 10, 9, 10, 10, 9, 5, 6, 8, 8, 7, 7, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 9, 10, 10, 5, 10, 8, 9, 8, 9,  };

static const unsigned char second_code_lengths_5[STI2_METHOD13_SYMBOLS] ={ 8, 10, 11, 11, 11, 12, 11, 11, 12, 6, 11, 12, 10, 5, 12, 12, 12, 12, 12, 12, 12, 13, 13, 14, 13, 13, 12, 13, 12, 13, 12, 15, 4, 10, 7, 9, 11, 11, 10, 9, 6, 7, 8, 9, 6, 7, 6, 7, 8, 7, 7, 8, 8, 8, 8, 8, 8, 9, 8, 7, 10, 9, 10, 10, 11, 7, 8, 6, 7, 8, 8, 9, 8, 7, 10, 10, 8, 7, 8, 8, 7, 10, 7, 6, 7, 9, 9, 8, 11, 11, 11, 10, 11, 11, 11, 8, 11, 6, 7, 6, 6, 6, 6, 8, 7, 6, 10, 9, 6, 7, 6, 6, 7, 10, 6, 5, 6, 7, 7, 7, 10, 8, 11, 9, 13, 7, 14, 16, 12, 14, 14, 15, 15, 16, 16, 14, 15, 15, 15, 15, 15, 15, 15, 15, 14, 15, 13, 14, 14, 16, 15, 17, 14, 17, 15, 17, 12, 14, 13, 16, 12, 17, 13, 17, 14, 13, 13, 14, 14, 12, 13, 15, 15, 14, 15, 17, 14, 17, 15, 14, 15, 16, 12, 16, 15, 14, 15, 16, 15, 16, 17, 17, 15, 15, 17, 17, 13, 14, 15, 15, 13, 12, 16, 16, 17, 14, 15, 16, 15, 15, 13, 13, 15, 13, 16, 17, 15, 17, 17, 17, 16, 17, 14, 17, 14, 16, 15, 17, 15, 15, 14, 17, 15, 17, 15, 16, 15, 15, 16, 16, 14, 17, 17, 15, 15, 16, 15, 17, 15, 14, 16, 16, 16, 16, 16, 12, 4, 4, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 11, 10, 11, 11, 11, 11, 11, 12, 12, 12, 13, 13, 12, 13, 12, 14, 14, 12, 13, 13, 13, 13, 14, 12, 13, 13, 14, 14, 14, 13, 14, 14, 15, 15, 13, 15, 13, 17, 17, 17, 9, 17, 7,  };

static const unsigned char offset_code_lengths_5[11] ={ 6, 7, 7, 6, 4, 3, 2, 2, 3, 3, 6,  };

static const unsigned char *const first_code_lengths[5] ={ first_code_lengths_1, first_code_lengths_2, first_code_lengths_3, first_code_lengths_4, first_code_lengths_5 };

static const unsigned char *const second_code_lengths[5] ={ second_code_lengths_1, second_code_lengths_2, second_code_lengths_3, second_code_lengths_4, second_code_lengths_5 };

static const unsigned char *const offset_code_lengths[5] ={ offset_code_lengths_1, offset_code_lengths_2, offset_code_lengths_3, offset_code_lengths_4, offset_code_lengths_5 };

static const unsigned char offset_code_size[5] ={ 11, 13, 14, 11, 11 };

static const unsigned short meta_codes[37] ={ 0x5d8, 0x058, 0x040, 0x0c0, 0x000, 0x078, 0x02b, 0x014, 0x00c, 0x01c, 0x01b, 0x00b, 0x010, 0x020, 0x038, 0x018, 0x0d8, 0xbd8, 0x180, 0x680, 0x380, 0xf80, 0x780, 0x480, 0x080, 0x280, 0x3d8, 0xfd8, 0x7d8, 0x9d8, 0x1d8, 0x004, 0x001, 0x002, 0x007, 0x003, 0x008 };

static const unsigned char meta_code_lengths[37] ={ 11, 8, 8, 8, 8, 7, 6, 5, 5, 5, 5, 6, 5, 6, 7, 7, 9, 12, 10, 11, 11, 12, 12, 11, 11, 11, 12, 12, 12, 12, 12, 5, 2, 2, 3, 4, 5 };
// clang-format on

static unsigned int be16(const unsigned char *p)
{
	return ((unsigned int) p[0] << 8) | (unsigned int) p[1];
}

static unsigned int be32(const unsigned char *p)
{
	return ((unsigned int) p[0] << 24) |
	       ((unsigned int) p[1] << 16) |
	       ((unsigned int) p[2] << 8) |
	       (unsigned int) p[3];
}

static unsigned int be24(const unsigned char *p)
{
	return ((unsigned int) p[0] << 16) |
	       ((unsigned int) p[1] << 8) |
	       (unsigned int) p[2];
}

static unsigned int crc16_ibm(const unsigned char *data, size_t len)
{
	unsigned int crc = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		unsigned int bit;

		crc ^= data[i];
		for (bit = 0; bit < 8; bit++) {
			if (crc & 1u)
				crc = (crc >> 1) ^ 0xa001u;
			else
				crc >>= 1;
		}
	}

	return crc & 0xffffu;
}

static void copy_name_component(const unsigned char *src, unsigned int src_len,
                                char *dst, int dst_len)
{
	unsigned int i;
	int out_len = 0;

	for (i = 0; i < src_len && out_len < dst_len - 1; i++) {
		unsigned char c = src[i];

		if (c == '/' || c == '\\')
			dst[out_len++] = '_';
		else if (c >= 32 && c <= 126)
			dst[out_len++] = (char) c;
		else
			dst[out_len++] = '?';
	}

	dst[out_len] = '\0';
}

static int path_equals_ignore_case(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a;
		char cb = *b;

		if (ca >= 'A' && ca <= 'Z') ca = (char) (ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z') cb = (char) (cb - 'A' + 'a');
		if (ca != cb)
			return 0;
		a++;
		b++;
	}

	return *a == '\0' && *b == '\0';
}

static const char *basename_only(const char *path)
{
	const char *last = path;
	const char *p;

	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\')
			last = p + 1;
	}

	return last;
}

static int is_printable_name(const unsigned char *src, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i++) {
		if (src[i] < 32 || src[i] > 126)
			return 0;
	}

	return 1;
}

static int is_valid_method(unsigned int method)
{
	unsigned int folder_method = method & STI2_FOLDER_MASK;
	unsigned int compression_method = method & STI2_COMPRESSION_MASK;

	if (folder_method == STI2_START_FOLDER || folder_method == STI2_END_FOLDER)
		return 1;

	switch (compression_method) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 5:
		case 6:
		case 8:
		case 13:
		case 14:
		case 15:
			return 1;
		default:
			return 0;
	}
}

static int ext_matches(const char *filename, const char **extensions)
{
	const char *dot;

	if (!extensions)
		return 1;

	dot = strrchr(filename, '.');
	if (!dot || !dot[1])
		return 0;
	dot++;

	while (*extensions) {
		if (path_equals_ignore_case(dot, *extensions))
			return 1;
		extensions++;
	}

	return 0;
}

static int mkdirs_for_path(const char *path)
{
	char tmp[STI2_PATH_LEN * 2];
	char *p;

	if (!path)
		return -1;

	snprintf(tmp, sizeof(tmp), "%s", path);
	for (p = tmp + 1; *p; p++) {
		if (*p == '/' || *p == '\\') {
			char saved = *p;

			*p = '\0';
			if (tmp[0] && mkdir_one(tmp, 0755) != 0 && errno != EEXIST)
				return -1;
			*p = saved;
		}
	}

	if (tmp[0] && mkdir_one(tmp, 0755) != 0 && errno != EEXIST)
		return -1;

	return 0;
}

static int mkdirs_for_file(const char *path)
{
	char tmp[STI2_PATH_LEN * 2];
	char *p;

	if (!path)
		return -1;

	snprintf(tmp, sizeof(tmp), "%s", path);
	p = strrchr(tmp, '/');
	if (!p)
		p = strrchr(tmp, '\\');
	if (!p)
		return 0;
	*p = '\0';

	return mkdirs_for_path(tmp);
}

static int write_all(int fd, const unsigned char *buf, unsigned int len)
{
	while (len > 0) {
		int written = write_fd(fd, buf, len);

		if (written <= 0)
			return -1;
		buf += written;
		len -= (unsigned int) written;
	}

	return 0;
}

static int find_header_index_by_offset(const sti2_header_t *headers, int count, unsigned int offset)
{
	int i;

	for (i = 0; i < count; i++) {
		if (headers[i].offset == offset)
			return i;
	}

	return -1;
}

static int build_header_path(const sti2_header_t *headers, int count, int index,
                             char *out, int out_len, int depth)
{
	char parent_path[STI2_PATH_LEN];
	int parent_index;

	if (!headers || !out || out_len <= 0 || index < 0 || index >= count || depth > STI2_MAX_DEPTH)
		return -1;

	if (headers[index].parent_offset == 0) {
		snprintf(out, out_len, "%s", headers[index].name);
		return 0;
	}

	parent_index = find_header_index_by_offset(headers, count, headers[index].parent_offset);
	if (parent_index < 0) {
		snprintf(out, out_len, "%s", headers[index].name);
		return 0;
	}

	if (build_header_path(headers, count, parent_index, parent_path, sizeof(parent_path), depth + 1) < 0)
		return -1;
	if (parent_path[0]) {
		snprintf(out, out_len, "%s/%s", parent_path, headers[index].name);
		return 0;
	}

	snprintf(out, out_len, "%s", headers[index].name);
	return 0;
}

static int scan_headers(const unsigned char *archive_data, size_t archive_size,
                        sti2_header_t *headers, int *out_count)
{
	int count = 0;
	size_t i;

	for (i = 0; i + STI2_FILE_HEADER_SIZE <= archive_size; i++) {
		unsigned int resource_method = archive_data[i + SITFH_COMPRMETHOD];
		unsigned int data_method = archive_data[i + SITFH_COMPDMETHOD];
		unsigned int name_len = archive_data[i + SITFH_FNAMESIZE];

		if (name_len == 0 || name_len > 31)
			continue;
		if (!is_valid_method(resource_method) || !is_valid_method(data_method))
			continue;
		if (!is_printable_name(archive_data + i + SITFH_FNAME, name_len))
			continue;
		if (crc16_ibm(archive_data + i, SITFH_HDRCRC) != be16(archive_data + i + SITFH_HDRCRC))
			continue;
		if (count >= STI2_MAX_ENTRIES)
			return -1;

		headers[count].offset = (unsigned int) i;
		headers[count].parent_offset = be24(archive_data + i + SITFH_PARENTOFFS);
		headers[count].resource_uncompressed_size = be32(archive_data + i + SITFH_RSRCLENGTH);
		headers[count].data_uncompressed_size = be32(archive_data + i + SITFH_DATALENGTH);
		headers[count].resource_compressed_size = be32(archive_data + i + SITFH_COMPRLENGTH);
		headers[count].data_compressed_size = be32(archive_data + i + SITFH_COMPDLENGTH);
		headers[count].resource_method = resource_method;
		headers[count].data_method = data_method;
		headers[count].file_type = be32(archive_data + i + SITFH_FTYPE);
		headers[count].creator = be32(archive_data + i + SITFH_CREATOR);
		headers[count].finder_flags = be16(archive_data + i + SITFH_FNDRFLAGS);
		headers[count].is_directory = ((data_method & STI2_FOLDER_MASK) == STI2_START_FOLDER) ||
		                              ((resource_method & STI2_FOLDER_MASK) == STI2_START_FOLDER);
		copy_name_component(archive_data + i + SITFH_FNAME, name_len,
		                    headers[count].name, sizeof(headers[count].name));
		count++;
	}

	*out_count = count;
	return 0;
}

static unsigned int reverse32(unsigned int value)
{
	value = ((value >> 1) & 0x55555555u) | ((value & 0x55555555u) << 1);
	value = ((value >> 2) & 0x33333333u) | ((value & 0x33333333u) << 2);
	value = ((value >> 4) & 0x0f0f0f0fu) | ((value & 0x0f0f0f0fu) << 4);
	value = ((value >> 8) & 0x00ff00ffu) | ((value & 0x00ff00ffu) << 8);
	return (value >> 16) | (value << 16);
}

static unsigned int reverse_n(unsigned int value, int length)
{
	return reverse32(value) >> (32 - length);
}

static int leaf_value(int branch)
{
	return -branch + STI2_LEAF_BASE;
}

static int make_leaf(int value)
{
	return STI2_LEAF_BASE - value;
}

static int is_leaf_branch(int branch)
{
	return branch <= STI2_LEAF_BASE;
}

static void code_init(sti2_code_t *code)
{
	int i;

	memset(code, 0, sizeof(*code));
	code->node_count = 1;
	code->min_length = INT_MAX;
	code->max_length = INT_MIN;
	for (i = 0; i < STI2_CODE_MAX_NODES; i++) {
		code->nodes[i].left = STI2_OPEN_BRANCH;
		code->nodes[i].right = STI2_OPEN_BRANCH;
	}
	for (i = 0; i < STI2_CODE_TABLE_SIZE; i++) {
		code->table[i].length = -1;
		code->table[i].value = -1;
	}
}

static int new_node(sti2_code_t *code)
{
	int node;

	if (code->node_count >= STI2_CODE_MAX_NODES)
		return -1;
	node = code->node_count++;
	code->nodes[node].left = STI2_OPEN_BRANCH;
	code->nodes[node].right = STI2_OPEN_BRANCH;
	return node;
}

static int add_code_high(sti2_code_t *code, int value, unsigned int bits, int length)
{
	int node = 0;
	int bitpos;

	if (length <= 0)
		return 0;
	if (length > code->max_length)
		code->max_length = length;
	if (length < code->min_length)
		code->min_length = length;

	for (bitpos = length - 1; bitpos >= 0; bitpos--) {
		int bit = (bits >> bitpos) & 1u;
		int *branch = bit ? &code->nodes[node].right : &code->nodes[node].left;

		if (bitpos == 0) {
			if (*branch != STI2_OPEN_BRANCH)
				return -1;
			*branch = make_leaf(value);
		} else {
			if (*branch == STI2_OPEN_BRANCH) {
				int next = new_node(code);

				if (next < 0)
					return -1;
				*branch = next;
			} else if (is_leaf_branch(*branch)) {
				return -1;
			}
			node = *branch;
		}
	}

	return 0;
}

static int add_code_low(sti2_code_t *code, int value, unsigned int bits, int length)
{
	return add_code_high(code, value, reverse_n(bits, length), length);
}

static void make_table_le_branch(const sti2_code_t *code, int branch,
                                 sti2_code_table_entry_t *table,
                                 int depth, int max_depth)
{
	int table_size = 1 << (max_depth - depth);
	int stride = 1 << depth;
	int i;

	if (is_leaf_branch(branch)) {
		for (i = 0; i < table_size; i++) {
			table[i * stride].length = depth;
			table[i * stride].value = leaf_value(branch);
		}
	} else if (branch == STI2_OPEN_BRANCH) {
		for (i = 0; i < table_size; i++) {
			table[i * stride].length = -1;
			table[i * stride].value = -1;
		}
	} else if (depth == max_depth) {
		table[0].length = max_depth + 1;
		table[0].value = branch;
	} else {
		make_table_le_branch(code, code->nodes[branch].left, table, depth + 1, max_depth);
		make_table_le_branch(code, code->nodes[branch].right, table + stride, depth + 1, max_depth);
	}
}

static int finalize_code(sti2_code_t *code)
{
	code->table_bits = code->max_length < code->min_length ? STI2_CODE_TABLE_BITS : code->max_length;
	if (code->table_bits > STI2_CODE_TABLE_BITS)
		code->table_bits = STI2_CODE_TABLE_BITS;
	if (code->table_bits <= 0)
		code->table_bits = STI2_CODE_TABLE_BITS;

	make_table_le_branch(code, 0, code->table, 0, code->table_bits);
	return 0;
}

static int build_code_from_int_lengths(sti2_code_t *code, const int *lengths,
                                       int numsymbols, int max_length,
                                       int shortest_code_is_zero)
{
	unsigned int next_code = 0;
	int length;
	int symbol;

	code_init(code);
	for (length = 1; length <= max_length; length++) {
		for (symbol = 0; symbol < numsymbols; symbol++) {
			if (lengths[symbol] != length)
				continue;
			if (add_code_high(code, symbol,
			                  shortest_code_is_zero ? next_code : ~next_code,
			                  length) < 0)
				return -1;
			next_code++;
		}
		next_code <<= 1;
	}

	return finalize_code(code);
}

static int build_code_from_byte_lengths(sti2_code_t *code, const unsigned char *lengths,
                                        int numsymbols)
{
	int temp[STI2_METHOD13_SYMBOLS];
	int i;

	for (i = 0; i < numsymbols; i++)
		temp[i] = lengths[i];

	return build_code_from_int_lengths(code, temp, numsymbols, 32, 1);
}

static void br_init(sti2_bit_reader_t *br, const unsigned char *data, size_t size)
{
	memset(br, 0, sizeof(*br));
	br->data = data;
	br->size = size;
}

static void br_fill(sti2_bit_reader_t *br, int min_bits)
{
	while (br->bit_count < min_bits) {
		unsigned int next = 0;

		if (br->byte_pos < br->size)
			next = br->data[br->byte_pos++];
		br->bitbuf |= (unsigned long long) next << br->bit_count;
		br->bit_count += 8;
	}
}

static unsigned int br_peek(sti2_bit_reader_t *br, int bits)
{
	if (bits <= 0)
		return 0;
	br_fill(br, bits);
	return (unsigned int) (br->bitbuf & ((1ull << bits) - 1ull));
}

static void br_skip(sti2_bit_reader_t *br, int bits)
{
	if (bits <= 0)
		return;
	br_fill(br, bits);
	br->bitbuf >>= bits;
	br->bit_count -= bits;
}

static unsigned int br_get(sti2_bit_reader_t *br, int bits)
{
	unsigned int value = br_peek(br, bits);

	br_skip(br, bits);
	return value;
}

static int code_decode(sti2_bit_reader_t *br, const sti2_code_t *code)
{
	sti2_code_table_entry_t entry = code->table[br_peek(br, code->table_bits)];

	if (entry.length < 0)
		return -1;
	if (entry.length <= code->table_bits) {
		br_skip(br, entry.length);
		return entry.value;
	}

	br_skip(br, code->table_bits);
	while (entry.value >= 0) {
		int bit = (int) br_get(br, 1);
		int branch = bit ? code->nodes[entry.value].right : code->nodes[entry.value].left;

		if (branch == STI2_OPEN_BRANCH)
			return -1;
		if (is_leaf_branch(branch))
			return leaf_value(branch);
		entry.value = branch;
	}

	return -1;
}

static int build_meta_code(sti2_code_t *meta)
{
	int i;

	code_init(meta);
	for (i = 0; i < 37; i++) {
		if (add_code_low(meta, i, meta_codes[i], meta_code_lengths[i]) < 0)
			return -1;
	}

	return finalize_code(meta);
}

static int parse_dynamic_code(sti2_bit_reader_t *br, int numcodes,
                              const sti2_code_t *meta, sti2_code_t *out)
{
	int lengths[STI2_METHOD13_SYMBOLS];
	int current_length = 0;
	int i;

	if (numcodes > STI2_METHOD13_SYMBOLS)
		return -1;

	for (i = 0; i < numcodes; i++) {
		int value = code_decode(br, meta);

		if (value < 0)
			return -1;

		switch (value) {
			case 31:
				current_length = -1;
				break;

			case 32:
				current_length++;
				break;

			case 33:
				current_length--;
				break;

			case 34:
				if (br_get(br, 1)) {
					if (i >= numcodes)
						return -1;
					lengths[i++] = current_length;
				}
				break;

			case 35:
				value = (int) br_get(br, 3) + 2;
				while (value--) {
					if (i >= numcodes)
						return -1;
					lengths[i++] = current_length;
				}
				break;

			case 36:
				value = (int) br_get(br, 6) + 10;
				while (value--) {
					if (i >= numcodes)
						return -1;
					lengths[i++] = current_length;
				}
				break;

			default:
				current_length = value + 1;
				break;
		}

		if (i >= numcodes)
			return -1;
		lengths[i] = current_length;
	}

	return build_code_from_int_lengths(out, lengths, numcodes, 32, 1);
}

static int init_method13_decoder(sti2_method13_decoder_t *decoder,
                                 const unsigned char *data, size_t size)
{
	sti2_code_t meta;
	unsigned int mode_byte;
	unsigned int table_index;

	br_init(&decoder->br, data, size);
	mode_byte = br_get(&decoder->br, 8);
	table_index = mode_byte >> 4;

	if (table_index == 0) {
		if (build_meta_code(&meta) < 0)
			return -1;
		if (parse_dynamic_code(&decoder->br, STI2_METHOD13_SYMBOLS, &meta, &decoder->first_code) < 0)
			return -1;
		if (mode_byte & 0x08)
			decoder->second_code = decoder->first_code;
		else if (parse_dynamic_code(&decoder->br, STI2_METHOD13_SYMBOLS, &meta, &decoder->second_code) < 0)
			return -1;
		if (parse_dynamic_code(&decoder->br, (mode_byte & 0x07) + 10, &meta, &decoder->offset_code) < 0)
			return -1;
	} else if (table_index < 6) {
		table_index--;
		if (build_code_from_byte_lengths(&decoder->first_code,
		                                 first_code_lengths[table_index],
		                                 STI2_METHOD13_SYMBOLS) < 0)
			return -1;
		if (build_code_from_byte_lengths(&decoder->second_code,
		                                 second_code_lengths[table_index],
		                                 STI2_METHOD13_SYMBOLS) < 0)
			return -1;
		if (build_code_from_byte_lengths(&decoder->offset_code,
		                                 offset_code_lengths[table_index],
		                                 offset_code_size[table_index]) < 0)
			return -1;
	} else {
		return -1;
	}

	decoder->current_code = &decoder->first_code;
	return 0;
}

static int decompress_method13(const unsigned char *data, size_t comp_size,
                               unsigned char *out, unsigned int out_size)
{
	sti2_method13_decoder_t decoder;
	unsigned char window[STI2_METHOD13_WINDOW];
	unsigned int window_pos = 0;
	unsigned int out_pos = 0;

	if (!out && out_size != 0)
		return -1;
	if (init_method13_decoder(&decoder, data, comp_size) < 0)
		return -1;

	memset(window, 0, sizeof(window));
	while (out_pos < out_size) {
		int symbol = code_decode(&decoder.br, decoder.current_code);

		if (symbol < 0)
			return -1;
		if (symbol < 0x100) {
			unsigned char byte = (unsigned char) symbol;

			window[window_pos & (STI2_METHOD13_WINDOW - 1)] = byte;
			window_pos++;
			out[out_pos++] = byte;
			decoder.current_code = &decoder.first_code;
		} else if (symbol <= 0x13f) {
			unsigned int length;
			unsigned int offset;
			int bit_length;

			if (symbol < 0x13e)
				length = (unsigned int) (symbol - 0x100 + 3);
			else if (symbol == 0x13e)
				length = br_get(&decoder.br, 10) + 65u;
			else
				length = br_get(&decoder.br, 15) + 65u;

			bit_length = code_decode(&decoder.br, &decoder.offset_code);
			if (bit_length < 0)
				return -1;
			if (bit_length == 0)
				offset = 1;
			else if (bit_length == 1)
				offset = 2;
			else
				offset = (1u << (bit_length - 1)) + br_get(&decoder.br, bit_length - 1) + 1u;

			decoder.current_code = &decoder.second_code;
			while (length-- && out_pos < out_size) {
				unsigned int src_index = (window_pos - offset) & (STI2_METHOD13_WINDOW - 1);
				unsigned char byte = window[src_index];

				window[window_pos & (STI2_METHOD13_WINDOW - 1)] = byte;
				window_pos++;
				out[out_pos++] = byte;
			}
		} else {
			return -1;
		}
	}

	return (int) out_pos;
}

#define STI2_METHOD14_WINDOW 0x40000u

typedef struct {
	sti2_bit_reader_t br;
	unsigned char code[308];
	unsigned char code_copy[308];
	unsigned short freq[308];
	unsigned int buff[308];
	unsigned char var1[52];
	unsigned short var2[52];
	unsigned short var3[75 * 2];
	unsigned char var4[76];
	unsigned int var5[75];
	unsigned short var7[308 * 2];
	unsigned char window[STI2_METHOD14_WINDOW];
} sti2_method14_decoder_t;

static void br_byte_boundary(sti2_bit_reader_t *br)
{
	br_skip(br, br->bit_count & 7);
}

static void method14_update(unsigned short first, unsigned short last,
                            unsigned char *code, unsigned short *freq)
{
	unsigned short i;
	unsigned short j;

	while (last - first > 1) {
		i = first;
		j = last;
		do {
			while (++i < last && code[first] > code[i]);
			while (--j > first && code[first] < code[j]);
			if (j > i) {
				unsigned short t16;
				unsigned char t8;

				t8 = code[i];
				code[i] = code[j];
				code[j] = t8;
				t16 = freq[i];
				freq[i] = freq[j];
				freq[j] = t16;
			}
		} while (j > i);
		if (first != j) {
			unsigned short t16;
			unsigned char t8;

			t8 = code[first];
			code[first] = code[j];
			code[j] = t8;
			t16 = freq[first];
			freq[first] = freq[j];
			freq[j] = t16;
			i = j + 1;
			if (last - i <= j - first) {
				method14_update(i, last, code, freq);
				last = j;
			} else {
				method14_update(first, j, code, freq);
				first = i;
			}
		} else {
			first++;
		}
	}
}

static int method14_read_tree(sti2_method14_decoder_t *decoder,
                              unsigned short code_size,
                              unsigned short *result)
{
	unsigned int size;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	unsigned int l;
	unsigned int m;
	unsigned int n;
	unsigned int o;
	int zero_marker;

	k = br_get(&decoder->br, 1);
	j = br_get(&decoder->br, 2) + 2;
	o = br_get(&decoder->br, 3) + 1;
	size = 1u << j;
	m = size - 1u;
	zero_marker = k ? (int) m - 1 : -1;
	if (br_get(&decoder->br, 2) & 1u) {
		if (method14_read_tree(decoder, (unsigned short) size, decoder->freq) < 0)
			return -1;
		for (i = 0; i < code_size;) {
			l = 0;
			do {
				if (l + 1u >= size * 2u) return -1;
				l = decoder->freq[l + br_get(&decoder->br, 1)];
				n = size << 1;
			} while (n > l);
			l -= n;
			if (zero_marker != (int) l) {
				if (l == m) {
					l = 0;
					do {
						if (l + 1u >= size * 2u) return -1;
						l = decoder->freq[l + br_get(&decoder->br, 1)];
						n = size << 1;
					} while (n > l);
					l += 3u - n;
					while (l--) {
						if (i == 0 || i >= code_size) return -1;
						decoder->code[i] = decoder->code[i - 1];
						i++;
					}
				} else {
					decoder->code[i++] = (unsigned char) (l + o);
				}
			} else {
				decoder->code[i++] = 0;
			}
		}
	} else {
		for (i = 0; i < code_size;) {
			l = br_get(&decoder->br, (int) j);
			if (zero_marker != (int) l) {
				if (l == m) {
					l = br_get(&decoder->br, (int) j) + 3u;
					while (l--) {
						if (i == 0 || i >= code_size) return -1;
						decoder->code[i] = decoder->code[i - 1];
						i++;
					}
				} else {
					decoder->code[i++] = (unsigned char) (l + o);
				}
			} else {
				decoder->code[i++] = 0;
			}
		}
	}

	for (i = 0; i < code_size; i++) {
		decoder->code_copy[i] = decoder->code[i];
		decoder->freq[i] = (unsigned short) i;
	}
	method14_update(0, code_size, decoder->code_copy, decoder->freq);
	for (i = 0; i < code_size && !decoder->code_copy[i]; i++);
	for (j = 0; i < code_size; i++, j++) {
		if (i)
			j <<= decoder->code_copy[i] - decoder->code_copy[i - 1];
		k = decoder->code_copy[i];
		m = 0;
		for (l = j; k--; l >>= 1)
			m = (m << 1) | (l & 1u);
		decoder->buff[decoder->freq[i]] = m;
	}
	memset(result, 0, sizeof(*result) * code_size * 2u);
	j = 2;
	for (i = 0; i < code_size; i++) {
		l = 0;
		m = decoder->buff[i];
		for (k = 0; k < decoder->code[i]; k++) {
			l += (m & 1u);
			if (decoder->code[i] - 1u <= k) {
				result[l] = (unsigned short) (code_size * 2u + i);
			} else {
				if (!result[l]) {
					if (j + 1u >= code_size * 2u) return -1;
					result[l] = (unsigned short) j;
					j += 2;
				}
				l = result[l];
			}
			m >>= 1;
		}
	}
	br_byte_boundary(&decoder->br);
	return 0;
}

static int method14_decode_symbol(sti2_method14_decoder_t *decoder,
                                  const unsigned short *tree,
                                  unsigned int leaf_base,
                                  unsigned int *out_symbol)
{
	unsigned int i = 0;

	while (i < leaf_base) {
		unsigned int next = tree[i + br_get(&decoder->br, 1)];

		if (next == 0)
			return -1;
		i = next;
	}
	*out_symbol = i - leaf_base;
	return 0;
}

static int decompress_method14(const unsigned char *data, size_t comp_size,
                               unsigned char *out, unsigned int out_size)
{
	sti2_method14_decoder_t *decoder;
	unsigned int blocks;
	unsigned int window_pos = 0;
	unsigned int out_pos = 0;
	unsigned int i;
	unsigned int k;
	unsigned int l;
	unsigned int n;
	int status = -1;

	if (!data || (!out && out_size != 0)) return -1;
	decoder = (sti2_method14_decoder_t *) calloc(1, sizeof(*decoder));
	if (!decoder) return -1;
	br_init(&decoder->br, data, comp_size);
	for (i = k = 0; i < 52; i++) {
		decoder->var2[i] = (unsigned short) k;
		decoder->var1[i] = (unsigned char) ((i >= 4) ? ((i - 4) >> 2) : 0);
		k += 1u << decoder->var1[i];
	}
	for (i = 0, k = 1; i < 75; i++) {
		decoder->var5[i] = k;
		decoder->var4[i] = (unsigned char) ((i >= 3) ? ((i - 3) >> 2) : 0);
		k += 1u << decoder->var4[i];
	}
	blocks = br_get(&decoder->br, 16);
	while (blocks-- && out_pos < out_size) {
		br_get(&decoder->br, 16);
		br_get(&decoder->br, 16);
		n = br_get(&decoder->br, 16);
		n |= br_get(&decoder->br, 16) << 16;
		if (method14_read_tree(decoder, 308, decoder->var7) < 0 ||
		    method14_read_tree(decoder, 75, decoder->var3) < 0)
			goto cleanup;
		while (n && out_pos < out_size) {
			if (method14_decode_symbol(decoder, decoder->var7, 616, &i) < 0)
				goto cleanup;
			if (i < 0x100u) {
				decoder->window[window_pos++] = (unsigned char) i;
				window_pos &= STI2_METHOD14_WINDOW - 1u;
				out[out_pos++] = (unsigned char) i;
				n--;
			} else {
				i -= 0x100u;
				if (i >= 52u) goto cleanup;
				k = decoder->var2[i] + 4u;
				if (decoder->var1[i])
					k += br_get(&decoder->br, decoder->var1[i]);
				if (method14_decode_symbol(decoder, decoder->var3, 150, &i) < 0 || i >= 75u)
					goto cleanup;
				l = decoder->var5[i];
				if (decoder->var4[i])
					l += br_get(&decoder->br, decoder->var4[i]);
				if (k > n) goto cleanup;
				n -= k;
				l = window_pos + STI2_METHOD14_WINDOW - l;
				while (k-- && out_pos < out_size) {
					unsigned char byte;

					l &= STI2_METHOD14_WINDOW - 1u;
					byte = decoder->window[l++];
					decoder->window[window_pos++] = byte;
					window_pos &= STI2_METHOD14_WINDOW - 1u;
					out[out_pos++] = byte;
				}
			}
		}
		br_byte_boundary(&decoder->br);
	}
	status = (out_pos == out_size) ? (int) out_pos : -1;

cleanup:
	free(decoder);
	return status;
}

#define STI2_METHOD15_NUM_BITS       26
#define STI2_METHOD15_ONE            (1 << (STI2_METHOD15_NUM_BITS - 1))
#define STI2_METHOD15_HALF           (1 << (STI2_METHOD15_NUM_BITS - 2))
#define STI2_METHOD15_MAX_BLOCK_BITS 24

static const uint16_t method15_randomization_table[256] = {
	0xee,
	0x56,
	0xf8,
	0xc3,
	0x9d,
	0x9f,
	0xae,
	0x2c,
	0xad,
	0xcd,
	0x24,
	0x9d,
	0xa6,
	0x101,
	0x18,
	0xb9,
	0xa1,
	0x82,
	0x75,
	0xe9,
	0x9f,
	0x55,
	0x66,
	0x6a,
	0x86,
	0x71,
	0xdc,
	0x84,
	0x56,
	0x96,
	0x56,
	0xa1,
	0x84,
	0x78,
	0xb7,
	0x32,
	0x6a,
	0x3,
	0xe3,
	0x2,
	0x11,
	0x101,
	0x8,
	0x44,
	0x83,
	0x100,
	0x43,
	0xe3,
	0x1c,
	0xf0,
	0x86,
	0x6a,
	0x6b,
	0xf,
	0x3,
	0x2d,
	0x86,
	0x17,
	0x7b,
	0x10,
	0xf6,
	0x80,
	0x78,
	0x7a,
	0xa1,
	0xe1,
	0xef,
	0x8c,
	0xf6,
	0x87,
	0x4b,
	0xa7,
	0xe2,
	0x77,
	0xfa,
	0xb8,
	0x81,
	0xee,
	0x77,
	0xc0,
	0x9d,
	0x29,
	0x20,
	0x27,
	0x71,
	0x12,
	0xe0,
	0x6b,
	0xd1,
	0x7c,
	0xa,
	0x89,
	0x7d,
	0x87,
	0xc4,
	0x101,
	0xc1,
	0x31,
	0xaf,
	0x38,
	0x3,
	0x68,
	0x1b,
	0x76,
	0x79,
	0x3f,
	0xdb,
	0xc7,
	0x1b,
	0x36,
	0x7b,
	0xe2,
	0x63,
	0x81,
	0xee,
	0xc,
	0x63,
	0x8b,
	0x78,
	0x38,
	0x97,
	0x9b,
	0xd7,
	0x8f,
	0xdd,
	0xf2,
	0xa3,
	0x77,
	0x8c,
	0xc3,
	0x39,
	0x20,
	0xb3,
	0x12,
	0x11,
	0xe,
	0x17,
	0x42,
	0x80,
	0x2c,
	0xc4,
	0x92,
	0x59,
	0xc8,
	0xdb,
	0x40,
	0x76,
	0x64,
	0xb4,
	0x55,
	0x1a,
	0x9e,
	0xfe,
	0x5f,
	0x6,
	0x3c,
	0x41,
	0xef,
	0xd4,
	0xaa,
	0x98,
	0x29,
	0xcd,
	0x1f,
	0x2,
	0xa8,
	0x87,
	0xd2,
	0xa0,
	0x93,
	0x98,
	0xef,
	0xc,
	0x43,
	0xed,
	0x9d,
	0xc2,
	0xeb,
	0x81,
	0xe9,
	0x64,
	0x23,
	0x68,
	0x1e,
	0x25,
	0x57,
	0xde,
	0x9a,
	0xcf,
	0x7f,
	0xe5,
	0xba,
	0x41,
	0xea,
	0xea,
	0x36,
	0x1a,
	0x28,
	0x79,
	0x20,
	0x5e,
	0x18,
	0x4e,
	0x7c,
	0x8e,
	0x58,
	0x7a,
	0xef,
	0x91,
	0x2,
	0x93,
	0xbb,
	0x56,
	0xa1,
	0x49,
	0x1b,
	0x79,
	0x92,
	0xf3,
	0x58,
	0x4f,
	0x52,
	0x9c,
	0x2,
	0x77,
	0xaf,
	0x2a,
	0x8f,
	0x49,
	0xd0,
	0x99,
	0x4d,
	0x98,
	0x101,
	0x60,
	0x93,
	0x100,
	0x75,
	0x31,
	0xce,
	0x49,
	0x20,
	0x56,
	0x57,
	0xe2,
	0xf5,
	0x26,
	0x2b,
	0x8a,
	0xbf,
	0xde,
	0xd0,
	0x83,
	0x34,
	0xf4,
	0x17,
};

static void msb_br_init(sti2_msb_bit_reader_t *br, const unsigned char *data, size_t size)
{
	memset(br, 0, sizeof(*br));
	br->data = data;
	br->size = size;
}

static int msb_br_get_bit(sti2_msb_bit_reader_t *br)
{
	int bit;

	if (br->byte_pos >= br->size)
		return 0;
	bit = (br->data[br->byte_pos] >> (7 - br->bit_pos)) & 1;
	br->bit_pos++;
	if (br->bit_pos == 8) {
		br->bit_pos = 0;
		br->byte_pos++;
	}
	return bit;
}

static int msb_br_get_bits(sti2_msb_bit_reader_t *br, int bits)
{
	int value = 0;
	int i;

	for (i = 0; i < bits; i++)
		value = (value << 1) | msb_br_get_bit(br);
	return value;
}

static void method15_reset_arithmetic_model(sti2_arithmetic_model_t *model)
{
	int i;

	model->total_frequency = model->increment * model->num_symbols;
	for (i = 0; i < model->num_symbols; i++)
		model->symbols[i].frequency = model->increment;
}

static int method15_init_arithmetic_model(sti2_arithmetic_model_t *model,
                                          int first_symbol, int last_symbol,
                                          int increment, int frequency_limit)
{
	int i;

	if (!model || first_symbol > last_symbol || last_symbol - first_symbol + 1 > 128)
		return -1;
	memset(model, 0, sizeof(*model));
	model->increment = increment;
	model->frequency_limit = frequency_limit;
	model->num_symbols = last_symbol - first_symbol + 1;
	for (i = 0; i < model->num_symbols; i++)
		model->symbols[i].symbol = i + first_symbol;
	method15_reset_arithmetic_model(model);
	return 0;
}

static void method15_increase_frequency(sti2_arithmetic_model_t *model, int sym_index)
{
	int i;

	model->symbols[sym_index].frequency += model->increment;
	model->total_frequency += model->increment;
	if (model->total_frequency > model->frequency_limit) {
		model->total_frequency = 0;
		for (i = 0; i < model->num_symbols; i++) {
			model->symbols[i].frequency++;
			model->symbols[i].frequency >>= 1;
			model->total_frequency += model->symbols[i].frequency;
		}
	}
}

static void method15_init_arithmetic_decoder(sti2_arithmetic_decoder_t *decoder,
                                             sti2_msb_bit_reader_t *input)
{
	decoder->input = input;
	decoder->range = STI2_METHOD15_ONE;
	decoder->code = msb_br_get_bits(input, STI2_METHOD15_NUM_BITS);
}

static int method15_read_next_code(sti2_arithmetic_decoder_t *decoder,
                                   int sym_low, int sym_size, int sym_total)
{
	int renorm_factor;
	int low_incr;

	if (sym_total <= 0 || sym_size <= 0)
		return -1;
	renorm_factor = decoder->range / sym_total;
	if (renorm_factor <= 0)
		return -1;
	low_incr = renorm_factor * sym_low;
	decoder->code -= low_incr;
	if (sym_low + sym_size == sym_total)
		decoder->range -= low_incr;
	else
		decoder->range = sym_size * renorm_factor;
	while (decoder->range <= STI2_METHOD15_HALF) {
		decoder->range <<= 1;
		decoder->code = (decoder->code << 1) | msb_br_get_bit(decoder->input);
	}
	return 0;
}

static int method15_next_symbol(sti2_arithmetic_decoder_t *decoder,
                                sti2_arithmetic_model_t *model)
{
	int divisor;
	int frequency;
	int cumulative = 0;
	int n;

	if (!decoder || !model || model->total_frequency <= 0)
		return -1;
	divisor = decoder->range / model->total_frequency;
	if (divisor <= 0)
		return -1;
	frequency = decoder->code / divisor;
	if (frequency < 0)
		return -1;
	for (n = 0; n < model->num_symbols - 1; n++) {
		if (cumulative + model->symbols[n].frequency > frequency)
			break;
		cumulative += model->symbols[n].frequency;
	}
	if (method15_read_next_code(decoder, cumulative,
	                            model->symbols[n].frequency,
	                            model->total_frequency) < 0)
		return -1;
	method15_increase_frequency(model, n);
	return model->symbols[n].symbol;
}

static int method15_next_bit_string(sti2_arithmetic_decoder_t *decoder,
                                    sti2_arithmetic_model_t *model,
                                    int bits, int *out_value)
{
	unsigned int result = 0;
	int i;

	if (!out_value || bits < 0 || bits > 32)
		return -1;
	for (i = 0; i < bits; i++) {
		int symbol = method15_next_symbol(decoder, model);

		if (symbol < 0)
			return -1;
		if (symbol)
			result |= 1u << i;
	}
	*out_value = (int) result;
	return 0;
}

static void method15_reset_mtf(sti2_mtf_state_t *mtf)
{
	int i;

	for (i = 0; i < 256; i++)
		mtf->table[i] = i;
}

static int method15_decode_mtf(sti2_mtf_state_t *mtf, int symbol)
{
	int result;
	int i;

	if (symbol < 0 || symbol > 255)
		return -1;
	result = mtf->table[symbol];
	for (i = symbol; i > 0; i--)
		mtf->table[i] = mtf->table[i - 1];
	mtf->table[0] = result;
	return result;
}

static void method15_calculate_inverse_bwt(uint32_t *transform,
                                           const unsigned char *block,
                                           int block_len)
{
	int counts[256] = { 0 };
	int cumulative_counts[256];
	int total = 0;
	int i;

	for (i = 0; i < block_len; i++)
		counts[block[i]]++;
	for (i = 0; i < 256; i++) {
		cumulative_counts[i] = total;
		total += counts[i];
		counts[i] = 0;
	}
	for (i = 0; i < block_len; i++) {
		transform[cumulative_counts[block[i]] + counts[block[i]]] = (uint32_t) i;
		counts[block[i]]++;
	}
}

static int method15_read_block(sti2_method15_decoder_t *state)
{
	int end_marker;
	int i;
	uint32_t *new_transform;

	method15_reset_mtf(&state->mtf);
	state->randomized = method15_next_symbol(&state->decoder, &state->initial_model);
	if (state->randomized < 0)
		return -1;
	if (method15_next_bit_string(&state->decoder, &state->initial_model,
	                             state->block_bits, &state->transform_index) < 0)
		return -1;
	state->num_bytes = 0;
	for (;;) {
		int sel = method15_next_symbol(&state->decoder, &state->selector_model);
		int symbol;

		if (sel < 0)
			return -1;
		if (sel == 0 || sel == 1) {
			int zero_state = 1;
			int zero_count = 0;
			int fill;

			while (sel < 2) {
				if (zero_state > state->block_size)
					return -1;
				zero_count += (sel == 0) ? zero_state : 2 * zero_state;
				if (zero_count < 0 || zero_count > state->block_size)
					return -1;
				zero_state *= 2;
				sel = method15_next_symbol(&state->decoder, &state->selector_model);
				if (sel < 0)
					return -1;
			}
			if (state->num_bytes + zero_count > state->block_size)
				return -1;
			fill = method15_decode_mtf(&state->mtf, 0);
			if (fill < 0)
				return -1;
			memset(&state->block[state->num_bytes], fill, (size_t) zero_count);
			state->num_bytes += zero_count;
		}

		if (sel == 10)
			break;
		if (sel == 2)
			symbol = 1;
		else if (sel >= 3 && sel <= 9)
			symbol = method15_next_symbol(&state->decoder, &state->mtf_model[sel - 3]);
		else
			return -1;
		if (symbol < 0 || state->num_bytes >= state->block_size)
			return -1;
		symbol = method15_decode_mtf(&state->mtf, symbol);
		if (symbol < 0)
			return -1;
		state->block[state->num_bytes++] = (unsigned char) symbol;
	}
	if (state->num_bytes <= 0 || state->transform_index >= state->num_bytes)
		return -1;
	method15_reset_arithmetic_model(&state->selector_model);
	for (i = 0; i < 7; i++)
		method15_reset_arithmetic_model(&state->mtf_model[i]);
	end_marker = method15_next_symbol(&state->decoder, &state->initial_model);
	if (end_marker < 0)
		return -1;
	if (end_marker) {
		int ignored_crc;

		if (method15_next_bit_string(&state->decoder, &state->initial_model, 32, &ignored_crc) < 0)
			return -1;
		state->end_of_blocks = 1;
	}
	new_transform = (uint32_t *) malloc(sizeof(*new_transform) * (size_t) state->num_bytes);
	if (!new_transform)
		return -1;
	free(state->transform);
	state->transform = new_transform;
	method15_calculate_inverse_bwt(state->transform, state->block, state->num_bytes);
	return 0;
}

static int method15_init_decoder(sti2_method15_decoder_t *state,
                                 sti2_msb_bit_reader_t *br)
{
	int marker;

	memset(state, 0, sizeof(*state));
	method15_init_arithmetic_decoder(&state->decoder, br);
	if (method15_init_arithmetic_model(&state->initial_model, 0, 1, 1, 256) < 0 ||
	    method15_init_arithmetic_model(&state->selector_model, 0, 10, 8, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[0], 2, 3, 8, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[1], 4, 7, 4, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[2], 8, 15, 4, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[3], 16, 31, 4, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[4], 32, 63, 2, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[5], 64, 127, 2, 1024) < 0 ||
	    method15_init_arithmetic_model(&state->mtf_model[6], 128, 255, 1, 1024) < 0)
		return -1;
	if (method15_next_bit_string(&state->decoder, &state->initial_model, 8, &marker) < 0 || marker != 'A')
		return -1;
	if (method15_next_bit_string(&state->decoder, &state->initial_model, 8, &marker) < 0 || marker != 's')
		return -1;
	if (method15_next_bit_string(&state->decoder, &state->initial_model, 4, &state->block_bits) < 0)
		return -1;
	state->block_bits += 9;
	if (state->block_bits < 9 || state->block_bits > STI2_METHOD15_MAX_BLOCK_BITS)
		return -1;
	state->block_size = 1 << state->block_bits;
	state->block = (unsigned char *) malloc((size_t) state->block_size);
	if (!state->block)
		return -1;
	state->end_of_blocks = method15_next_symbol(&state->decoder, &state->initial_model);
	if (state->end_of_blocks < 0)
		return -1;
	return 0;
}

static int decompress_method15(const unsigned char *data, size_t comp_size,
                               unsigned char *out, unsigned int out_size)
{
	sti2_msb_bit_reader_t br;
	sti2_method15_decoder_t state;
	unsigned int out_pos = 0;
	int status = -1;

	if (!data || (!out && out_size != 0))
		return -1;
	msb_br_init(&br, data, comp_size);
	if (method15_init_decoder(&state, &br) < 0)
		goto cleanup;
	while (out_pos < out_size) {
		int out_byte;

		if (state.repeat) {
			state.repeat--;
			out_byte = state.last;
		} else {
		retry_byte:
			if (state.byte_count >= state.num_bytes) {
				if (state.end_of_blocks)
					goto cleanup;
				if (method15_read_block(&state) < 0)
					goto cleanup;
				state.byte_count = 0;
				state.count = 0;
				state.last = 0;
				state.rand_index = 0;
				state.rand_count = method15_randomization_table[0];
			}
			state.transform_index = (int) state.transform[state.transform_index];
			out_byte = state.block[state.transform_index];
			if (state.randomized && state.rand_count == state.byte_count) {
				out_byte ^= 1;
				state.rand_index = (state.rand_index + 1) & 255;
				state.rand_count += method15_randomization_table[state.rand_index];
			}
			state.byte_count++;
			if (state.count == 4) {
				state.count = 0;
				if (out_byte == 0)
					goto retry_byte;
				state.repeat = out_byte - 1;
				out_byte = state.last;
			} else {
				if (out_byte == state.last)
					state.count++;
				else {
					state.count = 1;
					state.last = out_byte;
				}
			}
		}
		out[out_pos++] = (unsigned char) out_byte;
	}
	status = (int) out_pos;

cleanup:
	free(state.block);
	free(state.transform);
	return status;
}

static int extract_entry_data(const unsigned char *archive_data, size_t archive_size,
                              const sti2_entry_t *entry, unsigned char **out_data,
                              unsigned int *out_size)
{
	const unsigned char *src;
	unsigned char *dst;

	if (!archive_data || !entry || !out_data || !out_size || entry->is_directory)
		return -1;
	if ((size_t) entry->data_offset > archive_size ||
	    (size_t) entry->compressed_size > archive_size - (size_t) entry->data_offset)
		return -1;
	if (!dxx_extract_entry_allowed(entry->uncompressed_size, entry->compressed_size) ||
	    !dxx_extract_memory_allowed(entry->uncompressed_size, 0))
		return -1;

	*out_data = NULL;
	*out_size = 0;
	if (entry->uncompressed_size == 0)
		return 0;

	dst = (unsigned char *) malloc(entry->uncompressed_size);
	if (!dst)
		return -1;
	src = archive_data + entry->data_offset;

	switch (entry->data_method) {
		case 0:
			if (entry->compressed_size < entry->uncompressed_size) {
				free(dst);
				return -1;
			}
			memcpy(dst, src, entry->uncompressed_size);
			break;

		case 13:
			if (decompress_method13(src, entry->compressed_size,
			                        dst, entry->uncompressed_size) < 0) {
				free(dst);
				return -1;
			}
			break;

		case 14:
			if (decompress_method14(src, entry->compressed_size,
			                        dst, entry->uncompressed_size) < 0) {
				free(dst);
				return -1;
			}
			break;

		case 15:
			if (decompress_method15(src, entry->compressed_size,
			                        dst, entry->uncompressed_size) < 0) {
				free(dst);
				return -1;
			}
			break;

		default:
			free(dst);
			return -1;
	}

	*out_data = dst;
	*out_size = entry->uncompressed_size;
	return 0;
}

int sti2_is_archive(const unsigned char *archive_data, size_t archive_size)
{
	if (!archive_data || archive_size < STI2_ARCHIVE_HEADER_SIZE)
		return 0;
	if (be32(archive_data + 10) != STI2_HEADER_SIGNATURE)
		return 0;
	if (archive_data[0] == 'S' && archive_data[1] == 'I' &&
	    archive_data[2] == 'T' && archive_data[3] == '!')
		return 1;
	if (archive_data[0] != 'S' || archive_data[1] != 'T')
		return 0;
	if (archive_data[2] == 'i' && (archive_data[3] == 'n' || (archive_data[3] >= '0' && archive_data[3] <= '9')))
		return 1;
	if (archive_data[2] >= '0' && archive_data[2] <= '9' &&
	    archive_data[3] >= '0' && archive_data[3] <= '9')
		return 1;
	return 0;
}

int sti2_list_entries(const unsigned char *archive_data, size_t archive_size,
                      sti2_entry_list_t *out)
{
	sti2_header_t headers[STI2_MAX_ENTRIES];
	unsigned int declared_total_size;
	int header_count;
	int i;

	if (!out || !sti2_is_archive(archive_data, archive_size))
		return -1;

	memset(out, 0, sizeof(*out));
	declared_total_size = be32(archive_data + 6);
	if (declared_total_size < STI2_ARCHIVE_HEADER_SIZE || declared_total_size > archive_size)
		return -1;

	out->declared_file_count = be16(archive_data + 4);
	out->declared_total_size = declared_total_size;
	if (scan_headers(archive_data, declared_total_size, headers, &header_count) < 0)
		return -1;

	for (i = 0; i < header_count; i++) {
		sti2_entry_t *entry;
		char path[STI2_PATH_LEN];
		int is_end_folder;

		is_end_folder = ((headers[i].data_method & STI2_FOLDER_MASK) == STI2_END_FOLDER) ||
		                ((headers[i].resource_method & STI2_FOLDER_MASK) == STI2_END_FOLDER);
		if (is_end_folder)
			continue;
		if (build_header_path(headers, header_count, i, path, sizeof(path), 0) < 0)
			return -1;

		if (out->num_entries >= STI2_MAX_ENTRIES)
			return -1;
		entry = &out->entries[out->num_entries++];
		memset(entry, 0, sizeof(*entry));
		snprintf(entry->path, sizeof(entry->path), "%s", path);
		entry->header_offset = headers[i].offset;
		entry->resource_offset = headers[i].offset + STI2_FILE_HEADER_SIZE;
		entry->resource_compressed_size = headers[i].resource_compressed_size;
		entry->resource_uncompressed_size = headers[i].resource_uncompressed_size;
		entry->data_offset = headers[i].offset + STI2_FILE_HEADER_SIZE + headers[i].resource_compressed_size;
		entry->compressed_size = headers[i].data_compressed_size;
		entry->uncompressed_size = headers[i].data_uncompressed_size;
		entry->data_method = headers[i].data_method & STI2_COMPRESSION_MASK;
		entry->resource_method = headers[i].resource_method & STI2_COMPRESSION_MASK;
		entry->file_type = headers[i].file_type;
		entry->creator = headers[i].creator;
		entry->finder_flags = headers[i].finder_flags;
		entry->is_directory = headers[i].is_directory;
		if (!headers[i].is_directory &&
		    headers[i].data_uncompressed_size == 0 && headers[i].resource_uncompressed_size != 0) {
			out->num_entries--;
		}
	}

	return out->num_entries;
}

int sti2_find_entry_index(const sti2_entry_list_t *list, const char *path)
{
	int i;

	if (!list || !path)
		return -1;

	for (i = 0; i < list->num_entries; i++) {
		if (path_equals_ignore_case(list->entries[i].path, path) ||
		    path_equals_ignore_case(basename_only(list->entries[i].path), path))
			return i;
	}

	return -1;
}

int sti2_extract_entry(const unsigned char *archive_data, size_t archive_size,
                       const sti2_entry_t *entry, const char *output_path)
{
	unsigned char *data;
	unsigned int size;
	int fd;

	if (!output_path)
		return -1;
	if (extract_entry_data(archive_data, archive_size, entry, &data, &size) < 0)
		return -1;
	if (mkdirs_for_file(output_path) < 0) {
		free(data);
		return -1;
	}

	fd = open_fd(output_path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644);
	if (fd < 0) {
		free(data);
		return -1;
	}
	if (size != 0 && write_all(fd, data, size) < 0) {
		close_fd(fd);
		free(data);
		return -1;
	}

	close_fd(fd);
	free(data);
	return (int) size;
}

int sti2_extract_matching(const unsigned char *archive_data, size_t archive_size,
                          const char **extensions, const char *output_dir,
                          sti2_progress_fn progress, void *user_data)
{
	sti2_entry_list_t list;
	long long bytes_done = 0;
	long long bytes_total = 0;
	uint64_t output_bytes = 0;
	int extracted = 0;
	int i;

	if (!output_dir)
		return -1;
	if (sti2_list_entries(archive_data, archive_size, &list) < 0)
		return -1;
	if (mkdirs_for_path(output_dir) < 0)
		return -1;

	for (i = 0; i < list.num_entries; i++) {
		if (list.entries[i].is_directory ||
		    !ext_matches(basename_only(list.entries[i].path), extensions))
			continue;
		if (!dxx_extract_entry_allowed(list.entries[i].uncompressed_size,
		                               list.entries[i].compressed_size) ||
		    dxx_extract_add_bytes(&output_bytes, list.entries[i].uncompressed_size,
		                          DXX_EXTRACT_MAX_TOTAL_BYTES) < 0)
			return -1;
	}
	if (!dxx_extract_has_free_space(output_dir, output_bytes))
		return -1;
	bytes_total = (long long) output_bytes;

	for (i = 0; i < list.num_entries; i++) {
		char output_path[STI2_PATH_LEN * 2];
		const char *name;
		int written;

		if (list.entries[i].is_directory)
			continue;
		name = basename_only(list.entries[i].path);
		if (!ext_matches(name, extensions))
			continue;

		snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, name);
		written = sti2_extract_entry(archive_data, archive_size, &list.entries[i], output_path);
		if (written < 0)
			return -1;
		bytes_done += written;
		extracted++;
		if (progress && progress(name, bytes_done, bytes_total, user_data) != 0)
			return -1;
	}

	return extracted;
}
