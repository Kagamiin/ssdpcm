#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "types.h"

typedef struct
{
	sample_t initial_sample;
	uint8_t num_deltas;

	sample_t *slopes;

	codeword_t *deltas;
	size_t length;
} ssdpcm_block;

typedef struct
{
	ssdpcm_block *block;
	sample_t *raw;
	size_t index;
	sample_t sample_state;
} ssdpcm_block_iterator;

void ssdpcm_block_decode (sample_t *out, ssdpcm_block *block);

#endif
