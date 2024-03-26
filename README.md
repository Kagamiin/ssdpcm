# SSDPCM

SSDPCM is an audio codec developed by Algorithm (of demoscene fame). I was fascinated by its compression efficiency and decompression speed, so I decided to study it and perform my own implementation of it.

This repository contains this implementation of an encoder/decoder for multiple variants of the SSDPCM codec. It also specifies my own standard on how SSDPCM files should be encoded. Read further for more information.

## What is SSDPCM?

SSDPCM stands for Step-Selected Differential Pulse Code Modulation. It's basically a form of DPCM that's split into blocks of samples, where the step sizes for each codeword are explicitly specified along with each block. It's an efficient low-bitrate audio codec designed with very low decode complexity.

### Is SSDPCM a form of ADPCM?

**No.** ADPCM works by signalling the step size change **implicitly** and **continuously** over time. In ADPCM, the decoder infers the step size change from the codewords themselves, and the encoder is responsible for generating the correct codewords.

SSDPCM, on the other hand, signals the step size change explicitly and only at the beginning of a new block of samples. This might sound like a disadvantage for SSDPCM, but it turns out to be quite an advantage against ADPCM once we begin to look at one of its big shortcomings - adaptation delay.

ADPCM takes time to react to highly changing slope sizes, which at lower bitrates leads to higher levels of slope overload and impulse noise, because its encoding process can only predict the next sample. SSDPCM encoding, on the other hand, analyzes the entire block of samples at once and determines the best slopes to use within that block in order to minimize error, which minimizes slope overload and impulse noise in tradeoff for flat hissing which is less objectionable.

### What is SSDPCM good for?

SSDPCM was originally intended as an efficient low-bitrate audio codec for fast decompression on ancient hardware, and that's what it does best. SSDPCM's original bitrate was 2 bits per sample; SSDPCM's sweet spot for encoding efficiency lies at the medium bitrates, between 1.6 and 2.3 bits per sample inclusive (fractional bit rates are a relatively recent development of mine for SSDPCM; the original author had only worked with integer bitrates of 1 and 2 bits per sample).

SSDPCM also does very well at 1 bit per sample, but the quality is a bit harsh and in my opinion you should really only use it if you really need to save space. There's a comb filter mode that helps cut down on the hiss in exchange for some of the high frequency reproduction.

SSDPCM was meant to be easily played back by even simple 8-bit CPUs like the 6502. It's so lightweight that my SSDPCM player for the Nintendo Entertainment System, <https://github.com/Kagamiin/ssplayer-nes>, is capable of achieving peak decompression and playback rates of up to 35 KHz (with some sacrifice - in this case, a tiny bit of jitter). That's almost CD sample rate, mind you.

### Why does SSDPCM take so long to encode at higher bitrates?

SSDPCM requires the encoder to analyze each block of samples and search for the best set of slopes. The higher the bitrate, the higher the number of combinations of slopes that needs to be searched through. My older implementation used a brute force algorithm which was unbearably dog slow. My current implementation uses a bisecting search algorithm that's much faster, but it still takes somewhat of a long time to encode at 3 bits per sample.

Note that if you use 16-bit samples as input, it can take even longer to encode, too, especially for the higher bitrates. Though that's where it makes the most sense to use 16-bit samples.

## SSDPCM file format specification

SSDPCM can be stored in quite a few ways, as long as it's convenient enough for playback. For instance, `nes_encoder.c` illustrates a quite unorthodox way of storing SSDPCM - where bitstream and slope data is stripped apart into a bunch of separate binary files, to be later assembled into a NES ROM. Such method happens to be quite convenient for making NES sample players using my own tool (<https://github.com/Kagamiin/ssplayer-nes>).

But in any case, we need a file format with the following characteristics:

- can store SSDPCM audio in a single file
- is easy to manipulate
- can preferably be used with existing libraries (avoid custom containers as much as possible)

### File container

SSDPCM is stored in the WAV container file format, following the [WAVEFORMATEXTENSIBLE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible) specification.

SSDPCM uses the subformat GUID `50445353-4d43-4b3a-6167-616d69696e7e`. Note that subformat GUIDs are written out with the first 3 groups of bytes reversed, so this would be written out as: `0x53, 0x53, 0x44, 0x50, 0x43, 0x4d, 0x3a, 0x4b, 0x61, 0x67, 0x61, 0x6d, 0x69, 0x69, 0x6e, 0x7e`.

By convention, it's good practice to use the .AUD extension to name SSDPCM files, in order to not mix them up with normal WAV files.

NOTE: All values are little-endian unless specified.

