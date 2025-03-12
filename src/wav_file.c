/*
 * ssdpcm: reference implementation for the SSDPCM audio codec designed by
 * Algorithm.
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

#include <types.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errors.h>
#include <errno.h>

// TODO!

static const char *riff_magic_id = "RIFF";
static const char *wav_magic_id = "WAVE";
static const char *wav_fmt_chunk_id = "fmt ";
static const char *wav_data_chunk_id = "data";
static const char *ssdpcm_data_chunk_id = "SsDP";

// Yes, I made a plaintext GUID because I could. Yes, it is a valid GUID (though slightly off-spec) and a valid version 4 UUID.
static const char ssdpcm_codec_guid[] = // 50445353-4d43-4b3a-6167-616d69696e7e - note that GUID byte endianness is weird
{
	0x53, 0x53, 0x44, 0x50, 0x43, 0x4d, 0x3a, 0x4b, 0x61, 0x67, 0x61, 0x6d, 0x69, 0x69, 0x6e, 0x7e
};

// This is for reading high sample rate PCM WAV files that are sometimes encoded as WAVEFORMATEXTENSIBLE.
static const char waveformatext_pcm_codec_guid[] = // 00000001-0000-0010-8000-00aa00389b71 - note that GUID byte endianness is weird
{
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
};

static const uint16_t wave_format_ex_id = 0xfffe;

static const char *ssdpcm_mode_fourcc_list[] =
{
	"ss1 ",
	"ss1c",
	"s1.6",
	"ss2 ",
	"s2.3",
	"ss3 ",
	"\0"
};

#define MAX_NUM_SLOPES 16 // have you seen how long it takes to encode with 16 slopes, even with 8-bit input?



typedef struct
{
	// uint32_t id; // (implicit)
	uint16_t fmt_type;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t bytes_per_quantum;
	uint16_t bits_per_sample;
} wav_fmt_chunk;

typedef struct
{
	union
	{
		uint16_t valid_bits_per_sample;
		uint16_t samples_per_block;
		uint16_t reserved;
	} wfx_18_19;
	uint32_t channel_mask;
	char sub_format[16]; // GUID
} wave_format_ex;

typedef struct
{
	// uint32_t id; // (implicit)
	char mode_fourcc[4];
	uint8_t num_slopes;
	uint8_t bits_per_output_sample;
	uint8_t bytes_per_read_alignment;
	bool has_reference_sample_on_every_block;
	uint16_t block_length;
	uint16_t bytes_per_block;
} wav_ssdpcm_extra_chunk;

typedef struct
{
	// uint32_t id; // (implicit)
	uint32_t riff_payload_length;
	uint32_t fmt_length;
	wav_fmt_chunk fmt_content;
	uint32_t data_length;
	uint16_t extra_length;
	wave_format_ex *fmt_ex_chunk;
	wav_ssdpcm_extra_chunk *ssdpcm_extra_chunk;
	uint32_t data_offset_in_file;
} wav_file;

typedef struct
{
	FILE *fp;
	wav_file *header;
	bool write_mode;
	bool no_extra_chunks;
	bool header_synced;
} wav_handle;

/**
 * Attempts to determine the cause of fseek() or fread() failing.
 */
static err_t
wav_read_eof_error_code_ (wav_handle *w)
{
	if (feof(w->fp))
	{
		return E_PREMATURE_END_OF_FILE;
	}
	if (ferror(w->fp))
	{
		return E_READ_ERROR;
	}
	return E_UNKNOWN_ERROR;
}

ssdpcm_block_mode
wav_get_ssdpcm_mode (wav_handle *w, err_t *err_out)
{
	int i;
	char codec_mode[5];
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return -1;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return -1;
	}
	
	wav_ssdpcm_extra_chunk *ssdpcm_ex = w->header->ssdpcm_extra_chunk;
	
	memset(codec_mode, '\0', sizeof(codec_mode));
	for (i = 0; i < NUM_SSDPCM_MODES; i++)
	{
		memcpy(codec_mode, ssdpcm_ex->mode_fourcc, 4);
		if (strncmp(codec_mode, ssdpcm_mode_fourcc_list[i], sizeof(codec_mode)) == 0)
		{
			return (ssdpcm_block_mode) i;
		}
	}
	
	*err_out = E_UNRECOGNIZED_MODE;
	return -1;
}

