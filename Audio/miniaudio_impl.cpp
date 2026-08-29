//=============================================================================
// miniaudio_impl.cpp
//
// The ONE translation unit that instantiates miniaudio.  Kept separate so the
// ~10-minute-to-read single header is compiled exactly once and its warnings
// stay out of every other TU.  Nothing else in the project may define
// MINIAUDIO_IMPLEMENTATION.
//
// Vendored version: 0.11.25
//
// The cuts below are deliberate: this client is Windows-only and ships WAV
// assets exclusively, so every other backend and decoder is dead weight in
// both build time and binary size.
//=============================================================================
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI

#define MA_NO_MP3
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_GENERATION

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
