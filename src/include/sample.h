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

#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include "types.h"

void sample_decode_s16 (sample_t *dest, int16_t *src, size_t num_samples);
void sample_encode_s16 (int16_t *dest, sample_t *src, size_t num_samples);
void sample_decode_s16_multichannel (sample_t **dest, int16_t *src, size_t num_samples, size_t num_channels);
void sample_encode_s16_multichannel (int16_t *dest, sample_t **src, size_t num_samples, size_t num_channels);

void sample_decode_u16 (sample_t *dest, uint16_t *src, size_t num_samples);
void sample_encode_u16 (uint16_t *dest, sample_t *src, size_t num_samples);

void sample_decode_u8 (sample_t *dest, uint8_t *src, size_t num_samples);
void sample_encode_u8_overflow (uint8_t *dest, sample_t *src, size_t num_samples);
void sample_encode_u8_clamp (uint8_t *dest, sample_t *src, size_t num_samples);
void sample_decode_u8_multichannel (sample_t **dest, uint8_t *src, size_t num_samples, size_t num_channels);
void sample_encode_u8_overflow_multichannel (uint8_t *dest, sample_t **src, size_t num_samples, size_t num_channels);

void sample_convert_u8_to_s16 (int16_t *dest, uint8_t *src, size_t num_samples);
void sample_convert_s16_to_u8 (uint8_t *dest, int16_t *src, size_t num_samples);

void sample_convert_u8_to_u7 (uint8_t *dest, uint8_t *src, size_t num_samples);
void sample_convert_u7_to_u8 (uint8_t *dest, uint8_t *src, size_t num_samples);

void sample_filter_comb (sample_t *dest, size_t num_samples, sample_t starting_sample);

void sample_dither_triangular (sample_t **dest, sample_t **src, size_t num_samples, size_t num_channels, uint8_t strength, sample_t clamp_low, sample_t clamp_high);

#endif // #ifndef __SAMPLE_H__
