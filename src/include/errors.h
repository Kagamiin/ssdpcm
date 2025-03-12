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

#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef int err_t;

#define FAIL_ON_ERR(something)\
{\
	err_t err = something;\
	if (err)\
	{\
		return err;\
	}\
}

#define IF_OK(what, then_that)\
{\
	err_t err = what;\
	if (!err)\
	{\
		then_that;\
	}\
}

#ifdef _DEBUG
#define debug_assert(x) assert(x)
#else
#define debug_assert(x)
#endif // #ifdef _DEBUG

enum
{
	E_OK,
	E_END_OF_STREAM,
	E_NOT_IMPLEMENTED,
	E_NULLPTR,
	E_MEM_ALLOC,
	E_CANNOT_OPEN_FILE,
	E_NOT_A_RIFF_FILE,
	E_NOT_A_WAVE_FILE,
	E_CANNOT_FIND_FMT_CHUNK,
	E_FMT_CHUNK_TOO_SMALL,
	E_CANNOT_FIND_DATA_CHUNK,
	E_PREMATURE_END_OF_FILE,
	E_READ_ERROR,
	E_UNKNOWN_ERROR,
	E_EXTRA_CHUNKS,
	E_READ_ONLY,
	E_UNRECOGNIZED_FORMAT,
	E_ONLY_MONO_SUPPORTED,
	E_MISMATCHED_RATES,
	E_MISMATCHED_BLOCK_SIZE,
	E_UNSUPPORTED_BITS_PER_SAMPLE,
	E_INVALID_OFFSET,
	E_FILE_NOT_SEEKABLE,
	E_WRITE_ERROR,
	E_INVALID_ARGUMENT,
	E_UNRECOGNIZED_SUBFORMAT,
	E_INVALID_SUBHEADER,
	E_NOT_A_SSDPCM_WAV,
	E_UNRECOGNIZED_MODE,
	E_TOO_MANY_SLOPES,

	ERROR_CODES_LENGTH, // don't remove
};

extern const char *error_enum_strs[ERROR_CODES_LENGTH];

#endif
