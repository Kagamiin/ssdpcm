
#include <stdint.h>
#include <types.h>
#include <encode.h>
#include <errors.h>
#include <string.h>
#include <stdio.h>

static inline void
do_bruteforce_search_ (
	ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma, uint8_t num_deltas, sample_t max_abs_delta)
{
	int i;
	sample_t *best_slopes;
	uint64_t best_metric = UINT64_MAX;
	uint8_t half_num_deltas = num_deltas / 2;
	
	best_slopes = calloc(num_deltas, sizeof(sample_t));
	
	for (i = 0; i < half_num_deltas; i++)
	{
		dest->slopes[i] = half_num_deltas - i - 1;
		best_slopes[i] = half_num_deltas - i - 1;
		dest->slopes[i + half_num_deltas] = -dest->slopes[i];
		best_slopes[i + half_num_deltas] = -dest->slopes[i];
	}
	
	while (dest->slopes[0] <= max_abs_delta)
	{
		uint64_t sigma_metric = ssdpcm_block_encode(dest, in, sigma);
		if (sigma_metric < best_metric)
		{
			best_metric = sigma_metric;
			memcpy(best_slopes, dest->slopes, half_num_deltas * 2 * sizeof(sample_t));
		}
		
		for (i = half_num_deltas - 1; i >= 0; i--)
		{
			dest->slopes[i]++;
			if (i > 0 && dest->slopes[i] >= dest->slopes[i - 1])
			{
				dest->slopes[i] = 0;
				dest->slopes[i + half_num_deltas] = 0;
			}
			else
			{
				dest->slopes[i + half_num_deltas] = -dest->slopes[i];
				break;
			}
		}
	}
	
	memcpy(dest->slopes, best_slopes, num_deltas * sizeof(sample_t));
	
	//fprintf(stderr, " max_abs_delta=%ld ", max_abs_delta);
	//fprintf(stderr, "slope0=%ld\n", dest->slopes[0]);
	free(best_slopes);
}

uint64_t
ssdpcm_encode_bruteforce (ssdpcm_block *dest, sample_t *in, sigma_tracker *sigma)
{
	sample_t max_abs_delta = 0;
	sample_t delta;
	size_t i;

	// One'd have to be insane to expect a brute force algorithm to iterate through more than a couple different deltas in admissible time.
	assert(dest->num_deltas <= 8 || !"refusing to search through this many deltas");
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
	
	do_bruteforce_search_(dest, in, sigma, dest->num_deltas, max_abs_delta);
	return ssdpcm_block_encode(dest, in, sigma);
}
