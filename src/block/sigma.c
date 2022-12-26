
#include "ssdpcm_block_funcs.h"
#include <string.h>

#define INITIAL_BUF_SIZE 1024

/* ------------------------------------------------------------------------- */
void
sigma_tracker_alloc (void **ext_state)
{
	sigma_tracker_internal *state;
	
	state = malloc(sizeof(sigma_tracker_internal));
	assert(state != NULL);
	
	state->input_buf = NULL;
	state->decode_bufsize = INITIAL_BUF_SIZE;
	state->decode_buf = malloc(state->decode_bufsize * sizeof(sample_t));
	state->sigma_dec = malloc(sizeof(ssdpcm_block_iterator));
	assert(state->decode_buf != NULL);
	assert(state->sigma_dec != NULL);
	
	*ext_state = state;
}

/* ------------------------------------------------------------------------- */
void
sigma_tracker_init (void **ext_state, ssdpcm_block_iterator *iter)
{
	debug_assert(ext_state != NULL);
	debug_assert(*ext_state != NULL);
	sigma_tracker_internal *state = *ext_state;
	ssdpcm_block_iterator *sigma_dec = state->sigma_dec;
	
	state->input_buf = iter->raw;
	state->acc_error = 0;
	sigma_dec->block = iter->block;
	sigma_dec->raw = state->decode_buf;
	sigma_dec->sample_state = iter->sample_state;
	sigma_dec->index = iter->index;
	
	if (sigma_dec->block->length > state->decode_bufsize)
	{
		state->decode_bufsize = sigma_dec->block->length;
		state->decode_buf = realloc(state->decode_buf, sizeof(sample_t) * state->decode_bufsize);
		assert(state->decode_buf != NULL);
		*ext_state = state;
	}
}

/* ------------------------------------------------------------------------- */
uint64_t
sigma_tracker_get_accumulated_error (void **ext_state)
{
	debug_assert(ext_state != NULL);
	debug_assert(*ext_state != NULL);
	sigma_tracker_internal *state = *ext_state;
	debug_assert(state->input_buf != NULL);
	
	return state->acc_error;
}

/* ------------------------------------------------------------------------- */
void
sigma_tracker_free (void **ext_state)
{
	debug_assert(ext_state != NULL);
	debug_assert(*ext_state != NULL);
	sigma_tracker_internal *state = *ext_state;
	
	free(state->sigma_dec);
	free(state->decode_buf);
	free(state);
	*ext_state = NULL;
}
