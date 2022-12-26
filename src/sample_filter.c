
#include <types.h>

void
sample_filter_comb (sample_t *dest, size_t num_samples, sample_t starting_sample)
{
	size_t i;
	sample_t temp1 = dest[0];
	sample_t temp2 = starting_sample;
	
	for (i = 0; i < num_samples - 1; i++)
	{
		dest[i] = (temp1 + temp2) / 2;
		temp2 = temp1;
		temp1 = dest[i + 1];
	}
	dest[num_samples - 1] = (temp1 + temp2) / 2;
}
