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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encode.h>
#include <sample.h>
#include <errors.h>
#include <errno.h>
#include <limits.h>
#include <wav.h>

void
exit_error (const char *msg, const char *error)
{
	if (error == NULL)
	{
		fprintf(stderr, "\nOof!\n%s\n", msg);
	}
	else
	{
		fprintf(stderr, "\nOof!\n%s: %s\n", msg, error);
	}
	exit(1);
}

void
write_block_params (FILE *dest, uint8_t initial_sample, uint8_t length)
{
	fprintf(dest, "initial_sample := %hhu\n", initial_sample);
	fprintf(dest, "length := %hhu\n", length);
}

static const char usage[] = "\
\033[97mUsage:\033[0m wav_simulator (num_deltas) [-c] infile.wav outfile.wav\n\
- Parameters\n\
  - \033[96mnum_deltas\033[0m - Selects the number of deltas from 2 to 9;\n\
    for odd numbers, the last delta is always 0\n\
  - \033[96m-c\033[0m - Enables comb filtering (most useful for 1-bit SSDPCM)\n\
- \033[96minfile.wav\033[0m should be an 8-bit unsigned PCM or 16-bit signed.\n\
  PCM WAV file.\n\
- \033[96moutfile.wav\033[0m will be the same format and sample rate as the.\n\
  input file.\n";

err_t put_bits_msbfirst (bitstream_buffer *buf, codeword_t src, uint8_t num_bits);

#define SAMPLES_PER_BLOCK 128

