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
