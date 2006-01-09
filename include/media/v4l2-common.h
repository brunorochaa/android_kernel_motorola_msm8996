/*
    v4l2 common internal API header

    This header contains internal shared ioctl definitions for use by the
    internal low-level v4l2 drivers.
    Each ioctl begins with VIDIOC_INT_ to clearly mark that it is an internal
    define,

    Copyright (C) 2005  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef V4L2_COMMON_H_
#define V4L2_COMMON_H_

/* VIDIOC_INT_AUDIO_CLOCK_FREQ */
enum v4l2_audio_clock_freq {
	V4L2_AUDCLK_32_KHZ  = 32000,
	V4L2_AUDCLK_441_KHZ = 44100,
	V4L2_AUDCLK_48_KHZ  = 48000,
};

/* VIDIOC_INT_G_REGISTER and VIDIOC_INT_S_REGISTER */
struct v4l2_register {
	u32 i2c_id; 		/* I2C driver ID of the I2C chip. 0 for the I2C adapter. */
	unsigned long reg;
	u32 val;
};

/* VIDIOC_INT_DECODE_VBI_LINE */
struct v4l2_decode_vbi_line {
	u32 is_second_field;	/* Set to 0 for the first (odd) field,
				   set to 1 for the second (even) field. */
	u8 *p; 			/* Pointer to the sliced VBI data from the decoder.
				   On exit points to the start of the payload. */
	u32 line;		/* Line number of the sliced VBI data (1-23) */
	u32 type;		/* VBI service type (V4L2_SLICED_*). 0 if no service found */
};

/* VIDIOC_INT_G_CHIP_IDENT: identifies the actual chip installed on the board */
enum v4l2_chip_ident {
	/* general idents: reserved range 0-49 */
	V4L2_IDENT_UNKNOWN = 0,

	/* module saa7115: reserved range 100-149 */
	V4L2_IDENT_SAA7114 = 104,
	V4L2_IDENT_SAA7115 = 105,

	/* module saa7127: reserved range 150-199 */
	V4L2_IDENT_SAA7127 = 157,
	V4L2_IDENT_SAA7129 = 159,

	/* module cx25840: reserved range 200-249 */
	V4L2_IDENT_CX25840 = 240,
	V4L2_IDENT_CX25841 = 241,
	V4L2_IDENT_CX25842 = 242,
	V4L2_IDENT_CX25843 = 243,
};

/* only implemented if CONFIG_VIDEO_ADV_DEBUG is defined */
#define	VIDIOC_INT_S_REGISTER 		_IOR ('d', 100, struct v4l2_register)
#define	VIDIOC_INT_G_REGISTER 		_IOWR('d', 101, struct v4l2_register)

/* Reset the I2C chip */
#define VIDIOC_INT_RESET            	_IO  ('d', 102)

/* Set the frequency of the audio clock output.
   Used to slave an audio processor to the video decoder, ensuring that audio
   and video remain synchronized. */
#define VIDIOC_INT_AUDIO_CLOCK_FREQ 	_IOR ('d', 103, enum v4l2_audio_clock_freq)

/* Video decoders that support sliced VBI need to implement this ioctl.
   Field p of the v4l2_sliced_vbi_line struct is set to the start of the VBI
   data that was generated by the decoder. The driver then parses the sliced
   VBI data and sets the other fields in the struct accordingly. The pointer p
   is updated to point to the start of the payload which can be copied
   verbatim into the data field of the v4l2_sliced_vbi_data struct. If no
   valid VBI data was found, then the type field is set to 0 on return. */
#define VIDIOC_INT_DECODE_VBI_LINE  	_IOWR('d', 104, struct v4l2_decode_vbi_line)

/* Used to generate VBI signals on a video signal. v4l2_sliced_vbi_data is
   filled with the data packets that should be output. Note that if you set
   the line field to 0, then that VBI signal is disabled. */
#define VIDIOC_INT_S_VBI_DATA 		_IOW ('d', 105, struct v4l2_sliced_vbi_data)

/* Used to obtain the sliced VBI packet from a readback register. Not all
   video decoders support this. If no data is available because the readback
   register contains invalid or erroneous data -EIO is returned. Note that
   you must fill in the 'id' member and the 'field' member (to determine
   whether CC data from the first or second field should be obtained). */
#define VIDIOC_INT_G_VBI_DATA 		_IOWR('d', 106, struct v4l2_sliced_vbi_data *)

/* Returns the chip identifier or V4L2_IDENT_UNKNOWN if no identification can
   be made. */
#define VIDIOC_INT_G_CHIP_IDENT		_IOR ('d', 107, enum v4l2_chip_ident *)

/* Sets I2S speed in bps. This is used to provide a standard way to select I2S
   clock used by driving digital audio streams at some board designs.
   Usual values for the frequency are 1024000 and 2048000.
   If the frequency is not supported, then -EINVAL is returned. */
#define VIDIOC_INT_I2S_CLOCK_FREQ 	_IOW ('d', 108, u32)


#endif /* V4L2_COMMON_H_ */
