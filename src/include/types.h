#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include "verify.h"

typedef enum 
{
	FALSE,
	TRUE
} bool;

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

#endif