static err_t
wav_read_ssdpcm_extra_chunk_ (wav_handle *w)
{
	wav_ssdpcm_extra_chunk *ssdpcm_ex;
	err_t err;
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		w->header->ssdpcm_extra_chunk = calloc(1, sizeof(wav_ssdpcm_extra_chunk));
		if (w->header->ssdpcm_extra_chunk == NULL)
		{
			return E_MEM_ALLOC;
		}
	}
	ssdpcm_ex = w->header->ssdpcm_extra_chunk;
	int x = fread(ssdpcm_ex->mode_fourcc, sizeof(char), 4, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	if (wav_get_ssdpcm_mode(w, &err) < 0)
	{
		return err;
	}
	x = fread(&ssdpcm_ex->num_slopes, sizeof(uint8_t), 1, w->fp);
	x = x && fread(&ssdpcm_ex->bits_per_output_sample, sizeof(uint8_t), 1, w->fp);
	x = x && fread(&ssdpcm_ex->bytes_per_read_alignment, sizeof(uint8_t), 1, w->fp);
	x = x && fread(&ssdpcm_ex->has_reference_sample_on_every_block, 1, 1, w->fp);
	x = x && fread(&ssdpcm_ex->block_length, sizeof(uint16_t), 1, w->fp);
	x = x && fread(&ssdpcm_ex->bytes_per_block, sizeof(uint16_t), 1, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	
	if (ssdpcm_ex->num_slopes > MAX_NUM_SLOPES)
	{
		return E_TOO_MANY_SLOPES;
	}
	if (ssdpcm_ex->bits_per_output_sample != 8 && ssdpcm_ex->bits_per_output_sample != 16)
	{
		return E_UNSUPPORTED_BITS_PER_SAMPLE;
	}
	
	return E_OK;
}

static err_t
wav_read_waveformatext_chunk_ (wav_handle *w)
{
	int x = fread(&w->header->extra_length, sizeof(uint16_t), 1, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	int64_t chunk_start = ftell(w->fp);
	wave_format_ex *fmt_ex;
	if (w->header->fmt_ex_chunk == NULL)
	{
		w->header->fmt_ex_chunk = calloc(1, sizeof(wave_format_ex));
		if (w->header->fmt_ex_chunk == NULL)
		{
			return E_MEM_ALLOC;
		}
	}
	fmt_ex = w->header->fmt_ex_chunk;
	x = fread(&fmt_ex->wfx_18_19.reserved, sizeof(uint16_t), 1, w->fp);
	x = x && fread(&fmt_ex->channel_mask, sizeof(uint32_t), 1, w->fp);
	x = x && fread(fmt_ex->sub_format, sizeof(char), 16, w->fp) == 16;
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}

	if (memcmp(fmt_ex->sub_format, ssdpcm_codec_guid, 16) == 0)
	{
		char chunk_id[5];
		memset(chunk_id, '\0', sizeof(chunk_id));
		x = fread(chunk_id, sizeof(char), 4, w->fp);
		if (!x)
		{
			return wav_read_eof_error_code_(w);
		}
		if (strncmp(chunk_id, ssdpcm_data_chunk_id, sizeof(chunk_id)) == 0)
		{
			err_t err = wav_read_ssdpcm_extra_chunk_(w);
			if (err != E_OK)
			{
				return err;
			}
		}
		else
		{
			return E_INVALID_SUBHEADER;
		}
	}
	else if (!(memcmp(fmt_ex->sub_format, waveformatext_pcm_codec_guid, 16) == 0))
	{
		return E_UNRECOGNIZED_SUBFORMAT;
	}

	x = fseek(w->fp, chunk_start + w->header->extra_length, SEEK_SET);
	if (x < 0)
	{
		return wav_read_eof_error_code_(w);
	}
	return E_OK;
}

static err_t
wav_read_fmt_chunk_ (wav_handle *w)
{
	int x;
	
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	
	// Header parsing assumes a little-endian machine
	// fmt chunk length
	x = fread(&w->header->fmt_length, sizeof(uint32_t), 1, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}

	int64_t chunk_start = ftell(w->fp);
	
	if (w->header->fmt_length < 16)
	{
		return E_FMT_CHUNK_TOO_SMALL;
	}
	wav_fmt_chunk *chunk = &w->header->fmt_content;
	x = fread(&chunk->fmt_type, sizeof(uint16_t), 1, w->fp);
	x = x && fread(&chunk->num_channels, sizeof(uint16_t), 1, w->fp);
	x = x && fread(&chunk->sample_rate, sizeof(uint32_t), 1, w->fp);
	x = x && fread(&chunk->byte_rate, sizeof(uint32_t), 1, w->fp);
	x = x && fread(&chunk->bytes_per_quantum, sizeof(uint16_t), 1, w->fp);
	x = x && fread(&chunk->bits_per_sample, sizeof(uint16_t), 1, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}

	if (w->header->fmt_content.fmt_type == wave_format_ex_id)
	{
		err_t err = wav_read_waveformatext_chunk_(w);
		if (err != E_OK)
		{
			return err;
		}
	}

	x = fseek(w->fp, chunk_start + w->header->fmt_length, SEEK_SET);
	if (x < 0)
	{
		return wav_read_eof_error_code_(w);
	}

	if (chunk->fmt_type != 1 && chunk->fmt_type != wave_format_ex_id)
	{
		return E_UNRECOGNIZED_FORMAT;
	}
	if ((chunk->sample_rate * chunk->bits_per_sample * chunk->num_channels / 8) != chunk->byte_rate && chunk->fmt_type != wave_format_ex_id)
	{
		return E_MISMATCHED_RATES;
	}
	if ((chunk->bits_per_sample * chunk->num_channels / 8) != chunk->bytes_per_quantum && chunk->fmt_type != wave_format_ex_id)
	{
		return E_MISMATCHED_BLOCK_SIZE;
	}
	if (chunk->bits_per_sample != 8 && chunk->bits_per_sample != 16 && chunk->fmt_type != wave_format_ex_id)
	{
		return E_UNSUPPORTED_BITS_PER_SAMPLE;
	}
	
	return E_OK;
}

static err_t
wav_skip_chunk_anon_ (wav_handle *w)
{
	int x;
	uint32_t chunk_length;
	x = fread(&chunk_length, sizeof(uint32_t), 1, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	x = fseek(w->fp, chunk_length, SEEK_CUR);
	if (x < 0)
	{
		return wav_read_eof_error_code_(w);
	}
	return E_OK;
}

static err_t
wav_find_fmt_chunk_ (wav_handle *w)
{
	char chunk_id[5];
	int x;
	bool found_fmt_chunk = false, no_extra_chunks = true;
	err_t err;
	memset(chunk_id, '\0', sizeof(chunk_id));
	while (!found_fmt_chunk)
	{
		x = fread(chunk_id, sizeof(char), 4, w->fp);
		if (!x) {
			return wav_read_eof_error_code_(w);
		}
		if (strncmp(chunk_id, wav_fmt_chunk_id, sizeof(chunk_id)) == 0)
		{
			found_fmt_chunk = true;
			break;
		}
		else if (strncmp(chunk_id, wav_data_chunk_id, sizeof(chunk_id)) == 0 || x != 4)
		{
			return E_CANNOT_FIND_FMT_CHUNK;
		}
		else
		{
			no_extra_chunks = false;
			err = wav_skip_chunk_anon_(w);
			if (err != E_OK)
			{
				return err;
			}
		}
	}
	if (!no_extra_chunks)
	{
		return E_EXTRA_CHUNKS;
	}
	return E_OK;
}

static err_t
wav_find_data_chunk_ (wav_handle *w)
{
	char chunk_id[5];
	int x;
	bool found_data_chunk = false, no_extra_chunks = true;
	err_t err;
	memset(chunk_id, '\0', sizeof(chunk_id));
	while (!found_data_chunk)
	{
		x = fread(chunk_id, sizeof(char), 4, w->fp);
		if (x < 4)
		{
			return wav_read_eof_error_code_(w);
		}
		if (strncmp(chunk_id, wav_data_chunk_id, sizeof(chunk_id)) == 0)
		{
			found_data_chunk = true;
			break;
		}
		else
		{
			no_extra_chunks = false;
			err = wav_skip_chunk_anon_(w);
			if (err != E_OK)
			{
				return err;
			}
		}
	}
	if (!no_extra_chunks)
	{
		return E_EXTRA_CHUNKS;
	}
	(void) x;
	return E_OK;
}

static err_t
wav_read_header_ (wav_handle *w)
{
	char chunk_id[5];
	int x;
	err_t err;
	
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	
	if (w->header == NULL)
	{
		w->header = calloc(1, sizeof(wav_file));
		if (w->header == NULL)
		{
			return E_MEM_ALLOC;
		}
	}
	
	x = fseek(w->fp, 0, SEEK_SET);
	if (x < 0)
	{
		return wav_read_eof_error_code_(w);
	}
	memset(chunk_id, '\0', sizeof(chunk_id));

	// RIFF magic ID
	x = fread(chunk_id, sizeof(char), 4, w->fp) == 4;
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	if (strncmp(chunk_id, riff_magic_id, sizeof(chunk_id)) != 0)
	{
		return E_NOT_A_RIFF_FILE;
	}

	// RIFF payload size
	x = fread(&w->header->riff_payload_length, sizeof(uint32_t), 1, w->fp);

	// WAVE magic ID
	x = x && fread(chunk_id, sizeof(char), 4, w->fp) == 4;
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	if (strncmp(chunk_id, wav_magic_id, sizeof(chunk_id)) != 0)
	{
		return E_NOT_A_WAVE_FILE;
	}

	// find fmt chunk

	w->no_extra_chunks = true;
	err = wav_find_fmt_chunk_(w);
	if (err != E_OK && err != E_EXTRA_CHUNKS)
	{
		return err;
	}
	if (err == E_EXTRA_CHUNKS)
	{
		w->no_extra_chunks = false;
	}
	
	err = wav_read_fmt_chunk_(w);
	if (err != E_OK)
	{
		return err;
	}
	
	// find data chunk
	err = wav_find_data_chunk_(w);
	if (err != E_OK && err != E_EXTRA_CHUNKS)
	{
		return err;
	}
	if (err == E_EXTRA_CHUNKS)
	{
		w->no_extra_chunks = false;
	}
	// data chunk length
	x = fread(&w->header->data_length, sizeof(uint32_t), 1, w->fp);
	if (!x)
	{
		return wav_read_eof_error_code_(w);
	}
	
	// file pointer is now positioned at the first data member
	w->header->data_offset_in_file = ftell(w->fp);
	
	(void) x;
	return E_OK;
}

static wav_handle *
wav_init_new_header_ (wav_handle *w, err_t *err_out)
{
	debug_assert(w != NULL);
	if (w->header == NULL)
	{
		w->header = calloc(1, sizeof(wav_file));
		if (w->header == NULL)
		{
			*err_out = E_MEM_ALLOC;
			return (wav_handle *)NULL;
		}
	}
	
	// Default (sane) parameters
	w->header->fmt_length = 16;
	
	w->header->fmt_content.fmt_type = 1;
	w->header->fmt_content.num_channels = 1;
	w->header->fmt_content.sample_rate = 8000;
	w->header->fmt_content.byte_rate = 8000;
	w->header->fmt_content.bytes_per_quantum = 1;
	w->header->fmt_content.bits_per_sample = 8;
	
	w->header->data_offset_in_file = 28 + w->header->fmt_length;
	// Assuming empty data block
	w->header->riff_payload_length = w->header->data_offset_in_file - 8;
	
	w->no_extra_chunks = true;
	
	*err_out = E_OK;
	return w;
}

static void
wav_recalculate_size_ (wav_handle *w)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	
	if (w->no_extra_chunks)
	{
		w->header->riff_payload_length = 12 + w->header->fmt_length + 8 + w->header->data_length;
	}
	else
	{
		w->header->riff_payload_length = w->header->data_length + w->header->data_offset_in_file - 8;
	}
}

static err_t
wav_write_header_inplace_ (wav_handle *w)
{
	err_t err;
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	
	fseek(w->fp, 0, SEEK_SET);
	fwrite(riff_magic_id, 4, 1, w->fp);
	fwrite(&w->header->riff_payload_length, sizeof(uint32_t), 1, w->fp);
	fwrite(wav_magic_id, 4, 1, w->fp);
	
	err = wav_find_fmt_chunk_(w);
	if (err != E_OK)
	{
		return err;
	}
	
	fseek(w->fp, -4, SEEK_CUR);
	fwrite(wav_fmt_chunk_id, 4, 1, w->fp);
	fwrite(&w->header->fmt_length, sizeof(uint32_t), 1, w->fp);
	
	fwrite(&w->header->fmt_content.fmt_type, sizeof(uint16_t), 1, w->fp);
	fwrite(&w->header->fmt_content.num_channels, sizeof(uint16_t), 1, w->fp);
	fwrite(&w->header->fmt_content.sample_rate, sizeof(uint32_t), 1, w->fp);
	fwrite(&w->header->fmt_content.byte_rate, sizeof(uint32_t), 1, w->fp);
	fwrite(&w->header->fmt_content.bytes_per_quantum, sizeof(uint16_t), 1, w->fp);
	fwrite(&w->header->fmt_content.bits_per_sample, sizeof(uint16_t), 1, w->fp);
	
	// don't write extra chunk for in-place modification
	
	fseek(w->fp, w->header->data_offset_in_file - 8, SEEK_SET);
	fwrite(wav_data_chunk_id, 4, 1, w->fp);
	fwrite(&w->header->data_length, sizeof(uint32_t), 1, w->fp);
	
	return E_OK;
}

/*
 * Writes out the 
 */
err_t
wav_write_header (wav_handle *w)
{
	err_t err = E_OK;
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	if (!w->write_mode)
	{
		return E_READ_ONLY;
	}
	
	wav_recalculate_size_(w);
	
	size_t oldseek = ftell(w->fp);
	
	if (!w->no_extra_chunks)
	{
		err = wav_write_header_inplace_(w);
	}
	else
	{
		fseek(w->fp, 0, SEEK_SET);
		fwrite(riff_magic_id, 4, 1, w->fp);
		fwrite(&w->header->riff_payload_length, sizeof(uint32_t), 1, w->fp);
		fwrite(wav_magic_id, 4, 1, w->fp);
		
		fwrite(wav_fmt_chunk_id, 4, 1, w->fp);
		fwrite(&w->header->fmt_length, sizeof(uint32_t), 1, w->fp);
		
		fwrite(&w->header->fmt_content.fmt_type, sizeof(uint16_t), 1, w->fp);
		fwrite(&w->header->fmt_content.num_channels, sizeof(uint16_t), 1, w->fp);
		fwrite(&w->header->fmt_content.sample_rate, sizeof(uint32_t), 1, w->fp);
		fwrite(&w->header->fmt_content.byte_rate, sizeof(uint32_t), 1, w->fp);
		fwrite(&w->header->fmt_content.bytes_per_quantum, sizeof(uint16_t), 1, w->fp);
		fwrite(&w->header->fmt_content.bits_per_sample, sizeof(uint16_t), 1, w->fp);
		
		if (w->header->fmt_length > 16 && !(w->header->fmt_ex_chunk && w->header->ssdpcm_extra_chunk))
		{
			int x, res;
			x = ftell(w->fp);
			res = fseek(w->fp, w->header->fmt_length - 16, SEEK_CUR);
			if (res == -1)
			{
				size_t i;
				fseek(w->fp, x, SEEK_SET);
				for (i = 16; i < w->header->fmt_length; i++)
				{
					fputc(0x00, w->fp);
				}
			}
		}
		else if (w->header->fmt_ex_chunk && w->header->ssdpcm_extra_chunk)
		{
			wave_format_ex *fmt_ex_chunk = w->header->fmt_ex_chunk;
			wav_ssdpcm_extra_chunk *ssdpcm_ex = w->header->ssdpcm_extra_chunk;

			fwrite(&w->header->extra_length, sizeof(uint16_t), 1, w->fp);
			
			fwrite(&fmt_ex_chunk->wfx_18_19, sizeof(uint16_t), 1, w->fp);
			fwrite(&fmt_ex_chunk->channel_mask, sizeof(uint32_t), 1, w->fp);
			fwrite(fmt_ex_chunk->sub_format, sizeof(char), 16, w->fp);
			
			fwrite(ssdpcm_data_chunk_id, 4, 1, w->fp);
			fwrite(ssdpcm_ex->mode_fourcc, sizeof(char), 4, w->fp);
			fwrite(&ssdpcm_ex->num_slopes, sizeof(uint8_t), 1, w->fp);
			fwrite(&ssdpcm_ex->bits_per_output_sample, sizeof(uint8_t), 1, w->fp);
			fwrite(&ssdpcm_ex->bytes_per_read_alignment, sizeof(uint8_t), 1, w->fp);
			fwrite(&ssdpcm_ex->has_reference_sample_on_every_block, 1, 1, w->fp);
			fwrite(&ssdpcm_ex->block_length, sizeof(uint16_t), 1, w->fp);
			fwrite(&ssdpcm_ex->bytes_per_block, sizeof(uint16_t), 1, w->fp);
		}
		fwrite(wav_data_chunk_id, 4, 1, w->fp);
		fwrite(&w->header->data_length, sizeof(uint32_t), 1, w->fp);
	}
	
	w->header_synced = true;
	fseek(w->fp, oldseek, SEEK_SET);
	return err;
}

wav_handle *
wav_alloc (err_t *err_out)
{
	wav_handle *result = calloc(1, sizeof(wav_handle));
	if (result == NULL)
	{
		*err_out = E_MEM_ALLOC;
	}
	else
	{
		*err_out = E_OK;
	}
	return result;
}

wav_handle *
wav_open (wav_handle *w, char *filename, wav_open_mode mode, err_t *err_out)
{
	char *file_mode;
	err_t err;
	
	debug_assert(w != NULL);
	if (w == NULL)
	{
		*err_out = E_NULLPTR;
		return (wav_handle *)NULL;
	}
	
	w->write_mode = (mode != W_READ);
	switch (mode)
	{
	case W_READ:
		file_mode = "rb";
		break;
	case W_WRITE:
		file_mode = "r+b";
		break;
	case W_CREATE:
		file_mode = "w+b";
		break;
	default:
		*err_out = E_INVALID_ARGUMENT;
		return (wav_handle *)NULL;
	}
	w->fp = fopen(filename, file_mode);
	if (w->fp == NULL)
	{
		*err_out = E_CANNOT_OPEN_FILE;
		return (wav_handle *)NULL;
	}
	
	if (mode != W_CREATE)
	{
		err = wav_read_header_(w);
		w->header_synced = true;
		if (err != E_OK)
		{
			if (mode == W_WRITE && err != E_MEM_ALLOC)
			{
				wav_handle *ret = wav_init_new_header_(w, &err);
				w->header_synced = false;
				if (ret == NULL)
				{
					*err_out = err;
					return (wav_handle *)NULL;
				}
			}
			else
			{
				*err_out = err;
				return (wav_handle *)NULL;
			}
		}
	}
	else
	{
		wav_handle *ret = wav_init_new_header_(w, &err);
		w->header_synced = false;
		if (ret == NULL)
		{
			*err_out = err;
			return (wav_handle *)NULL;
		}
	}
	
	*err_out = E_OK;
	return w;
}

wav_handle *
wav_close(wav_handle *w, err_t *err_out)
{
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	
	if (w == NULL)
	{
		*err_out = E_NULLPTR;
		return (wav_handle *)NULL;
	}
	
	*err_out = E_OK;
	if (w->fp != NULL)
	{
		if (w->header != NULL && !w->header_synced)
		{
			err_t err = wav_write_header(w);
			if (err != E_OK)
			{
				*err_out = err;
			}
		}
		fclose(w->fp);
		w->fp = NULL;
	}
	
	if (w->header != NULL)
	{
		if (w->header->fmt_ex_chunk != NULL)
		{
			free(w->header->fmt_ex_chunk);
			w->header->fmt_ex_chunk = NULL;
		}
		
		if (w->header->ssdpcm_extra_chunk != NULL)
		{
			free(w->header->ssdpcm_extra_chunk);
			w->header->ssdpcm_extra_chunk = NULL;
		}

		free(w->header);
		w->header = NULL;
	}
	
	w->write_mode = w->no_extra_chunks = w->header_synced = false;
	
	return w;
}

int64_t
wav_get_sizeof(wav_handle *w, int64_t num_samples)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	debug_assert(w->header->fmt_content.bytes_per_quantum != 0);
	if (w == NULL)
	{
		return 0;
	}
	return num_samples * w->header->fmt_content.bytes_per_quantum;
}

