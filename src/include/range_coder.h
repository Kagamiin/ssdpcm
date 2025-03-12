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

#ifndef __RANGE_CODER_H__
#define __RANGE_CODER_H__

#include "types.h"

void range_encode_ss1_6(codeword_t *input, uint8_t *output, size_t num_words);
void range_decode_ss1_6(uint8_t *input, codeword_t *output, size_t num_bytes);

void range_encode_ss2_3(codeword_t *input, uint8_t *output, size_t num_words);
void range_decode_ss2_3(uint8_t *input, codeword_t *output, size_t num_bytes);

void range_encode_ss3(codeword_t *input, uint8_t *output, size_t num_words);
void range_decode_ss3(uint8_t *input, codeword_t *output, size_t num_bytes);

#endif
