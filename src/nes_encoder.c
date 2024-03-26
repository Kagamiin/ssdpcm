
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encode.h>
#include <sample.h>
#include <errors.h>
#include <errno.h>
#include <range_coder.h>

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

static const char out_decoded_suffix[] = "decoded.u8";
static const char out_bitstream_suffix[] = "bits.bin";
static const char out_slopes_suffix[] = "slopes.bin";
static const char out_params_suffix[] = "params.inc";

static const char usage[] = "\
\033[97mUsage:\033[0m nes_encoder (ss1|ss1c|ss2) infile.u8 outfiles_name\n\
- The encoding mode can either be:\n\
  - \033[96mss1\033[0m - 1-bit SSDPCM\n\
  - \033[96mss1c\033[0m - 1-bit SSDPCM with comb filtering\n\
  - \033[96mss1.6\033[0m - 1.6-bit SSDPCM\n\
  - \033[96mss2\033[0m - 2-bit SSDPCM\n\
- \033[96minfile.u8\033[0m should be a raw 8-bit unsigned PCM file.\n\
  You can use an 8-bit unsigned WAV too, but you'll get some garbage in the\n\
  beginning of the audio. (hint: use Audacity to export raw 8-bit PCM)\n\
- \033[96moutfiles_name\033[0m is the prefix path for the output files.\n\
  NOTE: This program generates A LOT of files in the output path.\n\
- This program doesn't do sample rate conversion. Your input file should\n\
  already be in the correct sample rate for playback. You can calculate that\n\
  with the following equation:\n\
         \033[96msample_rate = 315/88/2 * 1000000 / cycles_per_sample\033[0m\n\
  where cycles_per_sample is the number of clock cycles between each sample,\n\
  set either on the delay parameter (for non-IRQ) or timer interrupt (for IRQ)";

err_t put_bits_msbfirst (bitstream_buffer *buf, codeword_t src, uint8_t num_bits);

