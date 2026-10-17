/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 *
 * Routines to read/write pcx images.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gr.h"
#include "grdef.h"
#include "u_mem.h"
#include "pcx.h"
#include "physfsx.h"


int pcx_encode_byte(ubyte byt, ubyte cnt, PHYSFS_file *fid);
int pcx_encode_line(ubyte *inBuff, int inLen, PHYSFS_file *fp);

/* PCX Header data type */
typedef struct {
	ubyte   Manufacturer;
	ubyte   Version;
	ubyte   Encoding;
	ubyte   BitsPerPixel;
	short   Xmin;
	short   Ymin;
	short   Xmax;
	short   Ymax;
	short   Hdpi;
	short   Vdpi;
	ubyte   ColorMap[16][3];
	ubyte   Reserved;
	ubyte   Nplanes;
	short   BytesPerLine;
	ubyte   filler[60];
} __pack__ PCXHeader;

#define PCXHEADER_SIZE 128
#define PCX_MAX_DIMENSION 8192
#define PCX_MAX_DECODED_BYTES (64u * 1024u * 1024u)
#define PCX_MAX_ENCODED_BYTES (2u * PCX_MAX_DECODED_BYTES + PCXHEADER_SIZE + 769u)

/*
 * reads n PCXHeader structs from a PHYSFS_file
 */
int PCXHeader_read_n(PCXHeader *ph, int n, PHYSFS_file *fp)
{
	int i;

	for (i = 0; i < n; i++) {
		if (PHYSFS_fileLength(fp) - PHYSFS_tell(fp) < PCXHEADER_SIZE)
			return i;
		ph->Manufacturer = PHYSFSX_readByte(fp);
		ph->Version = PHYSFSX_readByte(fp);
		ph->Encoding = PHYSFSX_readByte(fp);
		ph->BitsPerPixel = PHYSFSX_readByte(fp);
		ph->Xmin = PHYSFSX_readShort(fp);
		ph->Ymin = PHYSFSX_readShort(fp);
		ph->Xmax = PHYSFSX_readShort(fp);
		ph->Ymax = PHYSFSX_readShort(fp);
		ph->Hdpi = PHYSFSX_readShort(fp);
		ph->Vdpi = PHYSFSX_readShort(fp);
		PHYSFS_read( fp, &ph->ColorMap, 16*3, 1 );
		ph->Reserved = PHYSFSX_readByte(fp);
		ph->Nplanes = PHYSFSX_readByte(fp);
		ph->BytesPerLine = PHYSFSX_readShort(fp);
		PHYSFS_read(fp, &ph->filler, 60, 1);
	}
	return i;
}

static int pcx_validate_dimensions(const PCXHeader *header, int *width, int *height)
{
	const int xsize = (int)header->Xmax - (int)header->Xmin + 1;
	const int ysize = (int)header->Ymax - (int)header->Ymin + 1;
	const int bytes_per_line = header->BytesPerLine;
	if (xsize <= 0 || ysize <= 0 || xsize > PCX_MAX_DIMENSION || ysize > PCX_MAX_DIMENSION ||
	    bytes_per_line < xsize || bytes_per_line > PCX_MAX_DIMENSION ||
	    (unsigned)bytes_per_line * (unsigned)ysize > PCX_MAX_DECODED_BYTES)
		return 0;
	*width = xsize;
	*height = ysize;
	return 1;
}

static int pcx_read_rle_row(PHYSFS_file *file, ubyte *row, int row_size)
{
	int offset = 0;
	while (offset < row_size) {
		ubyte data;
		int count = 1;
		if (PHYSFS_read(file, &data, 1, 1) != 1)
			return 0;
		if ((data & 0xc0) == 0xc0) {
			count = data & 0x3f;
			if (!count || count > row_size - offset || PHYSFS_read(file, &data, 1, 1) != 1)
				return 0;
		}
		memset(row + offset, data, count);
		offset += count;
	}
	return 1;
}

static int pcx_read_rle_row_memory(const ubyte **cursor, const ubyte *end, ubyte *row, int row_size)
{
	int offset = 0;
	while (offset < row_size) {
		ubyte data;
		int count = 1;
		if (*cursor == end)
			return 0;
		data = *(*cursor)++;
		if ((data & 0xc0) == 0xc0) {
			count = data & 0x3f;
			if (!count || count > row_size - offset || *cursor == end)
				return 0;
			data = *(*cursor)++;
		}
		memset(row + offset, data, count);
		offset += count;
	}
	return 1;
}