err_t
wav_set_format(wav_handle *w, wav_sample_fmt format)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		return E_NULLPTR;
	}
	
	w->header_synced = false;
	
	switch (format)
	{
	case W_U8:
		w->header->fmt_content.fmt_type = 1;
		w->header->fmt_content.bits_per_sample = 8;
		w->header->fmt_content.bytes_per_quantum = w->header->fmt_content.num_channels;
		w->header->fmt_content.byte_rate = w->header->fmt_content.bytes_per_quantum * w->header->fmt_content.sample_rate;
		break;
	case W_S16LE:
		w->header->fmt_content.fmt_type = 1;
		w->header->fmt_content.bits_per_sample = 16;
		w->header->fmt_content.bytes_per_quantum = 2 * w->header->fmt_content.num_channels;
		w->header->fmt_content.byte_rate = w->header->fmt_content.bytes_per_quantum * w->header->fmt_content.sample_rate;
		break;
	case W_SSDPCM:
		// WARNING: don't call wav_set_format directly with W_SSDPCM as argument!
		// call wav_init_ssdpcm instead!
		w->header->fmt_content.fmt_type = wave_format_ex_id;
		w->header->fmt_content.bits_per_sample = 0;
		w->header->fmt_content.bytes_per_quantum = 1;
		w->header->fmt_content.byte_rate = 0;
		break;
	default:
		return E_UNRECOGNIZED_FORMAT;
	}
	
	return E_OK;
}