int
main (int argc, char **argv)
{
	uint8_t u8_buffer[128];
	sample_t sample_buffer[128];
	codeword_t delta_buffer[128];
	uint8_t encoded_data[32];
	bitstream_buffer encoded_buffer;
	sample_t slopes[4];
	size_t read_data;
	FILE *infile;
	FILE *out_decoded;
	FILE *out_bitstream, *out_slopes, *out_params;
	char *out_decoded_name;
	char *out_bitstream_name, *out_slopes_name, *out_params_name;
	size_t outname_length;
	int block_count = 0, superblock_index = 0;
	sigma_tracker sigma;
	ssdpcm_block block;
	sample_t temp_initial_sample;
	int bits_per_sample;
	int codes_per_byte;
	bool comb_filter = FALSE;

	block.deltas = delta_buffer;
	block.slopes = slopes;
	block.length = sizeof(u8_buffer);
	
	if (argc < 4)
	{
		exit_error(usage, NULL);
	}
	
	if (!strcmp("ss1", argv[1]))
	{
		bits_per_sample = 1;
		block.num_deltas = 2;
		codes_per_byte = 8;
	}
	else if (!strcmp("ss1c", argv[1]))
	{
		bits_per_sample = 1;
		block.num_deltas = 2;
		codes_per_byte = 8;
		comb_filter = TRUE;
	}
	else if (!strcmp("ss1.6", argv[1]))
	{
		bits_per_sample = 0;
		block.num_deltas = 3;
		codes_per_byte = 5;
		block.length = 80;
	}
	else if (!strcmp("ss2", argv[1]))
	{
		bits_per_sample = 2;
		block.num_deltas = 4;
		codes_per_byte = 4;
	}
	else
	{
		bits_per_sample = 0;
		block.num_deltas = 0;
		codes_per_byte = 0;
		exit_error(usage, NULL);
	}
	
	if (comb_filter)
	{
		sigma.methods = sigma_u7_overflow_comb;
	}
	else
	{
		sigma.methods = sigma_u7_overflow;
	}
	sigma.methods->alloc(&sigma.state);
	
	encoded_buffer.byte_buf.buffer = encoded_data;
	encoded_buffer.byte_buf.buffer_size = sizeof(encoded_data);
	
	outname_length = strnlen(argv[3], 1024);
	out_decoded_name = malloc(outname_length + sizeof(out_decoded_suffix) + 19);
	out_bitstream_name = malloc(outname_length + sizeof(out_bitstream_suffix) + 19);
	out_slopes_name = malloc(outname_length + sizeof(out_slopes_suffix) + 19);
	out_params_name = malloc(outname_length + sizeof(out_params_suffix) + 19);
	
	infile = fopen(argv[2], "rb");
	if (!infile)
	{
		exit_error("Could not read input file", strerror(errno));
	}
	
	read_data = fread(u8_buffer, sizeof(uint8_t), block.length, infile);
	block.initial_sample = u8_buffer[0] >> 1;
	temp_initial_sample = block.initial_sample;
	
	snprintf(out_decoded_name, outname_length + sizeof(out_decoded_suffix) + 19, "%s_%s", argv[3],
		out_decoded_suffix);
	out_decoded = fopen(out_decoded_name, "wb");
	
	while (read_data == block.length)
	{
		sample_t temp_last_sample = block.initial_sample;
		snprintf(out_bitstream_name, outname_length + sizeof(out_bitstream_suffix) + 19, "%s_%d_%s", argv[3],
			superblock_index, out_bitstream_suffix);
		snprintf(out_slopes_name, outname_length + sizeof(out_slopes_suffix) + 19, "%s_%d_%s", argv[3],
			superblock_index, out_slopes_suffix);
		snprintf(out_params_name, outname_length + sizeof(out_params_suffix) + 19, "%s_%d_%s", argv[3],
			superblock_index, out_params_suffix);
		
		out_bitstream = fopen(out_bitstream_name, "wb");
		out_slopes = fopen(out_slopes_name, "wb");
		out_params = fopen(out_params_name, "w");
		if (!out_bitstream || !out_slopes || !out_params)
		{
			exit_error("Could not open output files", strerror(errno));
		}
		
		for (block_count = 0; block_count < 256 && read_data == block.length; block_count++)
		{
			size_t i;
			fprintf(stderr, "\rEncoding block %d:%d...", superblock_index, block_count);
			sample_convert_u8_to_u7(u8_buffer, u8_buffer, block.length);
			sample_decode_u8(sample_buffer, u8_buffer, block.length);
			(void) ssdpcm_encode_binary_search(&block, sample_buffer, &sigma);
			
			for (i = 0; i < block.num_deltas / 2; i++)
			{
				uint8_t slope = slopes[i] & 0xff;
				fwrite(&slope, sizeof(uint8_t), 1, out_slopes);
			}
			
			ssdpcm_block_decode(sample_buffer, &block);
			temp_last_sample = sample_buffer[block.length - 1];
			
			encoded_buffer.byte_buf.offset = 0;
			encoded_buffer.bit_index = 0;
			memset(encoded_data, 0, sizeof(encoded_data));
			
			if (bits_per_sample > 0)
			{
				for (i = 0; i < block.length; i++)
				{
					err_t rc = put_bits_msbfirst(&encoded_buffer, block.deltas[i], bits_per_sample);
					if (rc != E_OK)
					{
						fprintf(stderr, "\nrc = %d", rc);
						exit_error("put_bits_msbfirst returned non-ok status", NULL);
					}
				}
			}
			else
			{
				switch (block.num_deltas)
				{
				case 3:
					range_encode_ss1_6(block.deltas, encoded_data, block.length);
				}
			}
			
			fwrite(encoded_data, sizeof(uint8_t),
			       block.length / codes_per_byte, out_bitstream);
			
			if (comb_filter)
			{
				sample_filter_comb(sample_buffer, block.length, block.initial_sample);
			}
			block.initial_sample = temp_last_sample;
			
			sample_encode_u8_overflow(u8_buffer, sample_buffer, block.length);
			sample_convert_u7_to_u8(u8_buffer, u8_buffer, block.length);
			fwrite(u8_buffer, 1, block.length, out_decoded);
			
			read_data = fread(u8_buffer, sizeof(uint8_t), block.length, infile);
		}
		
		write_block_params(out_params, temp_initial_sample, block_count & 0xff);
		temp_initial_sample = temp_last_sample;
		superblock_index++;
		
		fclose(out_bitstream);
		fclose(out_slopes);
		fclose(out_params);
	}
	
	fprintf(stderr, "\nDone.\n");
	
	fclose(out_decoded);
	sigma.methods->free(&(sigma.state));
	free(out_decoded_name);
	free(out_bitstream_name);
	free(out_slopes_name);
	free(out_params_name);
	
	return 0;
}
