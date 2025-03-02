
#include <stdio.h>
#include <types.h>
#include <errors.h>

void
sample_decode_s16 (sample_t *dest, int16_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = src[i];
	}
}

void
sample_encode_s16 (int16_t *dest, sample_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		sample_t value = src[i];
		if (value > INT16_MAX)
		{
			value = INT16_MAX;
		}
		if (value < INT16_MIN)
		{
			value = INT16_MIN;
		}
		dest[i] = value;
	}
}

void
sample_decode_u16 (sample_t *dest, uint16_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = src[i];
	}
}

void
sample_decode_s16_multichannel (sample_t **dest, int16_t *src, size_t num_samples, size_t num_channels)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	size_t c = 0;
	for (i = 0; i < num_samples * num_channels; i++)
	{
		dest[c][i / num_channels] = src[i];
		c++;
		c %= num_channels;
	}
}

void
sample_encode_s16_multichannel (int16_t *dest, sample_t **src, size_t num_samples, size_t num_channels)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	size_t c = 0;
	for (i = 0; i < num_samples * num_channels; i++)
	{
		sample_t value = src[c][i / num_channels];
		if (value > INT16_MAX)
		{
			value = INT16_MAX;
		}
		if (value < INT16_MIN)
		{
			value = INT16_MIN;
		}
		dest[i] = value;
		c++;
		c %= num_channels;
	}
}

void
sample_encode_u16 (uint16_t *dest, sample_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		sample_t value = src[i];
		if (value > UINT16_MAX)
		{
			fprintf(stderr, "Clamped rogue sample from 0x%x to 0x%x\n", (unsigned)value, UINT16_MAX);
			value = UINT16_MAX;
		}
		if (value < 0)
		{
			fprintf(stderr, "Clamped rogue sample from 0x%x to 0x0000\n", (unsigned)value);
			value = 0;
		}
		dest[i] = value;
	}
}

void
sample_decode_u8 (sample_t *dest, uint8_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = src[i];
	}
}

void
sample_encode_u8_overflow (uint8_t *dest, sample_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = src[i] & 0xFF;
	}
}

void
sample_decode_u8_multichannel (sample_t **dest, uint8_t *src, size_t num_samples, size_t num_channels)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	size_t c = 0;
	for (i = 0; i < num_samples * num_channels; i++)
	{
		dest[c][i / num_channels] = src[i];
		c++;
		c %= num_channels;
	}
}

void
sample_encode_u8_overflow_multichannel (uint8_t *dest, sample_t **src, size_t num_samples, size_t num_channels)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	size_t c = 0;
	for (i = 0; i < num_samples * num_channels; i++)
	{
		dest[i] = src[c][i / num_channels] & 0xFF;
		c++;
		c %= num_channels;
	}
}

void
sample_encode_u8_clamp (uint8_t *dest, sample_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		sample_t value = src[i];
		if (value > UINT8_MAX)
		{
			value = UINT8_MAX;
		}
		if (value < 0)
		{
			value = 0;
		}
		dest[i] = value;
	}
}

void
sample_convert_u8_to_s16 (int16_t *dest, uint8_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		int16_t value = (int16_t)src[i] - 128;
		
		dest[i] = (value << 8) + (value & 0x7f << 1) + (value & 0x7f >> 6);
	}
}
// TODO: proper tests
verify(UINT8_MAX - 128 == INT8_MAX);
verify((INT8_MAX << 8) + (INT8_MAX << 1) + (INT8_MAX >> 6) == INT16_MAX);
//verify((INT8_MIN << 8) + ((INT8_MIN & 0x7F) << 1) + ((INT8_MIN & 0x7F) >> 6) == INT16_MIN);

void
sample_convert_s16_to_u8 (uint8_t *dest, int16_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = (src[i] >> 8) + 128;
	}
}
// TODO: proper tests
verify((INT16_MAX >> 8) + 128 == UINT8_MAX);
verify((INT16_MIN >> 8) + 128 == 0);

void
sample_convert_u8_to_u7 (uint8_t *dest, uint8_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = src[i] >> 1;
	}
}

void
sample_convert_u7_to_u8 (uint8_t *dest, uint8_t *src, size_t num_samples)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	size_t i;
	for (i = 0; i < num_samples; i++)
	{
		dest[i] = src[i] << 1;
	}
}

typedef uint32_t rng32_state;

static uint32_t rng_xorshift (rng32_state *rng)
{	
	*rng ^= *rng << 13;
	*rng ^= *rng >> 17;
	*rng ^= *rng << 5;
	return *rng;
}

// Constant starting seed
static rng32_state rng = 1835364215;

void
sample_dither_triangular (sample_t **dest, sample_t **src, size_t num_samples, size_t num_channels, uint8_t strength, sample_t clamp_low, sample_t clamp_high)
{
	debug_assert(dest != NULL);
	debug_assert(src != NULL);
	debug_assert(clamp_low < clamp_high);
	size_t i;
	for (i = 0; i < num_samples * num_channels; i++)
	{
		size_t sample_idx = i / num_channels;
		size_t c = 0;
		int64_t extended_sample = (int64_t)(src[c][sample_idx]) << 32;
		int64_t dithered_extended_sample;
		sample_t output_sample;
		int64_t dither_noise = 0;
		
		dither_noise += (int64_t)(rng_xorshift(&rng)) - (int64_t)(1 << 31);
		dither_noise += (int64_t)(rng_xorshift(&rng)) - (int64_t)(1 << 31);
		//dither_noise /= 2;
		dither_noise *= strength + 1;
		dither_noise += 1 << 31;
		
		dithered_extended_sample = extended_sample + dither_noise;
		
		output_sample = (sample_t)(dithered_extended_sample >> 32);
		
		dest[c][sample_idx] = output_sample;
		if (dest[c][sample_idx] > clamp_high) {
			dest[c][sample_idx] = clamp_high;
		}
		if (dest[c][sample_idx] < clamp_low) {
			dest[c][sample_idx] = clamp_low;
		}
		
		c++;
		c %= num_channels;
	}
}
