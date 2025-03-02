
#include "types.h"
#include <stdint.h>
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
\033[97mUsage:\033[0m encoder (mode) infile.wav outfile.aud [-d|--dither [strength]]\n\
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
  decoded WAV file in the case of the decode mode.\n\
- \033[96m-d\033[0m/\033[96m--dither\033[0m enables/disables dithering of the input WAV file to be encoded.\n\
  It's disabled by default. Use this flag to enable this behavior.\n\
  This also takes an optional argument that defines the dithering strength.\n\
  Valid values range from \033[96m0\033[0m to \033[96m255\033[0m, where \033[96m0\033[0m is the default and \033[96m255\033[0m is insanely\n\
  strong.\n\
  \033[96mNOTE:\033[0m dithering is currently not working right and it's not advised to\n\
  use it.\n\
";

#define SAMPLES_PER_BLOCK 128

int
main (int argc, char **argv)
{
	wav_handle *infile;
	wav_handle *outfile;
	
	size_t block_count = 0;
	
	char *infile_name;
	char *outfile_name;
	wav_sample_fmt format;
	ssdpcm_block_mode mode;
	uint32_t sample_rate;
	size_t code_buffer_size = 0;
	long block_length;
	int num_deltas;
	int i;
	bool stereo = false;
	bool comb_filter = false;
	bool decode_mode = false;
	bool has_reference_sample_on_every_block = false;
	
	void *code_buffer[2];
	void *sample_conv_buffer = NULL;
	bitstream_buffer bitpacker;
	sample_t *sample_buffer[2];
	sample_t *dither_buffer[2];
	codeword_t *delta_buffer[2];
	sample_t slopes[2][16];
	sigma_tracker sigma;
	ssdpcm_block block[2];
	sample_t temp_last_sample[2];
	err_t err;
	
	bool dither = false;
	uint8_t dither_strength = 0;
	
	memset(slopes[0], 0, sizeof(sample_t) * 16);
	memset(slopes[1], 0, sizeof(sample_t) * 16);
	
	if (argc < 4 || argc > 6)
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
	
	if (argc > 4)
	{
		bool is_dither_arg = !strcmp("-d", argv[4]) || !strcmp("--dither", argv[4]);
		if (is_dither_arg)
		{
			dither = true;
		}
		else
		{
			fprintf(stderr, "Invalid dither argument '%s'.\n", argv[4]);
			exit_error(usage, NULL);
		}
		if (argc > 5) {
			int result = sscanf(argv[5], "%hhu", &dither_strength);
			if (result != 1)
			{
				fprintf(stderr, "Invalid dither strength '%s'.\n", argv[5]);
				exit_error(usage, NULL);
			}
		}
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
		if (format == W_ERROR)
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
		for (i = 0; i <= stereo; i++)
		{
			code_buffer[i] = malloc(code_buffer_size);
			sample_buffer[i] = malloc(sizeof(sample_t) * block_length);
			dither_buffer[i] = malloc(sizeof(sample_t) * block_length);
			delta_buffer[i] = malloc(sizeof(codeword_t) * block_length);
		}
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
		sample_conv_buffer = malloc(wav_get_sizeof(infile, block_length * (stereo + 1)));
	}
	
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
	case W_SSDPCM:
		exit_error("Input file appears to be SSDPCM, not uncompressed WAV - please use \"decode\" option", NULL);
		break;
	default:
		exit_error("Input file has unrecognized format/codec - only 8/16-bit PCM is allowed", NULL);
		break;
	}
	
	sigma.methods->alloc(&sigma.state);
	
	if (!strcmp(outfile_name, infile_name))
	{
		exit_error("Input file and output file cannot be the same file!", NULL);
	}
	
	outfile = wav_open(outfile, outfile_name, W_CREATE, &err);
	if (outfile == NULL)
	{
		char err_msg[256];
		snprintf(err_msg, 256, "Could not open output file '%s' (%s). errno", outfile_name, error_enum_strs[err]);
		exit_error(err_msg, strerror(errno));
	}
	sample_rate = wav_get_sample_rate(infile);
	wav_set_sample_rate(outfile, sample_rate);
	wav_set_num_channels(outfile, stereo + 1);
	
	if (decode_mode)
	{
		wav_set_format(outfile, format);
		sample_conv_buffer = malloc(wav_get_sizeof(outfile, block_length));
		has_reference_sample_on_every_block = wav_ssdpcm_has_reference_sample_on_every_block(infile, &err);
	}
	else
	{
		wav_init_ssdpcm(outfile, format, mode, block_length, false);
		code_buffer_size = wav_get_ssdpcm_code_bytes_per_block(outfile, &err);
		for (i = 0; i <= stereo; i++)
		{
			code_buffer[i] = malloc(code_buffer_size);
			sample_buffer[i] = malloc(sizeof(sample_t) * block_length);
			dither_buffer[i] = malloc(sizeof(sample_t) * block_length);
			delta_buffer[i] = malloc(sizeof(codeword_t) * block_length);
		}
	}
	
	for (i = 0; i <= stereo; i++)
	{
		block[i].num_deltas = num_deltas;
		block[i].deltas = delta_buffer[i];
		block[i].slopes = slopes[i];
		block[i].length = block_length;
	}
	memset(&bitpacker, 0, sizeof(bitstream_buffer));
	bitpacker.byte_buf.buffer_size = code_buffer_size;
	
	wav_write_header(outfile);
	wav_seek(infile, 0, SEEK_SET);
	wav_seek(outfile, 0, SEEK_SET);
	
	if (decode_mode)
	{
		uint8_t initial_sample_temp[4];
		for (i = 0; i <= stereo; i++)
		{
			err = wav_read_ssdpcm_block(infile, initial_sample_temp, sample_conv_buffer, code_buffer[i], i);
			if (err != E_OK)
			{
				char err_msg[256];
				snprintf(err_msg, 256, "Read error (%s)", error_enum_strs[err]);
				exit_error(err_msg, strerror(errno));
			}
			switch (format)
			{
			case W_U8:
				sample_decode_u8(block[i].slopes, sample_conv_buffer, block[i].num_deltas / 2);
				if (i == 0)
				{
					sample_decode_u8(&block[0].initial_sample, initial_sample_temp, 1);
					sample_decode_u8(&block[1].initial_sample, initial_sample_temp + 1, 1);
				}
				break;
			case W_S16LE:
				sample_decode_u16(block[i].slopes, sample_conv_buffer, block[i].num_deltas / 2);
				if (i == 0)
				{
					sample_decode_s16(&block[0].initial_sample, ((int16_t *)initial_sample_temp), 1);
					sample_decode_s16(&block[1].initial_sample, ((int16_t *)initial_sample_temp) + 1, 1);
				}
				break;
			default:
				// unreachable
				break;
			}
		}
	}
	else
	{
		(void) wav_read(infile, sample_conv_buffer, block_length, &err);
		if (err != E_OK)
		{
			char err_msg[256];
			snprintf(err_msg, 256, "Read error (%s)", error_enum_strs[err]);
			exit_error(err_msg, strerror(errno));
		}
		switch (format)
		{
		case W_U8:
			block[0].initial_sample = ((uint8_t *)sample_conv_buffer)[0];
			block[1].initial_sample = ((uint8_t *)sample_conv_buffer)[1];
			break;
		case W_S16LE:
			block[0].initial_sample = ((int16_t *)sample_conv_buffer)[0];
			block[1].initial_sample = ((int16_t *)sample_conv_buffer)[1];
			break;
		default:
			// unreachable
			break;
		}
	}

	while (err == E_OK)
	{
		uint8_t initial_sample_temp[4];
		int c;
		if (!decode_mode)
		{
			switch (format)
			{
			case W_U8:
				if (dither)
				{
					sample_decode_u8_multichannel(dither_buffer, (uint8_t *)sample_conv_buffer, block_length, stereo + 1);
					sample_dither_triangular(sample_buffer, dither_buffer, block_length, stereo + 1, dither_strength, 0, UINT8_MAX);
				}
				else
				{
					sample_decode_u8_multichannel(sample_buffer, (uint8_t *)sample_conv_buffer, block_length, stereo + 1);
				}
				break;
			case W_S16LE:
				if (dither)
				{
					sample_decode_s16_multichannel(dither_buffer, (int16_t *)sample_conv_buffer, block_length, stereo + 1);
					sample_dither_triangular(sample_buffer, dither_buffer, block_length, stereo + 1, dither_strength, INT16_MIN, INT16_MAX);
				}
				else
				{
					sample_decode_s16_multichannel(sample_buffer, (int16_t *)sample_conv_buffer, block_length, stereo + 1);
				}
				break;
			default:
				// unreachable
				break;
			}
			
			memcpy(initial_sample_temp, sample_conv_buffer, sizeof(initial_sample_temp));
			
			fprintf(stderr, "\rEncoding block %lu...", block_count);
			for (c = 0; c <= stereo; c++)
			{
				(void) ssdpcm_encode_binary_search(&block[c], sample_buffer[c], &sigma);
				ssdpcm_block_decode(sample_buffer[c], &block[c]);
				temp_last_sample[c] = sample_buffer[c][block_length - 1];
				if (comb_filter)
				{
					sample_filter_comb(sample_buffer[c], block_length, block[c].initial_sample);
				}
				
				bitpacker.byte_buf.buffer = code_buffer[c];
				bitpacker.byte_buf.offset = 0;
				bitpacker.bit_index = 0;
				memset(code_buffer[c], 0, code_buffer_size);
				switch (mode)
				{
				case SS_SS1:
				case SS_SS1C:
				{
					int i;
					for (i = 0; i < block_length; i++)
					{
						err = put_bits_msbfirst(&bitpacker, block[c].deltas[i], 1);
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
						err = put_bits_msbfirst(&bitpacker, block[c].deltas[i], 2);
						if (err != E_OK)
						{
							exit_error("Runtime error: put_bits_msbfirst returned non-ok status", error_enum_strs[err]);
						}
					}
					break;
				}
				case SS_SS1_6:
					range_encode_ss1_6(block[c].deltas, (uint8_t *)(code_buffer[c]), block_length);
					break;
				case SS_SS2_3:
					range_encode_ss2_3(block[c].deltas, (uint8_t *)(code_buffer[c]), block_length);
					break;
				case SS_SS3:
					range_encode_ss3(block[c].deltas, (uint8_t *)(code_buffer[c]), block_length);
					break;
				default:
					// unreachable
					debug_assert(0 && "unexpected SSDPCM mode");
					break;
				}
			}
			
			for (c = 0; c <= stereo; c++)
			{
				switch (format)
				{
				case W_U8:
					sample_encode_u8_overflow(sample_conv_buffer, block[c].slopes, block[c].num_deltas / 2);
					break;
				case W_S16LE:
					sample_encode_u16(sample_conv_buffer, block[c].slopes, block[c].num_deltas / 2);
					break;
				default:
					// unreachable
					break;
				}

				err = wav_write_ssdpcm_block(outfile, initial_sample_temp, sample_conv_buffer, code_buffer[c], -1, c);
				if (err != E_OK)
				{
					char err_msg[256];
					int errno_copy = errno;
					snprintf(err_msg, 256, "Write error (%s)", error_enum_strs[err]);
					// Try to properly close the WAV file anyway
					wav_close(outfile, &err);
					exit_error(err_msg, strerror(errno_copy));
				}
				
				block[c].initial_sample = temp_last_sample[c];
			}
			
			(void) wav_read(infile, sample_conv_buffer, block_length, &err);
			if (err != E_OK)
			{
				if (err == E_END_OF_STREAM)
				{
					goto finish;
				}
				char err_msg[256];
				int errno_copy = errno;
				snprintf(err_msg, 256, "Read error (%s)", error_enum_strs[err]);
				// Try to properly close the WAV file anyway
				wav_close(outfile, &err);
				exit_error(err_msg, strerror(errno_copy));
			}
		}
		
		if (decode_mode)
		{

			int c;
			for (c = 0; c <= stereo; c++)
			{
				bitpacker.byte_buf.buffer = code_buffer[c];
				bitpacker.byte_buf.offset = 0;
				bitpacker.bit_index = 0;
				switch (mode)
				{
				case SS_SS1:
				case SS_SS1C:
					for (i = 0; i < block_length; i++)
					{
						err = get_bits_msbfirst(&block[c].deltas[i], &bitpacker, 1);
						if (err != E_OK)
						{
							exit_error("Runtime error: get_bits_msbfirst returned non-ok status", error_enum_strs[err]);
						}
					}
					break;
				case SS_SS2:
					for (i = 0; i < block_length; i++)
					{
						err = get_bits_msbfirst(&block[c].deltas[i], &bitpacker, 2);
						if (err != E_OK)
						{
							exit_error("Runtime error: get_bits_msbfirst returned non-ok status", error_enum_strs[err]);
						}
					}
					break;
				case SS_SS1_6:
					range_decode_ss1_6((uint8_t *)code_buffer[c], block[c].deltas, code_buffer_size);
					break;
				case SS_SS2_3:
					range_decode_ss2_3((uint8_t *)code_buffer[c], block[c].deltas, code_buffer_size);
					break;
				case SS_SS3:
					range_decode_ss3((uint8_t *)code_buffer[c], block[c].deltas, code_buffer_size);
					break;
				default:
					// unreachable
					debug_assert(0 && "unexpected SSDPCM mode");
					break;
				}
				
				for (i = 0; i < block[c].num_deltas / 2; i++)
				{
					block[c].slopes[i + block[c].num_deltas / 2] = -block[c].slopes[i];
				}
				
				fprintf(stderr, "\rDecoding block %lu...", block_count);
				ssdpcm_block_decode(sample_buffer[c], &block[c]);
				temp_last_sample[c] = sample_buffer[c][block_length - 1];
				if (comb_filter)
				{
					sample_filter_comb(sample_buffer[c], block_length, block[c].initial_sample);
				}
				block[c].initial_sample = temp_last_sample[c];
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
			
			wav_write(outfile, sample_conv_buffer, block_length, -1, &err);
			if (err != E_OK)
			{
				char err_msg[256];
				int errno_copy = errno;
				snprintf(err_msg, 256, "Write error (%s)", error_enum_strs[err]);
				// Try to properly close the WAV file anyway
				wav_close(outfile, &err);
				exit_error(err_msg, strerror(errno_copy));
			}
			
			for (c = 0; c <= stereo; c++)
			{
				err = wav_read_ssdpcm_block(infile, initial_sample_temp, sample_conv_buffer, code_buffer[c], c);
				if (err != E_OK)
				{
					if (err == E_END_OF_STREAM)
					{
						goto finish;
					}
					char err_msg[256];
					int errno_copy = errno;
					snprintf(err_msg, 256, "Read error (%s)", error_enum_strs[err]);
					// Try to properly close the WAV file anyway
					wav_close(outfile, &err);
					exit_error(err_msg, strerror(errno_copy));
				}
				switch (format)
				{
				case W_U8:
					sample_decode_u8(block[c].slopes, sample_conv_buffer, block[c].num_deltas / 2);
					if (c == 0 && has_reference_sample_on_every_block)
					{
						sample_decode_u8(&block[0].initial_sample, initial_sample_temp, 1);
						sample_decode_u8(&block[1].initial_sample, initial_sample_temp + 1, 1);
					}
					break;
				case W_S16LE:
					sample_decode_u16(block[c].slopes, sample_conv_buffer, block[c].num_deltas / 2);
					if (c == 0 && has_reference_sample_on_every_block)
					{
						sample_decode_s16(&block[0].initial_sample, ((int16_t *)initial_sample_temp), 1);
						sample_decode_s16(&block[1].initial_sample, ((int16_t *)initial_sample_temp) + 1, 1);
					}
					break;
				default:
					// unreachable
					break;
				}
			}
		}
		
		block_count++;
	}
finish:
	
	wav_close(infile, &err);
	wav_close(outfile, &err);
	
	fprintf(stderr, "\nDone.\n");
	sigma.methods->free(&(sigma.state));
	free(infile);
	free(outfile);
	for (i = 0; i <= stereo; i++)
	{
		free(code_buffer[i]);
		free(sample_buffer[i]);
		free(delta_buffer[i]);
	}
	free(sample_conv_buffer);
	
	return 0;
}
