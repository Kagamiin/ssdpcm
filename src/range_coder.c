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
			for (i = 2; i >= 0; i--)
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


void
range_encode_ss3(codeword_t *input, uint8_t *output, size_t num_words)
{
	debug_assert(input != NULL);
	debug_assert(output != NULL);
	debug_assert(num_words != 0);
	size_t i, byte, num, total = 0;
	while (total < num_words)
	{
		uint8_t packed_nums[4];
		for (num = 0; num < 4; num++)
		{
			packed_nums[num] = 0;
			i = 0;
			for (; i < 2 && total < num_words; i++, total++)
			{
				packed_nums[num] *= 8;
				packed_nums[num] += (*input % 8);
				input++;
			}
			for (; i < 2; i++)
			{
				// If misaligned, pad result with highest digits
				packed_nums[num] *= 8;
				packed_nums[num] += 7;
			}
		}
		for (byte = 0; byte < 3; byte++)
		{
			// Pack last bits of num 3 into other 3 nums, in reverse order
			packed_nums[byte] <<= 2;
			packed_nums[byte] |= packed_nums[3] & 0x03;
			*output = packed_nums[byte];
			output++;
			packed_nums[3] >>= 2;
		}
	}
}

void
range_decode_ss3(uint8_t *input, codeword_t *output, size_t num_bytes)
{
	debug_assert(input != NULL);
	debug_assert(output != NULL);
	debug_assert(num_bytes != 0);
	size_t j, total = 0;
	int i;
	while (total < num_bytes)
	{
		uint8_t num_3 = 0;
		codeword_t words[2];
		for (j = 0; j < 3 && total < num_bytes; j++)
		{
			uint8_t byte = *input;
			
			num_3 >>= 2;
			num_3 |= (byte & 0x03) << 6;
			byte >>= 2;
			
			for (i = 1; i >= 0; i--)
			{
				words[i] = byte % 8;
				byte /= 8;
			}
			for (i = 0; i < 2; i++)
			{
				*output = words[i];
				output++;
			}
			
			input++;
			total++;
		}
		if (j == 3)
		{
			num_3 >>= 2;
			for (i = 1; i >= 0; i--)
			{
				words[i] = num_3 % 8;
				num_3 /= 8;
			}
			for (i = 0; i < 2; i++)
			{
				*output = words[i];
				output++;
			}
		}
	}
}
