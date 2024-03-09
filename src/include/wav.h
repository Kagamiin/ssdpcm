#ifndef __WAV_H__
#define __WAV_H__
#include "types.h"
#include "errors.h"

typedef void wav_handle;

wav_handle *wav_open(wav_handle *w, char *filename, wav_open_mode mode, err_t *err_out);
wav_handle *wav_close(wav_handle *w, err_t *err_out);
long wav_read(wav_handle *w, void *dest, size_t num_samples, err_t *err_out);
long wav_write(wav_handle *w, void *src, size_t num_samples, err_t *err_out);
err_t wav_write_header (wav_handle *w);
err_t wav_seek(wav_handle *w, int64_t quantum_offset, int whence);
long wav_tell(wav_handle *w);
wav_sample_fmt wav_get_format(wav_handle *w, err_t *err_out);
err_t wav_set_format(wav_handle *w, wav_sample_fmt format);
uint32_t wav_get_sample_rate(wav_handle *w);
err_t wav_set_sample_rate(wav_handle *w, uint32_t sample_rate);
err_t wav_set_data_length(wav_handle *w, uint32_t num_samples);
int64_t wav_get_sizeof(wav_handle *w, int64_t num_samples);

#endif
