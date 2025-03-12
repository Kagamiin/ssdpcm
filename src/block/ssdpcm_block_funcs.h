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

#ifndef __SSDPCM_BLOCK_FUNCS_H__
#define __SSDPCM_BLOCK_FUNCS_H__

#include "block.h"
#include "encode.h"
#include "errors.h"
#include <stdint.h>
#include <types.h>

typedef struct {
	sample_t *input_buf;
	ssdpcm_block_iterator *sigma_dec;
	sample_t *decode_buf;
	size_t decode_bufsize;
	uint64_t acc_error;
	float filter_coeff[8];
	float filter_input_state[8];
	float filter_output_state[8];
} sigma_tracker_internal;

/* ------------------------------------------------------------------------- */
// Block functions
/* ------------------------------------------------------------------------- */

void block_iterator_init_ (ssdpcm_block_iterator *iter, ssdpcm_block *block, sample_t *raw);

sample_t calc_sample_ (ssdpcm_block_iterator *dec, codeword_t slope_code);
void decode_one_sample_no_advance_ (ssdpcm_block_iterator *dec);
void decode_one_sample_ (ssdpcm_block_iterator *dec);
void enqueue_one_codeword_ (ssdpcm_block_iterator *dest, codeword_t c);

codeword_t find_best_delta_ (ssdpcm_encoder *enc);

#endif
