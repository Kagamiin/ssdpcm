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

#include <stdint.h>
#include <types.h>
#include <encode.h>
#include <errors.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static inline uint64_t
do_binary_search_internal_ (
	ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma, uint8_t num_deltas, uint8_t chop_bits, sample_t *ranges_low, sample_t *ranges_hi, sample_t max_abs_delta)
{
	int i;
	sample_t *best_slopes;
	uint64_t best_metric = UINT64_MAX;
	uint8_t half_num_deltas = num_deltas / 2;
	
	best_slopes = calloc(num_deltas, sizeof(sample_t));
	
	for (i = 0; i < half_num_deltas; i++)
	{
		best_slopes[i] = dest->slopes[i];
		best_slopes[i + half_num_deltas] = -dest->slopes[i];
	}
	
	while (dest->slopes[0] <= max_abs_delta && dest->slopes[0] <= ranges_hi[0])
	{
		uint64_t sigma_metric = ssdpcm_block_encode(dest, in, sigma);
		
#if 0
		for (i = 0; i < num_deltas; i++)
		{
			fprintf(stderr, "%7ld ", dest->slopes[i]);
		}
		fprintf(stderr, "\n");
#endif
		
		if (sigma_metric < best_metric)
		{
			best_metric = sigma_metric;
			memcpy(best_slopes, dest->slopes, half_num_deltas * 2 * sizeof(sample_t));
		}
		
		for (i = half_num_deltas - 1; i >= 0; i--)
		{
			dest->slopes[i] += 1 << chop_bits;
			if (i > 0 && (dest->slopes[i] >= dest->slopes[i - 1] || dest->slopes[i] > ranges_hi[i]))
			{
				dest->slopes[i] = ranges_low[i];
				dest->slopes[i + half_num_deltas] = -ranges_low[i];
			}
			else
			{
				dest->slopes[i + half_num_deltas] = -dest->slopes[i];
				break;
			}
		}
	}
	
	memcpy(dest->slopes, best_slopes, num_deltas * sizeof(sample_t));
	free(best_slopes);
	return best_metric;
}

#define CHOP_PARAM 4

static inline void
do_binary_search_ (
	ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma, uint8_t num_deltas, sample_t max_abs_delta)
{
	int i;
	sample_t *best_slopes;
	uint8_t half_num_deltas = num_deltas / 2;
	int8_t chop_bits = round(log2(max_abs_delta)) - CHOP_PARAM;
	sample_t *ranges_low, *ranges_high;
	
	best_slopes = calloc(num_deltas, sizeof(sample_t));
	ranges_low = calloc(half_num_deltas, sizeof(sample_t));
	ranges_high = calloc(half_num_deltas, sizeof(sample_t));
	
	if (chop_bits < 0)
	{
		chop_bits = 0;
	}
	
	for (i = 0; i < half_num_deltas; i++)
	{
		dest->slopes[i] = (half_num_deltas - i - 1) << chop_bits;
		best_slopes[i] = (half_num_deltas - i - 1) << chop_bits;
		dest->slopes[i + half_num_deltas] = -dest->slopes[i];
		best_slopes[i + half_num_deltas] = -dest->slopes[i];
		ranges_low[i] = 0;
		ranges_high[i] = INT32_MAX;
	}
	
	(void) do_binary_search_internal_(dest, in, sigma, num_deltas, chop_bits, ranges_low, ranges_high, max_abs_delta);

	chop_bits--;
	
	for (; chop_bits >= 0; chop_bits--)
	{
		int i;
		for (i = 0; i < half_num_deltas; i++)
		{
			dest->slopes[i] -= (1 << chop_bits);
			if (dest->slopes[i] < 0)
			{
				dest->slopes[i] += (2 << chop_bits);
			}
			dest->slopes[i + half_num_deltas] = -dest->slopes[i];
			ranges_low[i] = (dest->slopes[i] - (1 << chop_bits)) < 0 ? 0 : (dest->slopes[i] - (1 << chop_bits));
			ranges_high[i] = dest->slopes[i] + (1 << chop_bits);
		}
		
		(void) do_binary_search_internal_(dest, in, sigma, num_deltas, chop_bits, ranges_low, ranges_high, max_abs_delta);
	}
	
	//fprintf(stderr, " max_abs_delta=%ld ", max_abs_delta);
	//fprintf(stderr, "slope0=%ld\n", dest->slopes[0]);
	free(best_slopes);
	free(ranges_low);
	free(ranges_high);
}

uint64_t
ssdpcm_encode_binary_search (ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma)
{
	sample_t max_abs_delta = 0;
	sample_t delta;
	size_t i;

	debug_assert(dest != NULL);
	debug_assert(in != NULL);
	debug_assert(sigma != NULL);
	
	// Find biggest difference between two successive samples
	for (i = 1; i < dest->length; i++)
	{
		delta = in[i] - in[i - 1];
		if (delta < 0)
		{
			delta = -delta;
		}
		if (delta > max_abs_delta)
		{
			max_abs_delta = delta;
		}
	}
	
	do_binary_search_(dest, in, sigma, dest->num_deltas, max_abs_delta);
	return ssdpcm_block_encode(dest, in, sigma);
}
