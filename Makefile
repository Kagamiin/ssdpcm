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

objects := \
	block_codec.o \
	sigma.o \
	sigma_generic.o \
	sigma_u8_overflow.o \
	sigma_u7_overflow.o \
	sigma_u7_overflow_comb.o \
	encode_bruteforce.o \
	sample_conv.o \
	sample_filter.o \
	bit_pack_unpack.o \
	nes_encoder.o

vpath %.c $(SRC_DIR) $(SRC_DIR)/block
vpath %.o $(BUILD_DIR)

.PHONY: build_dirs all clean

all: build_dirs $(BUILD_DIR)/nes_encoder

$(BUILD_DIR)/nes_encoder: $(objects)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/nes_encoder $(patsubst %,$(BUILD_DIR)/%,$(objects))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $(BUILD_DIR)/$@ $<

build_dirs:
	@mkdir -p $(BUILD_DIR) 2>/dev/null

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/nes_encoder