|   Field            |   Description                                                                     | Length   |  Accepted values     |
|:------------------:|:---------------------------------------------------------------------------------:|:--------:|:--------------------:|
| `wFormatTag`       | Legacy format tag from the WAVEFORMATEX header.                                   | 2 bytes  | `0xFFFE`             |
| `nChannels`        | Number of channels (mono or stereo).                                              | 2 bytes  | `1` or `2`           |
| `nSamplesPerSec`   | Sampling rate.                                                                    | 4 bytes  | Any unsigned integer |
| `nAvgBytesPerSec`  | Average bitrate divided by 8, rounded down.                                       | 4 bytes  | The expected value.  |
| `nBlockAlign`      | Number of bytes per block of samples, including slopes and references if present. | 2 bytes  | The expected value.  |
| `wBitsPerSample`   | Unused - my SSDPCM implementation has fractional bit-per-sample values.           | 2 bytes  | `0`                  |
| `cbSize`           | Length of the following extra data after the WAVEFORMATEX header.                 | 2 bytes  | `0x26`               |
| `wSamplesPerBlock` | Number of samples per block.                                                      | 2 bytes  | Any unsigned integer that's a multiple of the number of samples that fit in bytes_per_read_alignment (see below). |
| `dwChannelMask`    | Channel bitmask - see WAVEFORMATEXTENSIBLE specification for more information.    | 4 bytes  | 1 or 2 bits set depending on number of channels |
| `SubFormat`        | Subformat GUID (GUID-endianness)                                                  | 16 bytes | The SSDPCM GUID specified above. |
| `ssdpcm_data_chunk_id` | SSDPCM subchunk identifier (big-endian)                                       | 4 bytes  | `"SsDP"`             |
| `mode_fourcc`      | SSDPCM mode identifier (big-endian)                                               | 4 bytes  | _See below._         |
| `num_slopes`       | Number of distinct slopes in the chosen SSDPCM mode.                              | 1 byte   | _See below._         |
| `bits_per_output_sample` | Determines if the file is based around 8 or 16-bit samples.                 | 1 byte   | `8` or `16`          |
| `bytes_per_read_alignment` | Determines the minimum packing alignment for reading the codewords.       | 1 byte   | _See below._         |
| `has_reference_sample_on_every_block` | Determines if every block has a reference sample or not.       | 1 byte   | `0` or `1`           |
| `block_length`     | Number of samples per block.                                                      | 2 bytes  | Same as wSamplesPerBlock |
| `bytes_per_block`  | Number of bytes per block.                                                        | 2 bytes  | Same as nBlockAlign  |

Possible values for `mode_fourcc`:

- `ss1 ` - 1-bit SSDPCM
  - Implies `num_slopes` = `2`, `bytes_per_read_alignment` = `1`
- `ss1c` - 1-bit SSDPCM with comb filtering
  - Implies `num_slopes` = `2`, `bytes_per_read_alignment` = `1`
- `s1.6` - 1.6-bit SSDPCM
  - Implies `num_slopes` = `3`, `bytes_per_read_alignment` = `1`
- `ss2 ` - 2-bit SSDPCM
  - Implies `num_slopes` = `4`, `bytes_per_read_alignment` = `1`
- `s2.3` - 2.3-bit SSDPCM
  - Implies `num_slopes` = `5`, `bytes_per_read_alignment` = `3`
- `ss3 ` - 3-bit SSDPCM
  - Implies `num_slopes` = `8`, `bytes_per_read_alignment` = `7`

## Usage

To use this repository, you'll need a C compiler and the following tools/libraries:

- GNU Make
- OpenMP (optional)

If you don't have or don't want to use OpenMP, remove `-fopenmp` from line 23 of the Makefile and remove `encoder_parallel` from line 113.

### Building

To build the executables, simply run `make`. They'll be generated into the `build` directory, which will be automatically created for you.

The following programs will be compiled:

- `encoder` - This is a SSDPCM encoder/decoder. It supports all of the modes documented above and is able to both **encode** and **decode** files in the format documented above. It supports mono and stereo.

- `encoder_parallel` - This is a paralellized SSDPCM encoder. It also supports all of the modes documented above, but it's most useful for the higher bitrate modes. It generates slightly larger files than the normal encoder, because it has to store reference samples for every block in order to be able to encode them in parallel. It can decode files too, but it's not parallelized for that and it's a bit slower than the other program at it. It only supports mono (for now).

- `nes_encoder` - This is a special SSDPCM encoder tailored for my NES SSDPCM sample player. It only supports the subset of the modes that are supported by my sample player. It does not support WAV input, only raw unsigned 8-bit PCM (I need to change that). And the output it generates is not a single file, but a bunch of small files to be used in the assembly process. It also simultaneously generates a decoded output file so you can hear the result immediately after encoding. It obviously only supports mono, because the NES is mono.

- `wav_simulator` - This is a toy encoder that can be used to experiment with SSDPCM encoding. It lets you specify the number of slopes directly, and allows you to use comb filtering in any of the modes - so it can actually simulate a lot of SSDPCM bitrates that don't actually exist as a mode (yet, or due to being impractical to pack/unpack). The only disadvantage is that being a toy, it doesn't actually generate an encoded file - it internally encodes and decodes the output, then saves the decoded output as a WAV file. It also only supports mono, because it's an older program that was made before I conceived stereo encoding for SSDPCM.