int
main (int argc, char **argv)
{
	void *byte_buffer;
	sample_t sample_buffer[SAMPLES_PER_BLOCK];
	codeword_t delta_buffer[SAMPLES_PER_BLOCK];
	sample_t slopes[16];
	long read_data;
	wav_handle *infile;
	wav_handle *outfile;
	char *infile_name;
	char *outfile_name;
	wav_sample_fmt format;
	uint32_t sample_rate;
	size_t block_count = 0;
	long block_length = SAMPLES_PER_BLOCK;
	sigma_tracker sigma;
	ssdpcm_block block;
	sample_t temp_last_sample;
	err_t err;
	//int bits_per_sample;
	bool comb_filter = false;
	
	byte_buffer = malloc(SAMPLES_PER_BLOCK * 2);

	block.deltas = delta_buffer;
	block.slopes = slopes;
	
	if (argc < 4 || argc > 5)
	{
		exit_error(usage, NULL);
	}
	
#if 0
	if (!strcmp("1", argv[1]))
	{
		bits_per_sample = 1;
	}
	else if (!strcmp("2", argv[1]))
	{
		bits_per_sample = 2;
	}
	else if (!strcmp("3", argv[1]))
	{
		bits_per_sample = 3;
	}
	else
	{
		bits_per_sample = 0;
		exit_error(usage, NULL);
	}
#endif
	
	if (sscanf(argv[1], "%hhd", &block.num_deltas) != 1)
	{
		exit_error(usage, NULL);
	}
	if (block.num_deltas < 2 || block.num_deltas > 9)
	{
		exit_error("Error: Number of deltas must be between 2 and 9 inclusive", NULL);
	}
	
	if (argc == 5)
	{
		infile_name = argv[3];
		outfile_name = argv[4];
		if (!strcmp("-c", argv[2]))
		{
			comb_filter = true;
		}
		else
		{
			exit_error(usage, NULL);
		}
	}
	else
	{
		infile_name = argv[2];
		outfile_name = argv[3];
	}
	
	if (block.num_deltas < 4)
	{
		block_length /= 2;
	}
	
	//block.num_deltas = 1 << bits_per_sample;
	block.length = block_length;
	
	infile = calloc(1, sizeof(wav_handle));
	outfile = calloc(1, sizeof(wav_handle));
	
	infile = wav_open(infile, infile_name, W_READ, &err);
	if (infile == NULL)
	{
		char err_msg[256];
		snprintf(err_msg, 256, "Could not open input file '%s' (%s). errno", infile_name, error_enum_strs[err]);
		exit_error(err_msg, strerror(errno));
	}
	
	format = wav_get_format(infile, &err);
	switch (format)
	{
	case W_U8:
		if (comb_filter)
		{
			sigma.methods = sigma_u8_overflow_comb;
		}
		else
		{
			sigma.methods = sigma_u8_overflow;
		}
		break;
	case W_S16LE:
		if (comb_filter)
		{
			sigma.methods = sigma_generic_comb;
		}
		else
		{
			sigma.methods = sigma_generic;
		}
		break;
	default:
		exit_error("Input file has unrecognized format/codec - only 8/16-bit PCM is allowed", NULL);
		break;
	}
	
	sigma.methods->alloc(&sigma.state);
	
	sample_rate = wav_get_sample_rate(infile);
	
	outfile = wav_open(outfile, outfile_name, W_CREATE, &err);
	if (outfile == NULL)
	{
		char err_msg[256];
		snprintf(err_msg, 256, "Could not open output file '%s' (%s). errno", outfile_name, error_enum_strs[err]);
		exit_error(err_msg, strerror(errno));
	}
	wav_set_format(outfile, format);
	wav_set_sample_rate(outfile, sample_rate);
	wav_write_header(outfile);
	wav_seek(infile, 0, SEEK_SET);
	wav_seek(outfile, 0, SEEK_SET);
	
	read_data = wav_read(infile, byte_buffer, block_length, &err);
	if (err != E_OK)
	{
		char err_msg[256];
		snprintf(err_msg, 256, "Read error (%s)", error_enum_strs[err]);
		exit_error(err_msg, strerror(errno));
	}
	switch (format)
	{
	case W_U8:
		block.initial_sample = ((uint8_t *)byte_buffer)[0];
		break;
	case W_S16LE:
		block.initial_sample = ((int16_t *)byte_buffer)[0];
		break;
	default:
		// unreachable
		break;
	}

	while (read_data == block_length)
	{
		switch (format)
		{
		case W_U8:
			sample_decode_u8(sample_buffer, (uint8_t *)byte_buffer, block_length);
			break;
		case W_S16LE:
			sample_decode_s16(sample_buffer, (int16_t *)byte_buffer, block_length);
			break;
		default:
			// unreachable
			break;
		}
		
		fprintf(stderr, "\rEncoding block %lu...", block_count);
		(void) ssdpcm_encode_binary_search(&block, sample_buffer, &sigma);
		ssdpcm_block_decode(sample_buffer, &block);
		temp_last_sample = sample_buffer[block_length - 1];
		if (comb_filter)
		{
			sample_filter_comb(sample_buffer, block_length, block.initial_sample);
		}
		block.initial_sample = temp_last_sample;
		
		switch (format)
		{
		case W_U8:
			sample_encode_u8_overflow((uint8_t *)byte_buffer, sample_buffer, block_length);
			break;
		case W_S16LE:
			sample_encode_s16((int16_t *)byte_buffer, sample_buffer, block_length);
			break;
		default:
			// unreachable
			break;
		}
		
		wav_write(outfile, byte_buffer, block_length, -1, &err);
		if (err != E_OK)
		{
			char err_msg[256];
			int errno_copy = errno;
			snprintf(err_msg, 256, "Write error (%s)", error_enum_strs[err]);
			// Try to properly close the WAV file anyway
			wav_close(outfile, &err);
			exit_error(err_msg, strerror(errno_copy));
		}
		
		read_data = wav_read(infile, byte_buffer, block_length, &err);
		if (err != E_OK)
		{
			if (err == E_END_OF_STREAM)
			{
				break;
			}
			char err_msg[256];
			int errno_copy = errno;
			snprintf(err_msg, 256, "Read error (%s)", error_enum_strs[err]);
			// Try to properly close the WAV file anyway
			wav_close(outfile, &err);
			exit_error(err_msg, strerror(errno_copy));
		}
		block_count++;
	}
	
	wav_close(infile, &err);
	wav_close(outfile, &err);
	
	fprintf(stderr, "\nDone.\n");
	sigma.methods->free(&(sigma.state));
	free(infile);
	free(outfile);
	free(byte_buffer);
	
	return 0;
}
