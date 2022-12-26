
#include "ssdpcm_block_funcs.h"
#include <stdint.h>

sigma_tracker_methods sigma_u8_overflow = &(const struct sigma_tracker_methods_s)
{
	&sigma_tracker_alloc,
	&sigma_tracker_init,
	&sigma_u8_overflow_calc_error,
	&sigma_u8_overflow_advance,
	&sigma_tracker_get_accumulated_error,
	&sigma_tracker_free
};

static inline uint64_t
calc_sigma_u8_overflow_ (sigma_tracker_internal *state, sample_t predicted)
{
	sample_t diff;
	sample_t expected;
	uint64_t sigma;
	
	expected = state->input_buf[state->sigma_dec->index];
	diff = (predicted & 0xff) - (expected & 0xff);
	if (diff < 0)
	{
		diff = -diff;
	}
	if (predicted != (predicted & 0xff))
	{
		// Penalize overflow
		diff *= 4;
	}
	
	sigma = (uint64_t)diff * diff;
	return sigma;
}

uint64_t
sigma_u8_overflow_calc_error (void **ext_state, codeword_t delta)
{
	sigma_tracker_internal *state = *ext_state;
	sample_t predicted;
	
	predicted = calc_sample_(state->sigma_dec, delta);
	return calc_sigma_u8_overflow_(state, predicted);
}

void
sigma_u8_overflow_advance (void **ext_state)
{
	sigma_tracker_internal *state = *ext_state;
	sample_t result;
	uint64_t sigma;
	
	decode_one_sample_no_advance_(state->sigma_dec);
	result = state->decode_buf[state->sigma_dec->index];
	sigma = calc_sigma_u8_overflow_(state, result);
	state->sigma_dec->index++;
	state->acc_error += sigma;
}

