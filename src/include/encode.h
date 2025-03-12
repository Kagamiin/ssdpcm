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

#ifndef __ENCODE_H__
#define __ENCODE_H__

#include "block.h"
#include <stdint.h>

typedef struct sigma_tracker_methods_s
{
	// Allocates memory for the state structure and its members.
	void (*alloc)(void **state);
	
	// Initialize/reset the sigma tracker's state structure, associating it to a block iterator.
	void (*init)(void **state, ssdpcm_block_iterator *iter);
	
	// Given the current state and a delta, calculate the absolute error at that delta's sample point.
	uint64_t (*calc_error)(void **state, codeword_t delta);
	
	// Advance the sigma tracker's decoder by one sample and accumulate the calculated error.
	void (*advance)(void **state);
	
	// Get the accumulated error.
	uint64_t (*get_accumulated_error)(void **state);
	
	// Free the sigma tracker's state structure and its members.
	void (*free)(void **state);
} const *sigma_tracker_methods;

typedef struct
{
	sigma_tracker_methods methods;
	void *state;
} sigma_tracker;

typedef struct
{
	ssdpcm_block_iterator iter;
	sigma_tracker *sigma;
} ssdpcm_encoder;

uint64_t ssdpcm_encode_bruteforce (ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma);

uint64_t ssdpcm_encode_binary_search (ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma);

uint64_t ssdpcm_block_encode (ssdpcm_block *block, sample_t *in, sigma_tracker *sigma);

/* ------------------------------------------------------------------------- */
// Error tracking method implementations
/* ------------------------------------------------------------------------- */

void sigma_tracker_alloc (void **ext_state);
void sigma_tracker_init (void **ext_state, ssdpcm_block_iterator *iter);
uint64_t sigma_tracker_get_accumulated_error (void **ext_state);
void sigma_tracker_free (void **ext_state);

uint64_t sigma_generic_calc_error (void **ext_state, codeword_t delta);
void sigma_generic_advance (void **ext_state);

uint64_t sigma_generic_comb_calc_error (void **ext_state, codeword_t delta);
void sigma_generic_comb_advance (void **ext_state);

uint64_t sigma_u8_overflow_calc_error (void **ext_state, codeword_t delta);
void sigma_u8_overflow_advance (void **ext_state);

uint64_t sigma_u7_overflow_calc_error (void **ext_state, codeword_t delta);
void sigma_u7_overflow_advance (void **ext_state);

uint64_t sigma_u7_overflow_comb_calc_error (void **ext_state, codeword_t delta);
void sigma_u7_overflow_comb_advance (void **ext_state);

uint64_t sigma_u8_overflow_comb_calc_error (void **ext_state, codeword_t delta);
void sigma_u8_overflow_comb_advance (void **ext_state);

// Generic SSDPCM error tracker, with no sample pre-filter or postprocessing
extern sigma_tracker_methods sigma_generic;

// Generic SSDPCM error tracker, with simple 2-sample comb filter
extern sigma_tracker_methods sigma_generic_comb;

// SSDPCM error tracker for non-clamped unsigned 8-bit PCM, with no sample pre-filter or postprocessing
extern sigma_tracker_methods sigma_u8_overflow;

// SSDPCM error tracker for non-clamped unsigned 7-bit PCM, with no sample pre-filter or postprocessing
extern sigma_tracker_methods sigma_u7_overflow;

// SSDPCM error tracker for non-clamped unsigned 7-bit PCM, with simple 2-sample comb filter
extern sigma_tracker_methods sigma_u7_overflow_comb;

// SSDPCM error tracker for non-clamped unsigned 7-bit PCM, with simple 2-sample comb filter
extern sigma_tracker_methods sigma_u8_overflow_comb;

#endif
