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
