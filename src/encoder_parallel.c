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
#include <bit_pack_unpack.h>
#include <range_coder.h>
#include <wav.h>
#include <omp.h>

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

static const char usage[] = "\
\033[97mUsage:\033[0m encoder_parallel (mode) infile.wav outfile.aud\n\
This encoder takes advantage of multithreading to accelerate encoding of the\n\
higher quality modes, such as ss2, ss2.3 and ss3. For lower quality modes,\n\
usage of the normal encoder is recommended.\n\
- Parameters\n\
  - \033[96mmode\033[0m - Selects the encoding mode; the following modes are\n\
    supported (in increasing order of bitrate):\n\
    - \033[96mss1\033[0m    - 1-bit SSDPCM\n\
    - \033[96mss1c\033[0m   - 1-bit SSDPCM with comb filtering\n\
    - \033[96mss1.6\033[0m  - 1.6-bit SSDPCM\n\
    - \033[96mss2\033[0m    - 2-bit SSDPCM\n\
    - \033[96mss2.3\033[0m  - 2.3-bit SSDPCM\n\
    - \033[96mss3\033[0m  - 3-bit SSDPCM\n\
    - \033[96mdecode\033[0m - decodes an encoded input file\n\
- \033[96minfile.wav\033[0m should be an 8-bit unsigned PCM or 16-bit signed\n\
  PCM WAV file for the encoding modes, or an encoded .aud SSDPCM file for the\n\
  decode mode.\n\
- \033[96moutfile.aud\033[0m is the path for the encoded output file, or the\n\
  decoded WAV file in the case of the decode mode.";



