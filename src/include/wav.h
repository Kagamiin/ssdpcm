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

#ifndef __WAV_H__
#define __WAV_H__
#include "types.h"
#include "errors.h"

typedef void wav_handle;

wav_handle *wav_alloc(err_t *err_out);
wav_handle *wav_open(wav_handle *w, char *filename, wav_open_mode mode, err_t *err_out);
wav_handle *wav_close(wav_handle *w, err_t *err_out);
long wav_read(wav_handle *w, void *dest, size_t num_samples, err_t *err_out);
long wav_write(wav_handle *w, void *src, size_t num_samples, int64_t offset, err_t *err_out);
err_t wav_write_header (wav_handle *w);
err_t wav_seek(wav_handle *w, int64_t quantum_offset, int whence);
long wav_tell(wav_handle *w);
wav_sample_fmt wav_get_format(wav_handle *w, err_t *err_out);
err_t wav_set_format(wav_handle *w, wav_sample_fmt format);
uint32_t wav_get_sample_rate(wav_handle *w);
err_t wav_set_sample_rate(wav_handle *w, uint32_t sample_rate);
err_t wav_set_data_length(wav_handle *w, uint32_t num_samples);
int64_t wav_get_sizeof(wav_handle *w, int64_t num_samples);
uint8_t wav_get_num_channels(wav_handle *w, err_t *err_out);
err_t wav_set_num_channels(wav_handle *w, uint8_t num_channels);

err_t wav_init_ssdpcm(wav_handle *w, wav_sample_fmt format, ssdpcm_block_mode mode, uint16_t block_length, bool has_reference_sample);
ssdpcm_block_mode wav_get_ssdpcm_mode(wav_handle *w, err_t *err_out);
uint16_t wav_get_ssdpcm_block_length(wav_handle *w, err_t *err_out);
uint16_t wav_get_ssdpcm_total_bytes_per_block(wav_handle *w, err_t *err_out);
uint16_t wav_get_ssdpcm_code_bytes_per_block(wav_handle *w, err_t *err_out);
uint8_t wav_get_ssdpcm_num_slopes(wav_handle *w, err_t *err_out);
err_t wav_write_ssdpcm_block(wav_handle *w, void *reference, void *slopes, void *code, int64_t index, uint16_t channel_idx);
err_t wav_read_ssdpcm_block(wav_handle *w, void *reference, void *slopes, void *code, uint16_t channel_idx);
wav_sample_fmt wav_get_ssdpcm_output_format(wav_handle *w, err_t *err_out);
bool wav_ssdpcm_has_reference_sample_on_every_block(wav_handle *w, err_t *err_out);

#endif