wav_sample_fmt
wav_get_format(wav_handle *w, err_t *err_out)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		*err_out = E_NULLPTR;
		return W_ERROR;
	}

	*err_out = E_OK;

	if (w->header->fmt_content.fmt_type == 1
		&& w->header->fmt_content.bits_per_sample == 8
		&& w->header->fmt_content.bytes_per_quantum == 1 * w->header->fmt_content.num_channels)
	{
		return W_U8;
	}
	else if (w->header->fmt_content.fmt_type == 1
		&& w->header->fmt_content.bits_per_sample == 16
		&& w->header->fmt_content.bytes_per_quantum == 2 * w->header->fmt_content.num_channels)
	{
		return W_S16LE;
	}
	else if (w->header->fmt_content.fmt_type == wave_format_ex_id
		&& w->header->fmt_ex_chunk != NULL
		&& memcmp(w->header->fmt_ex_chunk->sub_format, ssdpcm_codec_guid, 16) == 0)
	{
		return W_SSDPCM;
	}
	else if (w->header->fmt_content.fmt_type == wave_format_ex_id
		&& w->header->fmt_ex_chunk != NULL
		&& memcmp(w->header->fmt_ex_chunk->sub_format, waveformatext_pcm_codec_guid, 16) == 0
		&& w->header->fmt_content.bits_per_sample == 8
		&& w->header->fmt_content.bytes_per_quantum == 1 * w->header->fmt_content.num_channels)
	{
		return W_U8;
	}
	else if (w->header->fmt_content.fmt_type == wave_format_ex_id
		&& w->header->fmt_ex_chunk != NULL
		&& memcmp(w->header->fmt_ex_chunk->sub_format, waveformatext_pcm_codec_guid, 16) == 0
		&& w->header->fmt_content.bits_per_sample == 16
		&& w->header->fmt_content.bytes_per_quantum == 2 * w->header->fmt_content.num_channels)
	{
		return W_S16LE;
	}
	else
	{
		*err_out = E_UNRECOGNIZED_FORMAT;
		return W_ERROR;
	}
}

