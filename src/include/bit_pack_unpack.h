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

#ifndef __BIT_PACK_UNPACK_H__
#define __BIT_PACK_UNPACK_H__

#include "types.h"
#include "errors.h"

err_t put_bits_msbfirst (bitstream_buffer *buf, codeword_t src, uint8_t num_bits);
err_t get_bits_msbfirst (codeword_t *dest, bitstream_buffer *buf, uint8_t num_bits);

// TODO: implement these below
err_t put_bits_lsbfirst (bitstream_buffer *buf, codeword_t src, uint8_t num_bits);
err_t get_bits_lsbfirst (codeword_t *dest, bitstream_buffer *buf, uint8_t num_bits);

#endif