int bald_guy_load( char * filename, grs_bitmap * bmp,int bitmap_type ,ubyte * palette )
{
	PCXHeader header;
	PHYSFS_file * PCXfile;
	PHYSFS_sint64 file_size;
	int allocated = 0;
	int i, row, col, xsize, ysize;
	size_t payload_size;
	ubyte xor_value, *bguy_data, *row_data = NULL;
	const ubyte *p, *end;
	
	PCXfile = PHYSFSX_openReadBuffered( filename );
	if ( !PCXfile )
		return PCX_ERROR_OPENING;
	file_size = PHYSFS_fileLength(PCXfile);
	if (file_size <= PCXHEADER_SIZE || file_size > PCX_MAX_ENCODED_BYTES + 1u) {
		PHYSFS_close(PCXfile);
		return PCX_ERROR_READING;
	}
	bguy_data = d_malloc((size_t)file_size);
	if (!bguy_data) {
		PHYSFS_close(PCXfile);
		return PCX_ERROR_MEMORY;
	}
	if (PHYSFS_read(PCXfile, bguy_data, 1, (PHYSFS_uint32)file_size) != file_size) {
		PHYSFS_close(PCXfile);
		d_free(bguy_data);
		return PCX_ERROR_READING;
	}
	PHYSFS_close(PCXfile);
	payload_size = (size_t)file_size - 1;
	xor_value = bguy_data[payload_size] - 1;
	for (i = 0; (size_t)i < payload_size / 2; ++i) {
		const size_t opposite = payload_size - (size_t)i - 1;
		const ubyte low = bguy_data[i];
		const ubyte high = bguy_data[opposite];
		bguy_data[i] = high ^ (ubyte)(xor_value - i);
		bguy_data[opposite] = low ^ (ubyte)(xor_value - opposite);
	}
	if (payload_size & 1u) {
		const size_t middle = payload_size / 2;
		bguy_data[middle] ^= (ubyte)(xor_value - middle);
	}

	p = bguy_data;
	end = bguy_data + payload_size;
	memcpy( &header, p, sizeof(PCXHeader) );
	p += sizeof(PCXHeader);
	
	// Is it a 256 color PCX file?
	if ((header.Manufacturer != 10)||(header.Encoding != 1)||(header.Nplanes != 1)||(header.BitsPerPixel != 8)||(header.Version != 5))	{
		d_free(bguy_data);
		return PCX_ERROR_WRONG_VERSION;
	}
	header.Xmin= INTEL_SHORT(header.Xmin);
	header.Xmax = INTEL_SHORT(header.Xmax);
	header.Ymin = INTEL_SHORT(header.Ymin);
	header.Ymax = INTEL_SHORT(header.Ymax);
	header.BytesPerLine = INTEL_SHORT(header.BytesPerLine);
	if (!pcx_validate_dimensions(&header, &xsize, &ysize)) {
		d_free(bguy_data);
		return PCX_ERROR_WRONG_VERSION;
	}

	if ( bmp->bm_data == NULL )	{
		gr_init_bitmap_alloc(bmp, bitmap_type, 0, 0, xsize, ysize, xsize);
		allocated = 1;
	}
	if (!bmp->bm_data || bmp->bm_w < xsize || bmp->bm_h < ysize ||
	    (bmp->bm_type == BM_LINEAR && bmp->bm_rowsize < xsize))
		goto bald_reading_error;
	row_data = d_malloc(header.BytesPerLine);
	if (!row_data)
		goto bald_memory_error;
	for (row = 0; row < ysize; ++row) {
		if (!pcx_read_rle_row_memory(&p, end, row_data, header.BytesPerLine))
			goto bald_reading_error;
		if (bmp->bm_type == BM_LINEAR)
			memcpy(&bmp->bm_data[bmp->bm_rowsize * row], row_data, xsize);
		else
			for (col = 0; col < xsize; ++col)
				gr_bm_pixel(bmp, col, row, row_data[col]);
	}
	d_free(row_data);
	row_data = NULL;
	if (palette != NULL) {
		if (p == end || *p++ != 12 || (size_t)(end - p) < 768)
			goto bald_reading_error;
		for (i = 0; i < 768; i++) {
			palette[i] = *p++ >> 2;
		}
	}
	d_free(bguy_data);
	return PCX_ERROR_NONE;

bald_memory_error:
	d_free(row_data);
	d_free(bguy_data);
	if (allocated)
		gr_free_bitmap_data(bmp);
	return PCX_ERROR_MEMORY;

bald_reading_error:
	d_free(row_data);
	d_free(bguy_data);
	if (allocated)
		gr_free_bitmap_data(bmp);
	return PCX_ERROR_READING;
}

