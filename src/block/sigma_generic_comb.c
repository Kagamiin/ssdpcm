/*
 * ssdpcm: implementation of the SSDPCM audio codec designed by Algorithm.
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

#include "ssdpcm_block_funcs.h"

sigma_tracker_methods sigma_generic_comb = &(const struct sigma_tracker_methods_s)
{
	&sigma_tracker_alloc,
	&sigma_tracker_init,
	&sigma_generic_comb_calc_error,
	&sigma_generic_comb_advance,
	&sigma_tracker_get_accumulated_error,
	&sigma_tracker_free
};

static inline uint64_t
calc_sigma_generic_comb_ (sigma_tracker_internal *state, sample_t predicted)
{
	sample_t diff;
	sample_t expected;
	uint64_t sigma;
	
	expected = state->input_buf[state->sigma_dec->index];
	if (state->sigma_dec->index > 0)
	{
		predicted += state->decode_buf[state->sigma_dec->index - 1];
		predicted /= 2;
		// Apply half-strength comb filter to the expected sample because it helps to reduce hiss
		expected *= 2;
		expected += state->input_buf[state->sigma_dec->index - 1];
		expected /= 3;
	}
	diff = predicted - expected;
	if (diff < 0)
	{
		diff = -diff;
	}
	
	sigma = (uint64_t)diff * diff;
	return sigma;
}

uint64_t
sigma_generic_comb_calc_error (void **ext_state, codeword_t delta)
{
	sigma_tracker_internal *state = *ext_state;
	sample_t predicted;
	
	predicted = calc_sample_(state->sigma_dec, delta);
	return calc_sigma_generic_comb_(state, predicted);
}

void
sigma_generic_comb_advance (void **ext_state)
{
	sigma_tracker_internal *state = *ext_state;
	sample_t result;
	uint64_t sigma;
	
	decode_one_sample_no_advance_(state->sigma_dec);
	result = state->decode_buf[state->sigma_dec->index];
	sigma = calc_sigma_generic_comb_(state, result);
	state->sigma_dec->index++;
	state->acc_error += sigma;
}

