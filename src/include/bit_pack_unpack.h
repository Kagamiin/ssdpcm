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
