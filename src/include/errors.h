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
};

#endif
