/*
 * ssdpcm: reference implementation for the SSDPCM audio codec designed by
 * Algorithm.
 * Copyright (C) 2022-2025 Kagamiin~
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
