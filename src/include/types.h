#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "verify.h"

#define FALSE 0
#define TRUE 1

typedef uint_fast8_t codeword_t;
#define CODEWORD_WIDTH 8

typedef int_fast32_t sample_t;
#define SAMPLE_MAX INT32_MAX
#define SAMPLE_MIN INT32_MIN

// General-purpose byte-wise buffer for reading or writing
typedef struct
{
	uint8_t *buffer;
	int buffer_size;
	int offset;
} buffer_u8;

// General-purpose bitstream reader/writer structure
typedef struct
{
	buffer_u8 byte_buf;
	uint8_t bit_index;
} bitstream_buffer;

typedef enum
{
	W_READ,
	W_WRITE,
	W_CREATE,
} wav_open_mode;

typedef enum
{
	W_U8,
	W_S16LE,
	W_SSDPCM,
} wav_sample_fmt;

typedef enum
{
	SS_SS1,
	SS_SS1C,
	SS_SS1_6,
	SS_SS2,
	SS_SS2_3,
	SS_SS3,
	
	NUM_SSDPCM_MODES,
} ssdpcm_block_mode;

#endif
