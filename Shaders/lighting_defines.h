//=============================================================================
// lighting_defines.h
//
// Single source of truth for lighting constants shared between C++ and HLSL.
// This file must be consumable by BOTH the MSVC C++ preprocessor (via
// #include from .h/.cpp) and fxc (via #include from .hlsl). That means:
//   - no templates, no classes, no enums with underlying types
//   - pure #define constants ONLY
//   - no #pragma once (fxc warns); use a classic include guard instead
//
// When you change LIGHT_MAX_POINT_LIGHTS, rebuild the shaders (MSBuild
// FxCompile step) — the .cso files embed the value at compile time.
//=============================================================================

#ifndef TRIGGERON_LIGHTING_DEFINES_H
#define TRIGGERON_LIGHTING_DEFINES_H

// Maximum number of point lights uploaded to and read by the pixel shader.
// Keep in sync with the C++ side; both sides read from this file.
//
// Current value: 16 (up from 4). Each light is 32 bytes, so the point-light
// cbuffer is 16 * 32 + 16 = 528 bytes — comfortably under the 64 KB D3D11
// cbuffer limit. The shader loop remains dynamic (for i < point_light_count),
// so raising the cap does not add runtime cost to maps with fewer lights.
#define LIGHT_MAX_POINT_LIGHTS 16

// cbuffer slot assignments for lighting, mirrored with the C++ enum
// LightCbufferSlot in Graphics/light.h. HLSL cannot consume an enum, so both
// sides must agree on these integers.
#define LIGHT_CB_SLOT_AMBIENT     1
#define LIGHT_CB_SLOT_DIRECTIONAL 2
#define LIGHT_CB_SLOT_SPECULAR    3
#define LIGHT_CB_SLOT_POINTLIGHTS 4

#endif // TRIGGERON_LIGHTING_DEFINES_H
