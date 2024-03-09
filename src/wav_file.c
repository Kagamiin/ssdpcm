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

typedef struct
{
	uint16_t fmt_type;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t bytes_per_quantum;
	uint16_t bits_per_sample;
} wav_fmt_chunk;

typedef struct
{
	uint32_t riff_payload_length;
	uint32_t fmt_length;
	wav_fmt_chunk fmt_content;
	uint32_t data_length;
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

static err_t
wav_read_fmt_chunk_ (wav_handle *w)
{
	int x;
	
	debug_assert(w != NULL);
	debug_assert(w->fp != NULL);
	debug_assert(w->header != NULL);
	
	// fmt chunk length
	x = fread(&w->header->fmt_length, sizeof(uint32_t), 1, w->fp);
	debug_assert(x == 1);
	if (w->header->fmt_length < 16)
	{
		return E_FMT_CHUNK_TOO_SMALL;
	}
	fread(&w->header->fmt_content.fmt_type, sizeof(uint16_t), 1, w->fp);
	fread(&w->header->fmt_content.num_channels, sizeof(uint16_t), 1, w->fp);
	fread(&w->header->fmt_content.sample_rate, sizeof(uint32_t), 1, w->fp);
	fread(&w->header->fmt_content.byte_rate, sizeof(uint32_t), 1, w->fp);
	fread(&w->header->fmt_content.bytes_per_quantum, sizeof(uint16_t), 1, w->fp);
	fread(&w->header->fmt_content.bits_per_sample, sizeof(uint16_t), 1, w->fp);
	if (w->header->fmt_length > 16)
	{
		x = fseek(w->fp, w->header->fmt_length - 16, SEEK_CUR);
		debug_assert(x != -1);
	}
	
	{
		wav_fmt_chunk *chunk = &w->header->fmt_content;
		if (chunk->fmt_type != 1)
		{
			return E_UNRECOGNIZED_FORMAT;
		}
		if (chunk->num_channels != 1)
		{
			return E_ONLY_MONO_SUPPORTED;
		}
		if ((chunk->sample_rate * chunk->bits_per_sample * chunk->num_channels / 8) != chunk->byte_rate)
		{
			return E_MISMATCHED_RATES;
		}
		if ((chunk->bits_per_sample * chunk->num_channels / 8) != chunk->bytes_per_quantum)
		{
			return E_MISMATCHED_BLOCK_SIZE;
		}
		if (chunk->bits_per_sample != 8 && chunk->bits_per_sample != 16)
		{
			return E_UNSUPPORTED_BITS_PER_SAMPLE;
		}
	}
	
	(void) x;
	return E_OK;
}

static err_t
wav_skip_chunk_anon_ (wav_handle *w)
{
	int x;
	uint32_t chunk_length;
	x = fread(&chunk_length, sizeof(uint32_t), 1, w->fp);
	debug_assert(x == 1);
	x = fseek(w->fp, chunk_length, SEEK_CUR);
	
	if (x == -1)
	{
		if (feof(w->fp))
		{
			return E_PREMATURE_END_OF_FILE;
		}
		else if (ferror(w->fp))
		{
			return E_READ_ERROR;
		}
		else
		{
			return E_UNKNOWN_ERROR;
		}
	}
	
	(void) x;
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
		//debug_assert(x == 4);
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
		debug_assert(x == 4);
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
		w->header = malloc(sizeof(wav_file));
		if (w->header == NULL)
		{
			return E_MEM_ALLOC;
		}
	}
	
	x = fseek(w->fp, 0, SEEK_SET);
	debug_assert(x != -1);
	memset(chunk_id, '\0', sizeof(chunk_id));

	// RIFF magic ID
	x = fread(chunk_id, sizeof(char), 4, w->fp);
	debug_assert(x == 4);
	if (strncmp(chunk_id, riff_magic_id, sizeof(chunk_id)) != 0)
	{
		return E_NOT_A_RIFF_FILE;
	}
	// RIFF payload size
	x = fread(&w->header->riff_payload_length, sizeof(uint32_t), 1, w->fp);
	debug_assert(x == 1);

	// WAVE magic ID
	x = fread(chunk_id, sizeof(char), 4, w->fp);
	debug_assert(x == 4);
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
	debug_assert(x == 1);
	
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
		
		if (w->header->fmt_length > 16)
		{
			int x, res;
			x = ftell(w->fp);
			res = fseek(w->fp, w->header->fmt_length - 16, SEEK_CUR);
			if (res == -1)
			{
				size_t i;
				fseek(w->fp, x, SEEK_SET);
				for (i = 0; i < w->header->fmt_length - 16; i++)
				{
					fputc(0x00, w->fp);
				}
			}
		}
		fwrite(wav_data_chunk_id, 4, 1, w->fp);
		fwrite(&w->header->data_length, sizeof(uint32_t), 1, w->fp);
	}
	
	w->header_synced = true;
	fseek(w->fp, oldseek, SEEK_SET);
	return err;
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
		return -1;
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
	else
	{
		*err_out = E_UNRECOGNIZED_FORMAT;
		return -1;
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
	w->header->fmt_content.byte_rate = w->header->fmt_content.bytes_per_quantum * w->header->fmt_content.sample_rate;
	
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
	
	amt_we_can_read = w->header->data_length - (wav_tell(w) * w->header->fmt_content.bytes_per_quantum);
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
wav_write(wav_handle *w, void *src, size_t num_samples, err_t *err_out)
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
	initial_offset = wav_tell(w) * w->header->fmt_content.bytes_per_quantum;
	if (initial_offset < 0)
	{
		wav_seek(w, 0, SEEK_SET);
		initial_offset = 0;
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