err_t
wav_set_sample_rate(wav_handle *w, uint32_t sample_rate)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		return E_NULLPTR;
	}
	
	w->header->fmt_content.sample_rate = sample_rate;
	if (!w->header->ssdpcm_extra_chunk)
	{
		w->header->fmt_content.byte_rate = w->header->fmt_content.bytes_per_quantum * w->header->fmt_content.sample_rate;
	}
	else
	{
		w->header->fmt_content.byte_rate = w->header->fmt_content.sample_rate * w->header->ssdpcm_extra_chunk->bytes_per_block * w->header->fmt_content.num_channels / w->header->ssdpcm_extra_chunk->block_length;
	}
	
	return E_OK;
}

uint32_t
wav_get_sample_rate(wav_handle *w)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		return 0;
	}
	
	return w->header->fmt_content.sample_rate;
}

uint8_t
wav_get_num_channels(wav_handle *w, err_t *err_out)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		*err_out = E_NULLPTR;
		return 0;
	}
	
	return w->header->fmt_content.num_channels;
}

err_t
wav_set_num_channels(wav_handle *w, uint8_t num_channels)
{
	debug_assert(w != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		return E_NULLPTR;
	}
	
	err_t err;
	wav_sample_fmt f = wav_get_format(w, &err);
	if (err != E_OK)
	{
		return err;
	}

	w->header->fmt_content.num_channels = num_channels;
	err = wav_set_format(w, f);
	debug_assert(err == E_OK);

	return err;
}

