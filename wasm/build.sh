#!/bin/bash

#################################
# compile time decoder settings #
#################################

# EMSCRIPTEN_PATH
# Path to your emscripten SDK.
EMSCRIPTEN_PATH=../../../playground/emsdk/emsdk_env.sh

# CHUNK_SIZE
# Maximum size of a single chunk, that can be loaded into the decoder.
# This has only a tiny impact on the decoder speed, thus we can go with
# a rather low value (aligned with typical PIPE_BUF values).
# Use one of 2 ^ (12 .. 16).
CHUNK_SIZE=65536 #16384

# INITIAL_MEMORY
# This is the total memory the wasm instance will occupy.
# Always adjust this after changes to values above.
# If not enough memory was given, emscripten will throw a linking error.
# This can be used to spot the real usage and round it up to the next 64KiB multiple.
INITIAL_MEMORY=$((3 * 65536))




##################
# compile script #
##################

cd wasm

# activate emscripten env
source $EMSCRIPTEN_PATH

#emcc -O3 \
#-DCHUNK_SIZE=$CHUNK_SIZE \
#-DCANVAS_SIZE=$CANVAS_SIZE \
#-DPALETTE_SIZE=$PALETTE_SIZE \
#-s ASSERTIONS=0 \
#-s SUPPORT_ERRNO=0 \
#-s TOTAL_STACK=16384 \
#-s MALLOC=none \
#-s INITIAL_MEMORY=$INITIAL_MEMORY \
#-s MAXIMUM_MEMORY=$INITIAL_MEMORY \
#-s EXPORTED_FUNCTIONS='[
#  "_decode",
#  "_get_chunk_address",
#  "_get_target_address"
#]' \
#-mbulk-memory --no-entry base64.cpp -o base64.wasm

emcc -O3 \
-DCHUNK_SIZE=$CHUNK_SIZE \
-DCANVAS_SIZE=$CANVAS_SIZE \
-DPALETTE_SIZE=$PALETTE_SIZE \
-s ASSERTIONS=0 \
-s SUPPORT_ERRNO=0 \
-s TOTAL_STACK=16384 \
-s MALLOC=none \
-s INITIAL_MEMORY=$INITIAL_MEMORY \
-s MAXIMUM_MEMORY=$INITIAL_MEMORY \
-s EXPORTED_FUNCTIONS='[
  "_transcode",
  "_decode",
  "_get_chunk_address",
  "_get_target_address"
]' \
-msimd128 -msse -msse2 -mssse3 -msse4.1 -mbulk-memory --no-entry base64.cpp -o base64.wasm

# wrap wasm bytes into JSON file
node wrap_wasm.js $CHUNK_SIZE