#ifdef __ANDROID__
#include "android_mod_assets.h"
#endif

int pcx_read_bitmap( char * filename, grs_bitmap * bmp,int bitmap_type ,ubyte * palette )
{
	PCXHeader header;
	PHYSFS_file * PCXfile;
	int allocated = 0;
	int i, row, col, xsize, ysize;
	ubyte data, *row_data = NULL;

	PCXfile = PHYSFSX_openReadBuffered( filename );
	if ( !PCXfile )
		return PCX_ERROR_OPENING;

	// read 128 char PCX header
	if (PCXHeader_read_n( &header, 1, PCXfile )!=1) {
		PHYSFS_close( PCXfile );
		return PCX_ERROR_NO_HEADER;
	}

	// Is it a 256 color PCX file?
	if ((header.Manufacturer != 10)||(header.Encoding != 1)||(header.Nplanes != 1)||(header.BitsPerPixel != 8)||(header.Version != 5))	{
		PHYSFS_close( PCXfile );
		return PCX_ERROR_WRONG_VERSION;
	}

	if (!pcx_validate_dimensions(&header, &xsize, &ysize)) {
		PHYSFS_close(PCXfile);
		return PCX_ERROR_WRONG_VERSION;
	}

	if ( bitmap_type == BM_LINEAR )	{
		if ( bmp->bm_data == NULL )	{
			gr_init_bitmap_alloc (bmp, bitmap_type, 0, 0, xsize, ysize, xsize);
			allocated = 1;
		}
	}
	if (!bmp->bm_data || bmp->bm_w < xsize || bmp->bm_h < ysize ||
	    (bmp->bm_type == BM_LINEAR && bmp->bm_rowsize < xsize))
		goto reading_error;
	row_data = d_malloc(header.BytesPerLine);
	if (!row_data)
		goto memory_error;

	for (row = 0; row < ysize; ++row) {
		if (!pcx_read_rle_row(PCXfile, row_data, header.BytesPerLine))
			goto reading_error;
		if (bmp->bm_type == BM_LINEAR)
			memcpy(&bmp->bm_data[bmp->bm_rowsize * row], row_data, xsize);
		else
			for (col = 0; col < xsize; ++col)
				gr_bm_pixel(bmp, col, row, row_data[col]);
	}
	d_free(row_data);
	row_data = NULL;

	// Read the extended palette at the end of PCX file
	if ( palette != NULL )	{
		// Read in a character which should be 12 to be extended palette file
		if (PHYSFS_read( PCXfile, &data, 1, 1 )==1)	{
			if ( data == 12 )	{
				if (PHYSFS_read(PCXfile, palette,768, 1)!=1)	{
					goto reading_error;
				}
				for (i=0; i<768; i++ )
					palette[i] >>= 2;
			}
		} else {
			if (allocated)
				gr_free_bitmap_data(bmp);
			PHYSFS_close(PCXfile);
			return PCX_ERROR_NO_PALETTE;
		}
	}
	PHYSFS_close(PCXfile);
#ifdef __ANDROID__
	android_mod_asset_loaded("image", filename, bmp->bm_w, bmp->bm_h);
#endif
	return PCX_ERROR_NONE;

memory_error:
	if (allocated)
		gr_free_bitmap_data(bmp);
	PHYSFS_close(PCXfile);
	return PCX_ERROR_MEMORY;

reading_error:
	d_free(row_data);
	if (allocated)
		gr_free_bitmap_data(bmp);
	PHYSFS_close(PCXfile);
	return PCX_ERROR_READING;
}