err_t
wav_seek(wav_handle *w, int64_t quantum_offset, int whence)
{
	int res;
	size_t byte_offset, initial_pos, final_pos;
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	if (w == NULL)
	{
		return E_NULLPTR;
	}
	
	byte_offset = wav_get_sizeof(w, quantum_offset);
	initial_pos = ftell(w->fp);
	switch (whence)
	{
	case SEEK_SET:
		res = fseek(w->fp, w->header->data_offset_in_file + byte_offset, SEEK_SET);
		break;
	case SEEK_CUR:
		res = fseek(w->fp, byte_offset, SEEK_CUR);
		break;
	case SEEK_END:
		res = fseek(w->fp, w->header->data_offset_in_file + w->header->data_length + byte_offset, SEEK_SET);
		break;
	default:
		return E_INVALID_ARGUMENT;
	}
	if (res == -1)
	{
		if (errno == EINVAL)
		{
			return E_INVALID_OFFSET;
		}
		else
		{
			return E_FILE_NOT_SEEKABLE;
		}
	}
	final_pos = ftell(w->fp);
	if (final_pos < w->header->data_offset_in_file)
	{
		fseek(w->fp, initial_pos, SEEK_SET);
		return E_INVALID_OFFSET;
	}
	
	return E_OK;
}

long
wav_tell(wav_handle *w)
{
	size_t byte_offset;

	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	debug_assert(w->header->fmt_content.bytes_per_quantum != 0);
	if (w == NULL)
	{
		return -1;
	}
	
	byte_offset = ftell(w->fp);
	byte_offset -= w->header->data_offset_in_file;
	debug_assert((byte_offset % w->header->fmt_content.bytes_per_quantum) == 0);
	return byte_offset / w->header->fmt_content.bytes_per_quantum;
}


static long
wav_tell_bytes(wav_handle *w)
{
	size_t byte_offset;

	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	if (w == NULL)
	{
		return -1;
	}
	
	byte_offset = ftell(w->fp);
	byte_offset -= w->header->data_offset_in_file;
	return byte_offset;
}


long
wav_read(wav_handle *w, void *dest, size_t num_samples, err_t *err_out)
{
	int64_t amt_to_read, amt_we_can_read, actually_read;
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	debug_assert(w->header->fmt_content.bytes_per_quantum != 0);
	if (w == NULL)
	{
		*err_out = E_NULLPTR;
		return -1;
	}
	
	amt_we_can_read = w->header->data_length - wav_tell_bytes(w);
	amt_to_read = wav_get_sizeof(w, num_samples);
	actually_read = fread(dest, 1, amt_to_read, w->fp);
	if (actually_read != amt_to_read)
	{
		clearerr(w->fp);
		if (amt_to_read > amt_we_can_read)
		{
			*err_out = E_END_OF_STREAM;
		}
		else
		{
			*err_out = E_PREMATURE_END_OF_FILE;
		}
	}
	else
	{
		*err_out = E_OK;
	}
	
	return actually_read / w->header->fmt_content.bytes_per_quantum;
}

long
wav_write(wav_handle *w, void *src, size_t num_samples, int64_t offset, err_t *err_out)
{
	size_t amt_to_write, actually_written;
	int64_t initial_offset, final_offset;
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	debug_assert(w->header->fmt_content.bytes_per_quantum != 0);
	if (w == NULL)
	{
		*err_out = E_NULLPTR;
		return -1;
	}
	
	amt_to_write = wav_get_sizeof(w, num_samples);
	initial_offset = wav_tell_bytes(w);
	if (initial_offset < 0)
	{
		wav_seek(w, 0, SEEK_SET);
		initial_offset = 0;
	}
	
	if (offset >= 0)
	{
		wav_seek(w, offset, SEEK_SET);
		initial_offset = wav_tell_bytes(w);
	}
	
	final_offset = initial_offset + amt_to_write;
	
	if (final_offset > w->header->data_length)
	{
		w->header->data_length = final_offset;
		w->header_synced = false;
	}
	
	actually_written = fwrite(src, 1, amt_to_write, w->fp);
	if (actually_written != amt_to_write)
	{
		clearerr(w->fp);
		*err_out = E_WRITE_ERROR;
	}
	else
	{
		*err_out = E_OK;
	}
	
	debug_assert((actually_written % w->header->fmt_content.bytes_per_quantum) == 0);
	return actually_written / w->header->fmt_content.bytes_per_quantum;
}

