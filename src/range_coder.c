#include "types.h"
#include "errors.h"

void
range_encode_ss1_6(codeword_t *input, uint8_t *output, size_t num_words)
{
	debug_assert(input != NULL);
	debug_assert(output != NULL);
	debug_assert(num_words != 0);
	size_t i, total = 0;
	while (total < num_words)
	{
		uint8_t result = 0;
		for (i = 0; i < 5 && total < num_words; i++, total++)
		{
			result *= 3;
			result += (*input % 3);
			input++;
		}
		for (; i < 5; i++)
		{
			// If misaligned, pad result with highest digits
			result *= 3;
			result += 2;
		}
		*output = result;
		output++;
	}
}

void
range_decode_ss1_6(uint8_t *input, codeword_t *output, size_t num_bytes)
{
	debug_assert(input != NULL);
	debug_assert(output != NULL);
	debug_assert(num_bytes != 0);
	size_t total = 0;
	int i;
	while (total < num_bytes)
	{
		uint8_t byte = *input;
		codeword_t words[5];
		for (i = 4; i >= 0; i--)
		{
			words[i] = byte % 3;
			byte /= 3;
		}
		for (i = 0; i < 5; i++)
		{
			*output = words[i];
			output++;
		}
		input++;
		total++;
	}
}


void
range_encode_ss2_3(codeword_t *input, uint8_t *output, size_t num_words)
{
	debug_assert(input != NULL);
	debug_assert(output != NULL);
	debug_assert(num_words != 0);
	size_t i, byte, num, total = 0;
	while (total < num_words)
	{
		// HACK: doubling the size of the array to circumvent buggy -Werror=stringop-overflow=
		uint8_t packed_nums[16];
		for (num = 0; num < 8; num++)
		{
			packed_nums[num] = 0;
			i = 0;
			for (; i < 3 && total < num_words; i++, total++)
			{
				packed_nums[num] *= 5;
				packed_nums[num] += (*input % 5);
				input++;
			}
			for (; i < 3; i++)
			{
				// If misaligned, pad result with highest digits
				packed_nums[num] *= 5;
				packed_nums[num] += 4;
			}
		}
		for (byte = 0; byte < 7; byte++)
		{
			// Pack last bits of num 7 into other 7 nums, in reverse order
			packed_nums[byte] <<= 1;
			packed_nums[byte] |= packed_nums[7] & 0x01;
			*output = packed_nums[byte];
			output++;
			packed_nums[7] >>= 1;
		}
	}
}

void
range_decode_ss2_3(uint8_t *input, codeword_t *output, size_t num_bytes)
{
	debug_assert(input != NULL);
	debug_assert(output != NULL);
	debug_assert(num_bytes != 0);
	size_t j, total = 0;
	int i;
	while (total < num_bytes)
	{
		uint8_t num_7 = 0;
		codeword_t words[3];
		for (j = 0; j < 7 && total < num_bytes; j++)
		{
			uint8_t byte = *input;
			
			num_7 >>= 1;
			num_7 |= (byte & 0x01) << 7;
			byte >>= 1;
			
			for (i = 2; i >= 0; i--)
			{
				words[i] = byte % 5;
				byte /= 5;
			}
			for (i = 0; i < 3; i++)
			{
				*output = words[i];
				output++;
			}
			
			input++;
			total++;
		}
		if (j == 7)
		{
			num_7 >>= 1;
			for (i = 0; i < 3; i++)
			{
				words[i] = num_7 % 5;
				num_7 /= 5;
			}
			for (i = 0; i < 3; i++)
			{
				*output = words[i];
				output++;
			}
		}
	}
}
