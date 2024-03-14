#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#include "types.h"

void sample_decode_s16 (sample_t *dest, int16_t *src, size_t num_samples);
void sample_encode_s16 (int16_t *dest, sample_t *src, size_t num_samples);

void sample_decode_u16 (sample_t *dest, uint16_t *src, size_t num_samples);
void sample_encode_u16 (uint16_t *dest, sample_t *src, size_t num_samples);

void sample_decode_u8 (sample_t *dest, uint8_t *src, size_t num_samples);
void sample_encode_u8_overflow (uint8_t *dest, sample_t *src, size_t num_samples);
void sample_encode_u8_clamp (uint8_t *dest, sample_t *src, size_t num_samples);

void sample_convert_u8_to_s16 (int16_t *dest, uint8_t *src, size_t num_samples);
void sample_convert_s16_to_u8 (uint8_t *dest, int16_t *src, size_t num_samples);

void sample_convert_u8_to_u7 (uint8_t *dest, uint8_t *src, size_t num_samples);
void sample_convert_u7_to_u8 (uint8_t *dest, uint8_t *src, size_t num_samples);

void sample_filter_comb (sample_t *dest, size_t num_samples, sample_t starting_sample);

#endif // #ifndef __SAMPLE_H__