err_t
wav_init_ssdpcm(wav_handle *w, wav_sample_fmt format, ssdpcm_block_mode mode, uint16_t block_length, bool has_reference_sample)
{
	if (w == NULL || w->header == NULL)
	{
		return E_NULLPTR;
	}
	
	w->header_synced = false;
	
	wav_set_format(w, W_SSDPCM);
	
	if (w->header->fmt_ex_chunk == NULL)
	{
		w->header->fmt_ex_chunk = calloc(1, sizeof(wave_format_ex));
		if (w->header->fmt_ex_chunk == NULL)
		{
			return E_MEM_ALLOC;
		}
	}
	w->header->fmt_ex_chunk->wfx_18_19.samples_per_block = block_length;
	w->header->fmt_ex_chunk->channel_mask = w->header->fmt_content.num_channels == 2 ? 0x03 : 0x04;
	memcpy(w->header->fmt_ex_chunk->sub_format, ssdpcm_codec_guid, 16);
	
	w->header->ssdpcm_extra_chunk = calloc(1, sizeof(wav_ssdpcm_extra_chunk));
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		w->header->ssdpcm_extra_chunk = calloc(1, sizeof(wav_ssdpcm_extra_chunk));
		if (w->header->ssdpcm_extra_chunk == NULL)
		{
			return E_MEM_ALLOC;
		}
	}
	w->header->extra_length = 16 + 22;
	w->header->fmt_length += w->header->extra_length + sizeof(uint16_t);
	w->header->data_offset_in_file = w->header->fmt_length + 20 + 8;
	
	wav_ssdpcm_extra_chunk *ssdpcm_ex = w->header->ssdpcm_extra_chunk;
	size_t block_header_data_size;

	switch (format)
	{
	case W_U8:
		ssdpcm_ex->bits_per_output_sample = 8;
		break;
	case W_S16LE:
		ssdpcm_ex->bits_per_output_sample = 16;
		break;
	default:
		return E_INVALID_ARGUMENT;
	}

	ssdpcm_ex->block_length = block_length;
	ssdpcm_ex->has_reference_sample_on_every_block = has_reference_sample;

	switch (mode)
	{
	case SS_SS1:
		ssdpcm_ex->num_slopes = 2;
		ssdpcm_ex->bytes_per_read_alignment = 1;

		block_header_data_size = (ssdpcm_ex->bits_per_output_sample / 8) * (ssdpcm_ex->num_slopes / 2);
		ssdpcm_ex->bytes_per_block = (ssdpcm_ex->block_length + 7) / 8 + block_header_data_size;
		break;
	case SS_SS1C:
		ssdpcm_ex->num_slopes = 2;
		ssdpcm_ex->bytes_per_read_alignment = 1;

		block_header_data_size = (ssdpcm_ex->bits_per_output_sample / 8) * (ssdpcm_ex->num_slopes / 2);
		ssdpcm_ex->bytes_per_block = (ssdpcm_ex->block_length + 7) / 8 + block_header_data_size;
		break;
	case SS_SS1_6:
		ssdpcm_ex->num_slopes = 3;
		ssdpcm_ex->bytes_per_read_alignment = 1;

		block_header_data_size = (ssdpcm_ex->bits_per_output_sample / 8) * (ssdpcm_ex->num_slopes / 2);
		ssdpcm_ex->bytes_per_block = (ssdpcm_ex->block_length + 4) / 5 + block_header_data_size;
		break;
	case SS_SS2:
		ssdpcm_ex->num_slopes = 4;
		ssdpcm_ex->bytes_per_read_alignment = 1;

		block_header_data_size = (ssdpcm_ex->bits_per_output_sample / 8) * (ssdpcm_ex->num_slopes / 2);
		ssdpcm_ex->bytes_per_block = (ssdpcm_ex->block_length + 3) / 4 + block_header_data_size;
		break;
	case SS_SS2_3:
		ssdpcm_ex->num_slopes = 5;
		ssdpcm_ex->bytes_per_read_alignment = 7;

		block_header_data_size = (ssdpcm_ex->bits_per_output_sample / 8) * (ssdpcm_ex->num_slopes / 2);
		ssdpcm_ex->bytes_per_block = (ssdpcm_ex->block_length * 7 + 23) / 24 + block_header_data_size;
		break;
	case SS_SS3:
		ssdpcm_ex->num_slopes = 8;
		ssdpcm_ex->bytes_per_read_alignment = 3;

		block_header_data_size = (ssdpcm_ex->bits_per_output_sample / 8) * (ssdpcm_ex->num_slopes / 2);
		ssdpcm_ex->bytes_per_block = (ssdpcm_ex->block_length * 3 + 7) / 8 + block_header_data_size;
		break;
	default:
		return E_INVALID_ARGUMENT;
	}

	w->header->fmt_content.byte_rate = w->header->fmt_content.sample_rate * ssdpcm_ex->bytes_per_block * w->header->fmt_content.num_channels / ssdpcm_ex->block_length;
	w->header->fmt_content.bytes_per_quantum = (ssdpcm_ex->bytes_per_block + (has_reference_sample ? (ssdpcm_ex->bits_per_output_sample / 8) : 0)) * w->header->fmt_content.num_channels;
	
	assert(mode >= 0 && mode < NUM_SSDPCM_MODES);
	assert(*ssdpcm_mode_fourcc_list[mode] != '\0' || "Unregistered mode fourcc");
	memcpy(ssdpcm_ex->mode_fourcc, ssdpcm_mode_fourcc_list[mode], 4);
	
	return E_OK;
}

err_t
wav_write_ssdpcm_block(wav_handle *w, void *reference, void *slopes, void *code, int64_t index, uint16_t channel_idx)
{
	if (w == NULL || w->header == NULL)
	{
		return E_NULLPTR;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		return E_NOT_A_SSDPCM_WAV;
	}
	
	wav_ssdpcm_extra_chunk *ssdpcm_ex = w->header->ssdpcm_extra_chunk;
	size_t sample_size_bytes = ssdpcm_ex->bits_per_output_sample / 8;
	size_t block_header_data_size = sample_size_bytes * (ssdpcm_ex->num_slopes / 2);
	size_t code_size = ssdpcm_ex->bytes_per_block - block_header_data_size;
	uint16_t num_channels = w->header->fmt_content.num_channels;
	size_t actually_written;
	int64_t initial_offset, final_offset;
	err_t err;
	
	initial_offset = wav_tell_bytes(w);
	if (initial_offset < 0)
	{
		wav_seek(w, 0, SEEK_SET);
		initial_offset = 0;
	}
	
	if (index >= 0)
	{
		err = wav_seek(w, index, SEEK_SET);
		if (err != E_OK)
		{
			return err;
		}
		if (channel_idx > 0)
		{
			int64_t amt_to_seek = ssdpcm_ex->bytes_per_block * channel_idx;
			if (ssdpcm_ex->has_reference_sample_on_every_block)
			{
				amt_to_seek += sample_size_bytes * (num_channels);
			}
			err = fseek(w->fp, amt_to_seek, SEEK_CUR);
			if (err != E_OK)
			{
				return err;
			}
		}
		initial_offset = wav_tell_bytes(w);
	}
	
	if ((ssdpcm_ex->has_reference_sample_on_every_block && (channel_idx % num_channels) == 0)
		|| wav_tell_bytes(w) == 0)
	{
		actually_written = fwrite(reference, sample_size_bytes, num_channels, w->fp);
		if (actually_written != num_channels)
		{
			return E_WRITE_ERROR;
		}
	}

	actually_written = fwrite(slopes, sample_size_bytes, ssdpcm_ex->num_slopes / 2, w->fp);
	if (actually_written != (ssdpcm_ex->num_slopes / 2))
	{
		return E_WRITE_ERROR;
	}
	actually_written = fwrite(code, 1, code_size, w->fp);
	if (actually_written != code_size)
	{
		return E_WRITE_ERROR;
	}
	
	final_offset = wav_tell_bytes(w);
	if (final_offset > w->header->data_length)
	{
		w->header->data_length = final_offset;
		w->header_synced = false;
	}
	
	return E_OK;
}

