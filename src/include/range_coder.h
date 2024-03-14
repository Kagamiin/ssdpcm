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
