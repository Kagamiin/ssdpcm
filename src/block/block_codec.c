
#include "ssdpcm_block_funcs.h"

/* ------------------------------------------------------------------------- */
inline sample_t
calc_sample_ (ssdpcm_block_iterator *dec, codeword_t slope_code)
{
	sample_t slope = dec->block->slopes[slope_code];
	return dec->sample_state + slope;
}

/* ------------------------------------------------------------------------- */
inline void
decode_one_sample_no_advance_ (ssdpcm_block_iterator *dec)
{
	codeword_t slope_code;
	
	debug_assert(dec->index < dec->block->length);
	slope_code = dec->block->deltas[dec->index];
	dec->sample_state = calc_sample_(dec, slope_code);
	dec->raw[dec->index] = dec->sample_state;
}

/* ------------------------------------------------------------------------- */
inline void
decode_one_sample_ (ssdpcm_block_iterator *dec)
{
	decode_one_sample_no_advance_(dec);
	dec->index++;
}

/* ------------------------------------------------------------------------- */
inline void
enqueue_one_codeword_ (ssdpcm_block_iterator *dest, codeword_t c)
{
	debug_assert(dest->index < dest->block->length);
	dest->block->deltas[dest->index] = c;
	dest->index++;
}

/* ------------------------------------------------------------------------- */
inline void
block_iterator_init_ (ssdpcm_block_iterator *iter, ssdpcm_block *block, sample_t *raw)
{
	iter->block = block;
	iter->raw = raw;
	iter->index = 0;
	iter->sample_state = block->initial_sample;
}

/* ------------------------------------------------------------------------- */
// ssdpcm_block_decode: Decodes an SSDPCM block into a sample buffer.
void
ssdpcm_block_decode (sample_t *out, ssdpcm_block *block)
{
	ssdpcm_block_iterator dec;
	block_iterator_init_(&dec, block, out);
	while (dec.index < dec.block->length)
	{
		decode_one_sample_(&dec);
	}
}

/* ------------------------------------------------------------------------- */
inline codeword_t
find_best_delta_ (ssdpcm_encoder *enc)
{
	codeword_t c;
	codeword_t best;
	uint64_t best_error = UINT64_MAX;
	ssdpcm_block *block = enc->iter.block;
	for (c = 0, best = 0; c < block->num_deltas; c++)
	{
		uint64_t error = enc->sigma->methods->calc_error(&enc->sigma->state, c);
		if (error < best_error)
		{
			best_error = error;
			best = c;
		}
	}
	return best;
}

/* ------------------------------------------------------------------------- */
// ssdpcm_block_encode: Encodes an SSDPCM block (given preinitialized fields),
// and given as input:
// - A sample buffer
// - An error tracker (with preallocated state).
// Returns the accumulated error metric.
uint64_t
ssdpcm_block_encode (ssdpcm_block *block, sample_t *in, sigma_tracker *sigma)
{
	uint64_t error_metric;
	ssdpcm_encoder enc;
	block_iterator_init_(&enc.iter, block, in);
	enc.sigma = sigma;
	enc.sigma->methods->init(&enc.sigma->state, &enc.iter);
	while (enc.iter.index < enc.iter.block->length)
	{
		codeword_t best_delta = find_best_delta_(&enc);
		enqueue_one_codeword_(&enc.iter, best_delta);
		enc.sigma->methods->advance(&enc.sigma->state);
	}
	error_metric = enc.sigma->methods->get_accumulated_error(&enc.sigma->state);
	return error_metric;
}
