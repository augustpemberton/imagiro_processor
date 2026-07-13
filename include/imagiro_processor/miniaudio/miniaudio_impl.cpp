// Single translation unit that compiles the miniaudio implementation.
// Decode-only configuration: no device I/O, no encoders, no signal generation,
// no resource manager. This keeps the object light — only the built-in decoders
// (wav/flac/mp3 + stb_vorbis for ogg) are compiled in.
#define MA_NO_DEVICE_IO
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER

#define MA_IMPLEMENTATION
#include "miniaudio.h"