int
main (int argc, char **argv)
{
	// Global read-write variables (locked)
	wav_handle *infile;
	wav_handle *outfile;
	
	// use pragma atomic capture to read and increment
	size_t block_count = 0;
	
	// Global read-only variables (during the encode loop)
	char *infile_name;
	char *outfile_name;
	wav_sample_fmt format;
	ssdpcm_block_mode mode;
	uint32_t sample_rate;
	size_t code_buffer_size;
	long block_length;
	int num_deltas;
	sigma_tracker_methods sigma_methods = NULL;
	int num_threads;
	bool stereo = false;
	bool comb_filter = false;
	bool decode_mode = false;
	bool has_reference_sample_on_every_block = false;
	
	// Thread-local variables

	err_t err;
	
	if (argc != 4)
	{
		exit_error(usage, NULL);
	}

	if (!strcmp("ss1", argv[1]))
	{
		mode = SS_SS1;
		num_deltas = 2;
		block_length = 64;
	}
	else if (!strcmp("ss1c", argv[1]))
	{
		mode = SS_SS1C;
		num_deltas = 2;
		block_length = 64;
		comb_filter = true;
	}
	else if (!strcmp("ss1.6", argv[1]))
	{
		mode = SS_SS1_6;
		num_deltas = 3;
		block_length = 65;
	}
	else if (!strcmp("ss2", argv[1]))
	{
		mode = SS_SS2;
		num_deltas = 4;
		block_length = 128;
	}
	else if (!strcmp("ss2.3", argv[1]))
	{
		mode = SS_SS2_3;
		num_deltas = 5;
		block_length = 120;
	}
	else if (!strcmp("ss3", argv[1]))
	{
		mode = SS_SS3;
		num_deltas = 8;
		block_length = 120;
	}
	else if (!strcmp("decode", argv[1]))
	{
		decode_mode = true;
	}
	else
	{
		num_deltas = 0;
		exit_error(usage, NULL);
	}
	
	infile_name = argv[2];
	outfile_name = argv[3];
	
	infile = wav_alloc(&err);
	outfile = wav_alloc(&err);
	
	infile = wav_open(infile, infile_name, W_READ, &err);
	if (infile == NULL)
	{
		char err_msg[256];
		snprintf(err_msg, 256, "Could not open input file (%s). errno", error_enum_strs[err]);
		exit_error(err_msg, strerror(errno));
	}
	
	if (decode_mode)
	{
		format = wav_get_ssdpcm_output_format(infile, &err);
		if (format < 0)
		{
			if (err == E_NOT_A_SSDPCM_WAV)
			{
				exit_error("Input file is not SSDPCM - cannot decode!", NULL);
			}
			exit_error("Error while parsing file format", error_enum_strs[err]);
		}
		mode = wav_get_ssdpcm_mode(infile, &err);
		block_length = wav_get_ssdpcm_block_length(infile, &err);
		num_deltas = wav_get_ssdpcm_num_slopes(infile, &err);
		int num_channels = wav_get_num_channels(infile, &err);
		if (num_channels == 2)
		{
			stereo = true;
		}
		if (num_channels > 2)
		{
			exit_error("Input file has more than 2 channels - only mono or stereo is supported", NULL);
		}
		comb_filter = (mode == SS_SS1C);
		code_buffer_size = wav_get_ssdpcm_code_bytes_per_block(infile, &err);
	}
	else
	{
		format = wav_get_format(infile, &err);
		int num_channels = wav_get_num_channels(infile, &err);
		if (num_channels == 2)
		{
			stereo = true;
		}
		if (num_channels > 2)
		{
			exit_error("Input file has more than 2 channels - only mono or stereo is supported", NULL);
		}
	}
	
	switch (format)
	{
	case W_U8:
		if (comb_filter)
		{
			sigma_methods = sigma_u8_overflow_comb;
		}
		else
		{
			sigma_methods = sigma_u8_overflow;
		}
		break;
	case W_S16LE:
		sigma_methods = sigma_generic;
		break;
	case W_SSDPCM:
		exit_error("Input file appears to be SSDPCM, not uncompressed WAV - please use \"decode\" option", NULL);
		break;
	default:
		exit_error("Input file has unrecognized format/codec - only 8/16-bit PCM is allowed", NULL);
		break;
	}
	
	outfile = wav_open(outfile, outfile_name, W_CREATE, &err);
	if (outfile == NULL)
	{
		char err_msg[256];
		snprintf(err_msg, 256, "Could not open output file (%s). errno", error_enum_strs[err]);
		exit_error(err_msg, strerror(errno));
	}
	sample_rate = wav_get_sample_rate(infile);
	wav_set_sample_rate(outfile, sample_rate);
	wav_set_num_channels(outfile, stereo + 1);
	
	if (decode_mode)
	{
		wav_set_format(outfile, format);
		has_reference_sample_on_every_block = wav_ssdpcm_has_reference_sample_on_every_block(infile, &err);
	}
	else
	{
		wav_init_ssdpcm(outfile, format, mode, block_length, true);
		code_buffer_size = wav_get_ssdpcm_code_bytes_per_block(outfile, &err);
	}
	
	wav_write_header(outfile);
	wav_seek(infile, 0, SEEK_SET);
	wav_seek(outfile, 0, SEEK_SET);
	

	// HACK: Decode serially, because it's faster lol
	if (decode_mode || has_reference_sample_on_every_block)
	{
		omp_set_num_threads(1);
	}
	
#pragma omp parallel firstprivate(err)
	{
		void *code_buffer[2] = {NULL, NULL};
		void *sample_conv_buffer = NULL;
		bitstream_buffer bitpacker;
		sample_t *sample_buffer[2] = {NULL, NULL};
		codeword_t *delta_buffer[2] = {NULL, NULL};
		sample_t slopes[2][16];
		long read_data = 0;
		ssdpcm_block block[2];
		size_t block_index = 0;
		sigma_tracker sigma;
		int n;
		
		sigma.methods = sigma_methods;
		sigma.methods->alloc(&sigma.state);

		for (n = 0; n <= stereo; n++)
		{
			memset(slopes, 0, sizeof(sample_t) * 16);
		}

		if (decode_mode)
		{
			sample_conv_buffer = malloc(wav_get_sizeof(outfile, block_length * (stereo + 1)));
		}
		else
		{
			sample_conv_buffer = malloc(wav_get_sizeof(infile, block_length * (stereo + 1)));
			if (omp_get_thread_num() == 0)
			{
				num_threads = omp_get_num_threads();
				fprintf(stderr, "\rEncoding in parallel with %d threads.\n", num_threads);
			}
		}
		for (n = 0; n <= stereo; n++)
		{
			sample_buffer[n] = malloc(sizeof(sample_t) * block_length);
			delta_buffer[n] = malloc(sizeof(codeword_t) * block_length);
			code_buffer[n] = malloc(code_buffer_size);
			block[n].deltas = delta_buffer[n];
			block[n].slopes = slopes[n];
			block[n].length = block_length;
			block[n].num_deltas = num_deltas;
		}

		memset(&bitpacker, 0, sizeof(bitstream_buffer));
		bitpacker.byte_buf.buffer_size = code_buffer_size;

		while (1 || (err == E_OK && ((decode_mode) || (read_data == block_length))))
		{
			
			uint8_t initial_sample_temp[4];
			if (!decode_mode)
			{
#pragma omp critical
				{
#pragma omp atomic capture
					{ block_index = block_count; block_count++; }
					read_data = wav_read(infile, sample_conv_buffer, block_length, &err);
				}
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
#pragma omp critical
					wav_close(outfile, &err);
					exit_error(err_msg, strerror(errno_copy));
				}
				
				memcpy(initial_sample_temp, sample_conv_buffer, sizeof(initial_sample_temp));
				switch (format)
				{
				case W_U8:
					sample_decode_u8_multichannel(sample_buffer, (uint8_t *)sample_conv_buffer, block_length, stereo + 1);
					for (n = 0; n <= stereo; n++)
					{
						block[n].initial_sample = initial_sample_temp[n];
					}
					break;
				case W_S16LE:
					sample_decode_s16_multichannel(sample_buffer, (int16_t *)sample_conv_buffer, block_length, stereo + 1);
					for (n = 0; n <= stereo; n++)
					{
						block[n].initial_sample = ((int16_t *)initial_sample_temp)[n];
					}
					break;
				default:
					// unreachable
					break;
				}
				
				

#pragma omp critical
				fprintf(stderr, "\rEncoding block %lu...", block_index);
				for (n = 0; n <= stereo; n++)
				{
					(void) ssdpcm_encode_binary_search(&block[n], sample_buffer[n], &sigma);
					ssdpcm_block_decode(sample_buffer[n], &block[n]);
					if (comb_filter)
					{
						sample_filter_comb(sample_buffer[n], block_length, block[n].initial_sample);
					}
					
					
					bitpacker.byte_buf.buffer = code_buffer[n];
					bitpacker.byte_buf.offset = 0;
					bitpacker.bit_index = 0;
					memset(code_buffer[n], 0, code_buffer_size);
					switch (mode)
					{
					case SS_SS1:
					case SS_SS1C:
					{
						int i;
						for (i = 0; i < block_length; i++)
						{
							err = put_bits_msbfirst(&bitpacker, block[n].deltas[i], 1);
							if (err != E_OK)
							{
								exit_error("Runtime error: put_bits_msbfirst returned non-ok status", error_enum_strs[err]);
							}
						}
						break;
					}
					case SS_SS2:
					{
						int i;
						for (i = 0; i < block_length; i++)
						{
							err = put_bits_msbfirst(&bitpacker, block[n].deltas[i], 2);
							if (err != E_OK)
							{
								exit_error("Runtime error: put_bits_msbfirst returned non-ok status", error_enum_strs[err]);
							}
						}
						break;
					}
					case SS_SS1_6:
						range_encode_ss1_6(block[n].deltas, (uint8_t *)code_buffer[n], block_length);
						break;
					case SS_SS2_3:
						range_encode_ss2_3(block[n].deltas, (uint8_t *)code_buffer[n], block_length);
						break;
					case SS_SS3:
						range_encode_ss3(block[n].deltas, (uint8_t *)code_buffer[n], block_length);
						break;
					default:
						// unreachable
						debug_assert(0 && "unexpected SSDPCM mode");
						break;
					}
				}
				
				for (n = 0; n <= stereo; n++)
				{
					switch (format)
					{
					case W_U8:
						sample_encode_u8_overflow(sample_conv_buffer, block[n].slopes, block[n].num_deltas / 2);
						break;
					case W_S16LE:
						sample_encode_u16(sample_conv_buffer, block[n].slopes, block[n].num_deltas / 2);
						break;
					default:
						// unreachable
						break;
					}
				
#pragma omp critical
					err = wav_write_ssdpcm_block(outfile, initial_sample_temp, sample_conv_buffer, code_buffer[n], block_index, n);
					if (err != E_OK)
					{
						char err_msg[256];
						int errno_copy = errno;
						snprintf(err_msg, 256, "Write error (%s)", error_enum_strs[err]);
						// Try to properly close the WAV file anyway
#pragma omp critical
						wav_close(outfile, &err);
						exit_error(err_msg, strerror(errno_copy));
					}
				}
			}
		
		
			if (decode_mode)
			{
				int i;
//#pragma omp critical
				{
//#pragma omp atomic capture
					{ block_index = block_count; block_count++; }
					for (n = 0; n <= stereo; n++)
					{
						err = wav_read_ssdpcm_block(infile, initial_sample_temp + n * 2, sample_conv_buffer + n * (num_deltas / 2) * 4, code_buffer[n], n);
						if (err != E_OK)
						{
							break;
						}
					}
				}
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
//#pragma omp critical
					wav_close(outfile, &err);
					exit_error(err_msg, strerror(errno_copy));
				}
				for (n = 0; n <= stereo; n++)
				{
					switch (format)
					{
					case W_U8:
						sample_decode_u8(block[n].slopes, sample_conv_buffer + n * (num_deltas / 2) * 4, block[n].num_deltas / 2);
						if (n == 0 && has_reference_sample_on_every_block)
						{
							sample_decode_u8(&block[0].initial_sample, initial_sample_temp, 1);
							sample_decode_u8(&block[1].initial_sample, initial_sample_temp + 2, 1);
						}
						break;
					case W_S16LE:
						sample_decode_u16(block[n].slopes, sample_conv_buffer + n * (num_deltas / 2) * 4, block[n].num_deltas / 2);
						if (n == 0 && has_reference_sample_on_every_block)
						{
							sample_decode_s16(&block[0].initial_sample, (int16_t *)initial_sample_temp, 1);
							sample_decode_s16(&block[1].initial_sample, ((int16_t *)initial_sample_temp) + 1, 1);
						}
						break;
					default:
						// unreachable
						break;
					}
					
					bitpacker.byte_buf.buffer = code_buffer[n];
					bitpacker.byte_buf.offset = 0;
					bitpacker.bit_index = 0;
					switch (mode)
					{
					case SS_SS1:
					case SS_SS1C:
						for (i = 0; i < block_length; i++)
						{
							err = get_bits_msbfirst(&block[n].deltas[i], &bitpacker, 1);
							if (err != E_OK)
							{
								exit_error("Runtime error: get_bits_msbfirst returned non-ok status", error_enum_strs[err]);
							}
						}
						break;
					case SS_SS2:
						for (i = 0; i < block_length; i++)
						{
							err = get_bits_msbfirst(&block[n].deltas[i], &bitpacker, 2);
							if (err != E_OK)
							{
								exit_error("Runtime error: get_bits_msbfirst returned non-ok status", error_enum_strs[err]);
							}
						}
						break;
					case SS_SS1_6:
						range_decode_ss1_6((uint8_t *)code_buffer[n], block[n].deltas, code_buffer_size);
						break;
					case SS_SS2_3:
						range_decode_ss2_3((uint8_t *)code_buffer[n], block[n].deltas, code_buffer_size);
						break;
					case SS_SS3:
						range_decode_ss3((uint8_t *)code_buffer[n], block[n].deltas, code_buffer_size);
						break;
					default:
						// unreachable
						debug_assert(0 && "unexpected SSDPCM mode");
						break;
					}
					
					for (i = 0; i < block[n].num_deltas / 2; i++)
					{
						block[n].slopes[i + block[n].num_deltas / 2] = -block[n].slopes[i];
					}
				
//#pragma omp critical
					fprintf(stderr, "\rDecoding block %lu...", block_index);
					ssdpcm_block_decode(sample_buffer[n], &block[n]);
					if (comb_filter)
					{
						sample_filter_comb(sample_buffer[n], block_length, block[n].initial_sample);
					}
				}
				
				switch (format)
				{
				case W_U8:
					sample_encode_u8_overflow_multichannel((uint8_t *)sample_conv_buffer, sample_buffer, block_length, stereo + 1);
					break;
				case W_S16LE:
					sample_encode_s16_multichannel((int16_t *)sample_conv_buffer, sample_buffer, block_length, stereo + 1);
					break;
				default:
					// unreachable
					break;
				}
				
//#pragma omp critical
				wav_write(outfile, sample_conv_buffer, block_length, block_index * block_length, &err);
				if (err != E_OK)
				{
					char err_msg[256];
					int errno_copy = errno;
					snprintf(err_msg, 256, "Write error (%s)", error_enum_strs[err]);
					// Try to properly close the WAV file anyway
//#pragma omp critical
					wav_close(outfile, &err);
					exit_error(err_msg, strerror(errno_copy));
				}
			}
		}

		for (n = 0; n <= stereo; n++)
		{
			free(code_buffer[n]);
			free(sample_buffer[n]);
			free(delta_buffer[n]);
		}
		sigma.methods->free(&(sigma.state));
		free(sample_conv_buffer);
	}
	
	wav_close(infile, &err);
	wav_close(outfile, &err);
	
	fprintf(stderr, "\nDone.\n");
	free(infile);
	free(outfile);
	
	return 0;
}
