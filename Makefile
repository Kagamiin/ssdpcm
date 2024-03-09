CC := cc
SRC_DIR := ./src
BUILD_DIR := ./build


CFLAGS_DBG := \
	-Wall \
	-Wextra \
	-Werror \
	-O0 \
	-g \
	-fsanitize=address \
	-fno-omit-frame-pointer \
	-fno-optimize-sibling-calls \

CFLAGS_DEV := \
	-Wall \
	-Wextra \
	-Werror \
	-O3 \
	-Ofast
	

DEFINES_DEV := \
	-D_DEBUG \
	-I$(SRC_DIR)/include

CFLAGS := $(CFLAGS_DEV) $(DEFINES_DEV)

objects_nes := \
	block_codec.o \
	sigma.o \
	sigma_generic.o \
	sigma_generic_comb.o \
	sigma_u8_overflow.o \
	sigma_u8_overflow_comb.o \
	sigma_u7_overflow.o \
	sigma_u7_overflow_comb.o \
	encode_bruteforce.o \
	sample_conv.o \
	sample_filter.o \
	bit_pack_unpack.o \
	nes_encoder.o \
	wav_file.o \
	error_strs.o

objects_wave := \
	block_codec.o \
	sigma.o \
	sigma_generic.o \
	sigma_generic_comb.o \
	sigma_u8_overflow.o \
	sigma_u8_overflow_comb.o \
	sigma_u7_overflow.o \
	sigma_u7_overflow_comb.o \
	encode_bruteforce.o \
	encode_binary_search.o \
	sample_conv.o \
	sample_filter.o \
	bit_pack_unpack.o \
	wav_simulator.o \
	wav_file.o \
	error_strs.o
	



vpath %.c $(SRC_DIR) $(SRC_DIR)/block
vpath %.o $(BUILD_DIR)

.PHONY: build_dirs all clean

all: build_dirs $(BUILD_DIR)/nes_encoder $(BUILD_DIR)/wav_simulator

$(BUILD_DIR)/wav_simulator: $(objects_wave)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/wav_simulator -lm $(patsubst %,$(BUILD_DIR)/%,$(objects_wave))

$(BUILD_DIR)/nes_encoder: $(objects_nes)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/nes_encoder $(patsubst %,$(BUILD_DIR)/%,$(objects_nes))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/$@ $<

build_dirs:
	@mkdir -p $(BUILD_DIR) 2>/dev/null

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/nes_encoder
