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
#ifdef OGL
#include "palette.h"
#endif

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
		PHYSFS_read(fp, &ph->ColorMap, 16*3, 1);
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

struct PCX_PHYSFS_file
{
	PHYSFS_file *PCXfile;
};

static int pcx_read_bitmap_file(struct PCX_PHYSFS_file *const pcxphysfs, grs_bitmap * bmp,int bitmap_type ,ubyte * palette);

#ifdef __ANDROID__
#include "android_mod_assets.h"
#endif

int pcx_read_bitmap( char * filename, grs_bitmap * bmp,int bitmap_type ,ubyte * palette )
{
	struct PCX_PHYSFS_file pcxphysfs;
	int result;
	pcxphysfs.PCXfile = PHYSFSX_openReadBuffered( filename );
	if (!pcxphysfs.PCXfile)
		return PCX_ERROR_OPENING;
	result = pcx_read_bitmap_file(&pcxphysfs, bmp, bitmap_type, palette);
	PHYSFS_close(pcxphysfs.PCXfile);
#ifdef __ANDROID__
	if (result == PCX_ERROR_NONE) android_mod_asset_loaded("image", filename, bmp->bm_w, bmp->bm_h);
#endif
	return result;
}

static int PCX_PHYSFS_read(struct PCX_PHYSFS_file *pcxphysfs, ubyte *data, unsigned size)
{
	return PHYSFS_read(pcxphysfs->PCXfile, data, size, sizeof(*data));
}

static int pcx_read_rle_row(struct PCX_PHYSFS_file *pcxphysfs, ubyte *row, int row_size)
{
	int offset = 0;
	while (offset < row_size) {
		ubyte data;
		int count = 1;
		if (PCX_PHYSFS_read(pcxphysfs, &data, 1) != 1)
			return 0;
		if ((data & 0xc0) == 0xc0) {
			count = data & 0x3f;
			if (!count || count > row_size - offset || PCX_PHYSFS_read(pcxphysfs, &data, 1) != 1)
				return 0;
		}
		memset(row + offset, data, count);
		offset += count;
	}
	return 1;
}

static int pcx_read_bitmap_file(struct PCX_PHYSFS_file *const pcxphysfs, grs_bitmap * bmp,int bitmap_type ,ubyte * palette)
{
	PCXHeader header;
	int allocated = 0;
	int i, row, col, xsize, ysize;
	ubyte data, *row_data = NULL;

	// read 128 char PCX header
	if (PCXHeader_read_n( &header, 1, pcxphysfs->PCXfile )!=1) {
		return PCX_ERROR_NO_HEADER;
	}

	// Is it a 256 color PCX file?
	if ((header.Manufacturer != 10)||(header.Encoding != 1)||(header.Nplanes != 1)||(header.BitsPerPixel != 8)||(header.Version != 5))	{
		return PCX_ERROR_WRONG_VERSION;
	}

	if (!pcx_validate_dimensions(&header, &xsize, &ysize))
		return PCX_ERROR_WRONG_VERSION;

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
		if (!pcx_read_rle_row(pcxphysfs, row_data, header.BytesPerLine))
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
		if (PCX_PHYSFS_read(pcxphysfs, &data, 1) == 1)	{
			if ( data == 12 )	{
				if (PCX_PHYSFS_read(pcxphysfs, palette, 768) != 1)	{
					goto reading_error;
				}
				for (i=0; i<768; i++ )
					palette[i] >>= 2;
			}
		} else {
			if (allocated)
				gr_free_bitmap_data(bmp);
			return PCX_ERROR_NO_PALETTE;
		}
	}
	return PCX_ERROR_NONE;

memory_error:
	if (allocated)
		gr_free_bitmap_data(bmp);
	return PCX_ERROR_MEMORY;

reading_error:
	d_free(row_data);
	if (allocated)
		gr_free_bitmap_data(bmp);
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
	ubyte ub, last;
	int srcIndex, i;
	register int total;
	register ubyte runCount; 	// max single runlength is 63
	total = 0;
	last = *(inBuff);
	runCount = 1;

	for (srcIndex = 1; srcIndex < inLen; srcIndex++) {
		ub = *(++inBuff);
		if (ub == last)	{
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
			last = ub;
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
