
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encode.h>
#include <sample.h>
#include <errors.h>

void
exit_error (const char *msg)
{
	fprintf(stderr, "\nOof!\n%s\n", msg);
	exit(1);
}

void
write_block_params (FILE *dest, uint8_t initial_sample, uint8_t length)
{
	fprintf(dest, "initial_sample := %hhd\n", initial_sample);
	fprintf(dest, "length := %hhd\n", length);
}

//static const char out_decoded_suffix[] = "decoded.u8";
static const char out_bitstream_suffix[] = "bits.bin";
static const char out_slopes_suffix[] = "slopes.bin";
static const char out_params_suffix[] = "params.inc";

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
	//FILE *out_decoded;
	FILE *out_bitstream, *out_slopes, *out_params;
	//char *out_decoded_name;
	char *out_bitstream_name, *out_slopes_name, *out_params_name;
	size_t outname_length;
	int block_count = 0, superblock_index = 0;
	sigma_tracker sigma;
	ssdpcm_block block;
	sample_t temp_initial_sample;

	block.deltas = delta_buffer;
	block.slopes = slopes;
	block.num_deltas = 4;
	block.length = sizeof(u8_buffer);

	sigma.methods = sigma_u7_overflow;
	sigma.methods->alloc(&sigma.state);
	
	encoded_buffer.byte_buf.buffer = encoded_data;
	encoded_buffer.byte_buf.buffer_size = sizeof(encoded_data);
	
	if (argc < 3)
	{
		exit_error("Usage: nes_encoder infile.u8 outfiles_name");
	}
	
	outname_length = strnlen(argv[2], 1024);
	//out_decoded_name = malloc(outname_length + sizeof(out_decoded_suffix) + 19);
	out_bitstream_name = malloc(outname_length + sizeof(out_bitstream_suffix) + 19);
	out_slopes_name = malloc(outname_length + sizeof(out_slopes_suffix) + 19);
	out_params_name = malloc(outname_length + sizeof(out_params_suffix) + 19);
	
	infile = fopen(argv[1], "rb");
	if (!infile)
	{
		exit_error("Could not read input file.");
	}
	
	read_data = fread(u8_buffer, sizeof(uint8_t), sizeof(u8_buffer), infile);
	block.initial_sample = u8_buffer[0] >> 1;
	temp_initial_sample = block.initial_sample;
	
	while (read_data == sizeof(u8_buffer))
	{
		sample_t temp_last_sample = block.initial_sample;
		
		//snprintf(out_decoded_name, outname_length + sizeof(out_decoded_suffix) + 19, "%s_%d_%s", argv[2],
		//	superblock_index, out_decoded_suffix);
		snprintf(out_bitstream_name, outname_length + sizeof(out_bitstream_suffix) + 19, "%s_%d_%s", argv[2],
			superblock_index, out_bitstream_suffix);
		snprintf(out_slopes_name, outname_length + sizeof(out_slopes_suffix) + 19, "%s_%d_%s", argv[2],
			superblock_index, out_slopes_suffix);
		snprintf(out_params_name, outname_length + sizeof(out_params_suffix) + 19, "%s_%d_%s", argv[2],
			superblock_index, out_params_suffix);
		
		//out_decoded = fopen(out_decoded_name, "wb");
		out_bitstream = fopen(out_bitstream_name, "wb");
		out_slopes = fopen(out_slopes_name, "wb");
		out_params = fopen(out_params_name, "w");
		if (!out_bitstream || !out_slopes || !out_params)
		{
			exit_error("Could not open output files.");
		}
		
		for (block_count = 0; block_count < 256 && read_data == sizeof(u8_buffer); block_count++)
		{
			size_t i;
			fprintf(stderr, "\rEncoding block %d:%d...", superblock_index, block_count);
			sample_convert_u8_to_u7(u8_buffer, u8_buffer, sizeof(u8_buffer));
			sample_decode_u8(sample_buffer, u8_buffer, sizeof(u8_buffer));
			(void) ssdpcm_encode_bruteforce(&block, sample_buffer, &sigma);
			
			for (i = 0; i < block.num_deltas / 2; i++)
			{
				uint8_t slope = slopes[i] & 0xff;
				fwrite(&slope, sizeof(uint8_t), 1, out_slopes);
			}
			
			ssdpcm_block_decode(sample_buffer, &block);
			temp_last_sample = sample_buffer[sizeof(u8_buffer) - 1];
			
			encoded_buffer.byte_buf.offset = 0;
			encoded_buffer.bit_index = 0;
			memset(encoded_data, 0, sizeof(encoded_data));
			
			for (i = 0; i < block.length; i++)
			{
				err_t rc = put_bits_msbfirst(&encoded_buffer, block.deltas[i], 2);
				if (rc != E_OK)
				{
					fprintf(stderr, "\nrc = %d", rc);
					exit_error("put_bits_msbfirst returned non-ok status");
				}
			}
			
			fwrite(encoded_data, sizeof(uint8_t),
			       sizeof(encoded_data), out_bitstream);
			
			//sample_filter_comb(sample_buffer, sizeof(u8_buffer), block.initial_sample);
			sample_encode_u8_overflow(u8_buffer, sample_buffer, sizeof(u8_buffer));
			block.initial_sample = temp_last_sample;
			
			//sample_convert_u7_to_u8(u8_buffer, u8_buffer, sizeof(u8_buffer));
			//fwrite(u8_buffer, 1, sizeof(u8_buffer), out_decoded);
			
			read_data = fread(u8_buffer, sizeof(uint8_t), sizeof(u8_buffer), infile);
		}
		
		write_block_params(out_params, temp_initial_sample, block_count & 0xff);
		temp_initial_sample = temp_last_sample;
		superblock_index++;
		
		//fclose(out_decoded);
		fclose(out_bitstream);
		fclose(out_slopes);
		fclose(out_params);
	}
	
	//fprintf(stderr, "\n%d blocks encoded.\n", block_count);
	fprintf(stderr, "\nDone.\n");
	
	sigma.methods->free(&(sigma.state));
	//free(out_decoded_name);
	free(out_bitstream_name);
	free(out_slopes_name);
	free(out_params_name);
	
	return 0;
}
