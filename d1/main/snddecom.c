//THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
//SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
//END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
//ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
//IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
//SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
//FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
//CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
//AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
//COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
//
// $Source: /cvsroot/dxx-rebirth/d1x-rebirth/main/snddecom.c,v $
// $Revision: 1.1.1.1 $
// $Author: zicodxx $
// $Date: 2006/03/17 19:43:28 $
//
// Routines for compressing digital sounds.
//
// $Log: snddecom.c,v $
// Revision 1.1.1.1  2006/03/17 19:43:28  zicodxx
// initial import
//
// Revision 1.1.1.1  1999/06/14 22:11:34  donut
// Import of d1x 1.37 source.
//
// Revision 2.0  1995/02/27  11:29:36  john
// New version 2.0, which has no anonymous unions, builds with
// Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
//
// Revision 1.2  1994/11/30  14:08:46  john
// First version.
// Revision 1.1  1994/11/29  14:33:51  john
// Initial revision
//

#include "d1_shareware_sound.h"

void sound_decompress(unsigned char *data, int size, unsigned char *outp)
{
 if (size >= 0)
  d1_shareware_sound_decode(data, (size_t)size, outp, (size_t)size * 2);
}
