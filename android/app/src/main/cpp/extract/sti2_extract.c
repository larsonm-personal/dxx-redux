/*
 * sti2_extract.c - STi2 installer archive listing and extraction.
 *
 * The method 13 decompressor in this file is derived primarily from
 * XADMaster's StuffIt support, especially XADStuffIt13Handle and
 * XADPrefixCode, which are distributed under LGPL-2.1.
 * https://github.com/ashang/unar
 */

#include <errno.h>
#include <limits.h>
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
	int extracted = 0;
	int i;

	if (!output_dir)
		return -1;
	if (sti2_list_entries(archive_data, archive_size, &list) < 0)
		return -1;
	if (mkdirs_for_path(output_dir) < 0)
		return -1;

	for (i = 0; i < list.num_entries; i++) {
		if (!list.entries[i].is_directory &&
		    ext_matches(basename_only(list.entries[i].path), extensions))
			bytes_total += list.entries[i].uncompressed_size;
	}

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