int pcx_write_bitmap( char * filename, grs_bitmap * bmp, ubyte * palette )
{
	int retval;
	int i;
	ubyte data;
	PCXHeader header;
	PHYSFS_file *PCXfile;

	memset( &header, 0, PCXHEADER_SIZE );

	header.Manufacturer = 10;
	header.Encoding = 1;
	header.Nplanes = 1;
	header.BitsPerPixel = 8;
	header.Version = 5;
	header.Xmax = bmp->bm_w-1;
	header.Ymax = bmp->bm_h-1;
	header.BytesPerLine = bmp->bm_w;

	PCXfile = PHYSFSX_openWriteBuffered(filename);
	if ( !PCXfile )
		return PCX_ERROR_OPENING;

	if (PHYSFS_write(PCXfile, &header, PCXHEADER_SIZE, 1) != 1)
	{
		PHYSFS_close(PCXfile);
		return PCX_ERROR_WRITING;
	}

	for (i=0; i<bmp->bm_h; i++ )	{
		if (!pcx_encode_line( &bmp->bm_data[bmp->bm_rowsize*i], bmp->bm_w, PCXfile ))	{
			PHYSFS_close(PCXfile);
			return PCX_ERROR_WRITING;
		}
	}

	// Mark an extended palette
	data = 12;
	if (PHYSFS_write(PCXfile, &data, 1, 1) != 1)
	{
		PHYSFS_close(PCXfile);
		return PCX_ERROR_WRITING;
	}

	// Write the extended palette
	for (i=0; i<768; i++ )
		palette[i] <<= 2;

	retval = PHYSFS_write(PCXfile, palette, 768, 1);

	for (i=0; i<768; i++ )
		palette[i] >>= 2;

	if (retval !=1)	{
		PHYSFS_close(PCXfile);
		return PCX_ERROR_WRITING;
	}

	PHYSFS_close(PCXfile);
	return PCX_ERROR_NONE;

}

// returns number of bytes written into outBuff, 0 if failed
int pcx_encode_line(ubyte *inBuff, int inLen, PHYSFS_file *fp)
{
	ubyte this, last;
	int srcIndex, i;
	register int total;
	register ubyte runCount; 	// max single runlength is 63
	total = 0;
	last = *(inBuff);
	runCount = 1;

	for (srcIndex = 1; srcIndex < inLen; srcIndex++) {
		this = *(++inBuff);
		if (this == last)	{
			runCount++;			// it encodes
			if (runCount == 63)	{
				if (!(i=pcx_encode_byte(last, runCount, fp)))
					return(0);
				total += i;
				runCount = 0;
			}
		} else {   	// this != last
			if (runCount)	{
				if (!(i=pcx_encode_byte(last, runCount, fp)))
					return(0);
				total += i;
			}
			last = this;
			runCount = 1;
		}
	}

	if (runCount)	{		// finish up
		if (!(i=pcx_encode_byte(last, runCount, fp)))
			return 0;
		return total + i;
	}
	return total;
}

// subroutine for writing an encoded byte pair
// returns count of bytes written, 0 if error
int pcx_encode_byte(ubyte byt, ubyte cnt, PHYSFS_file *fid)
{
	if (cnt) {
		if ( (cnt==1) && (0xc0 != (0xc0 & byt)) )	{
			if(EOF == PHYSFSX_putc(fid, (int)byt))
				return 0; 	// disk write error (probably full)
			return 1;
		} else {
			if(EOF == PHYSFSX_putc(fid, (int)0xC0 | cnt))
				return 0; 	// disk write error
			if(EOF == PHYSFSX_putc(fid, (int)byt))
				return 0; 	// disk write error
			return 2;
		}
	}
	return 0;
}

//text for error messges
static const char pcx_error_messages[] = {
	"No error.\0"
	"Error opening file.\0"
	"Couldn't read PCX header.\0"
	"Unsupported PCX version.\0"
	"Error reading data.\0"
	"Couldn't find palette information.\0"
	"Error writing data.\0"
};


//function to return pointer to error message
const char *pcx_errormsg(int error_number)
{
	const char *p = pcx_error_messages;

	while (error_number--) {

		if (!p) return NULL;

		p += strlen(p)+1;

	}

	return p;
}