wav_sample_fmt
wav_get_ssdpcm_output_format(wav_handle *w, err_t *err_out)
{
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return W_ERROR;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return W_ERROR;
	}
	
	*err_out = E_OK;
	switch (w->header->ssdpcm_extra_chunk->bits_per_output_sample)
	{
	case 8:
		return W_U8;
	case 16:
		return W_S16LE;
	default:
		*err_out = E_UNSUPPORTED_BITS_PER_SAMPLE;
		return W_ERROR;
	}
}

err_t
wav_read_ssdpcm_block(wav_handle *w, void *reference, void *slopes, void *code, uint16_t channel_idx)
{
	if (w == NULL || w->header == NULL)
	{
		return E_NULLPTR;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		return E_NOT_A_SSDPCM_WAV;
	}
	
	wav_ssdpcm_extra_chunk *ssdpcm_ex = w->header->ssdpcm_extra_chunk;
	size_t sample_size_bytes = ssdpcm_ex->bits_per_output_sample / 8;
	size_t block_header_data_size = sample_size_bytes * (ssdpcm_ex->num_slopes / 2);
	int64_t code_size = ssdpcm_ex->bytes_per_block - block_header_data_size;
	uint16_t num_channels = w->header->fmt_content.num_channels;
	int64_t amt_to_read, amt_we_can_read, actually_read;
	
	amt_we_can_read = w->header->data_length - wav_tell_bytes(w);
	if (amt_we_can_read == 0)
	{
		return E_END_OF_STREAM;
	}
	
	if ((ssdpcm_ex->has_reference_sample_on_every_block && (channel_idx % num_channels) == 0)
		|| wav_tell_bytes(w) == 0)
	{
		amt_to_read = sample_size_bytes;
		actually_read = fread(reference, sample_size_bytes, num_channels, w->fp);
		if (actually_read != num_channels)
		{
			if (amt_to_read > amt_we_can_read)
			{
				clearerr(w->fp);
				return E_END_OF_STREAM;
			}
			else
			{
				return E_PREMATURE_END_OF_FILE;
			}
		}
	}

	actually_read = fread(slopes, sample_size_bytes, ssdpcm_ex->num_slopes / 2, w->fp);
	if (actually_read != ssdpcm_ex->num_slopes / 2)
	{
		// File ended in the middle of a block
		fprintf(stderr, "\namt_we_can_read: %ld\n", amt_we_can_read);
		return E_PREMATURE_END_OF_FILE;
	}

	actually_read = fread(code, 1, code_size, w->fp);
	if (actually_read != code_size)
	{
		// File ended in the middle of a block
		fprintf(stderr, "\namt_we_can_read: %ld\n", amt_we_can_read);
		return E_PREMATURE_END_OF_FILE;
	}
	
	return E_OK;
}

uint16_t
wav_get_ssdpcm_block_length(wav_handle *w, err_t *err_out)
{
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return 0;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return 0;
	}
	
	*err_out = E_OK;
	return w->header->ssdpcm_extra_chunk->block_length;
}

uint16_t
wav_get_ssdpcm_total_bytes_per_block(wav_handle *w, err_t *err_out)
{
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return 0;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return 0;
	}
	
	*err_out = E_OK;
	return w->header->ssdpcm_extra_chunk->bytes_per_block;
}

uint16_t
wav_get_ssdpcm_code_bytes_per_block(wav_handle *w, err_t *err_out)
{
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return 0;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return 0;
	}
	
	*err_out = E_OK;
	wav_ssdpcm_extra_chunk *ssdpcm_ex = w->header->ssdpcm_extra_chunk;
	size_t sample_size_bytes = ssdpcm_ex->bits_per_output_sample / 8;
	size_t block_header_data_size = sample_size_bytes * (ssdpcm_ex->num_slopes / 2 + (ssdpcm_ex->has_reference_sample_on_every_block ? 1 : 0));
	return ssdpcm_ex->bytes_per_block - block_header_data_size;
}

uint8_t
wav_get_ssdpcm_num_slopes(wav_handle *w, err_t *err_out)
{
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return 0;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return 0;
	}
	
	*err_out = E_OK;
	return w->header->ssdpcm_extra_chunk->num_slopes;
}

bool
wav_ssdpcm_has_reference_sample_on_every_block(wav_handle *w, err_t *err_out)
{
	if (w == NULL || w->header == NULL)
	{
		*err_out = E_NULLPTR;
		return 0;
	}
	if (w->header->ssdpcm_extra_chunk == NULL)
	{
		*err_out = E_NOT_A_SSDPCM_WAV;
		return 0;
	}
	
	return w->header->ssdpcm_extra_chunk->has_reference_sample_on_every_block;
}

err_t
wav_set_data_length(wav_handle *w, uint32_t num_samples)
{
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	debug_assert(w->header->fmt_content.bytes_per_quantum != 0);
	if (w == NULL)
	{
		return E_NULLPTR;
	}
	
	w->header->data_length = num_samples * w->header->fmt_content.bytes_per_quantum;
	
	return E_OK;
}
