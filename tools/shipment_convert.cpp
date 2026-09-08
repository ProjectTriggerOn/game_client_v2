//=============================================================================
// shipment_convert.cpp — generates resource/maps/shipment.map
//
// A reconstruction of Call of Duty: Modern Warfare (2019) "Shipment", laid out
// from the in-game tactical map. The container is the ruler: the reference's
// north/south edge container measures 67 x 27 px, an aspect of 2.48 against a
// real 20 ft ISO container's 2.485, which pins the reference at 11.2 px/m and
// the playable yard at ~33 x 31 m. That agrees with the engine's movement —
// MAX_WALK_SPEED is 5.0 m/s against the IW engine's 190 units/s (1 unit = 1
// inch, so 4.83 m/s), within 4%.
//
// Every piece of geometry is an FBX from resource/model: the Low Poly Shooter
// Pack's props, plus the container authored in tools/blender_build_assets.py.
// The only box brush left is the ground slab. Nothing is scaled by eye and
// nothing carries a hand-written collider, because both of those produced the
// previous attempt's failures — containers interpenetrating each other, and
// props rendering on their backs inside colliders twice their size.
//
// Two rules keep it that way:
//
//   1. Size, pivot and orientation are MEASURED from the FBX at generation
//      time (see Measure), through the same Assimp flags and the same node
//      transform baking Graphics/model.cpp performs. Anything the player can
//      collide with gets its collider from those same measured bounds, so the
//      visual and the collision volume cannot disagree.
//   2. The result is validated before it is written: no two colliders may
//      overlap, nothing solid may leave the yard, no spawn may sit inside
//      geometry, and no decorative prop may be buried in a solid. A violation
//      aborts rather than shipping a broken map.
//
// Build and run, from the client repo root inside a VS developer prompt:
//   cl /nologo /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS /I Game
//      /I ThirdParty\assimp\include tools\shipment_convert.cpp
//      /Fe:shipment_convert.exe
//      /link /LIBPATH:ThirdParty\assimp\lib assimp-vc143-mt.lib
//   shipment_convert.exe
//
// Assets are produced beforehand by:
//   blender --background --python tools/blender_build_assets.py
//=============================================================================
#include "map_io.h"

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr float PI  = 3.14159265358979f;
constexpr float DEG = PI / 180.0f;

//-----------------------------------------------------------------------------
// Yard metrics. Every dimension below is metres.
//-----------------------------------------------------------------------------
constexpr float CL = 6.06f;     // ISO 20 ft container: length
constexpr float CW = 2.44f;     //                      width
constexpr float CH = 2.59f;     //                      height

// The layout is built from the centre outwards, and the yard edge is then
// placed ON the outer face of the perimeter containers. Deriving it the other
// way round left a gap behind them: at 0.80 m the player (0.70 m across) could
// slip in and run the whole perimeter, and even a narrow one reads as a seam.
constexpr float ALLEY_X = 2.40f;   // north-south alley through the middle
constexpr float ALLEY_Z = 2.60f;   // east-west alley
constexpr float LANE_EW = 3.66f;   // centre block to the east/west containers
constexpr float LANE_NS = 4.00f;   // centre block to the north/south containers

constexpr float BX = CL * 0.5f + ALLEY_X * 0.5f;    // 4.23, centre block centres
constexpr float BZ = CW + ALLEY_Z * 0.5f;           // 3.74

constexpr float EW_INNER = BX + CL * 0.5f + LANE_EW + CW * 0.5f;   // 12.14
constexpr float EW_OUTER = EW_INNER + CW;                          // 14.58
constexpr float NS_INNER = BZ + CW + LANE_NS + CW * 0.5f;          // 11.40
constexpr float NS_OUTER = NS_INNER + CW;                          // 13.84

constexpr float HX = EW_OUTER + CW * 0.5f;    // 15.80, flush with the containers
constexpr float HZ = NS_OUTER + CW * 0.5f;    // 15.06

// A player on a container roof (2.59) who jumps (1.6) tops out at 4.19 m with
// eyes near 5.7, so the boundary has to beat that.
constexpr float CLIP_H = 6.5f;

constexpr float PLAYER_RADIUS = 0.35f;
constexpr float PLAYER_HEIGHT = 1.70f;

// Box-brush palette; see g_BoxTexPalette in Game/map.cpp. The ground slab is
// the only box brush left in the map, so it is the only entry needed.
enum BoxTex : uint32_t { TEX_GROUND = 1 };

//-----------------------------------------------------------------------------
// Output accumulators
//-----------------------------------------------------------------------------
struct Box { float mn[3], mx[3]; };

std::vector<mapio::MapModelRef> g_Models;
std::vector<mapio::MapAABB>     g_Colliders;
std::vector<mapio::MapSpawn>    g_Spawns;

// Bookkeeping for the validator, kept alongside the colliders it describes.
std::vector<Box>         g_SolidBoxes;
std::vector<std::string> g_SolidNames;
std::vector<Box>         g_PropBoxes;
std::vector<std::string> g_PropNames;
std::vector<std::string> g_Errors;

void Fail(const std::string& msg) { g_Errors.push_back(msg); }

//-----------------------------------------------------------------------------
// Geometry helpers
//-----------------------------------------------------------------------------
Box EmptyBox() {
    return Box{ {  FLT_MAX,  FLT_MAX,  FLT_MAX },
                { -FLT_MAX, -FLT_MAX, -FLT_MAX } };
}

bool IsEmpty(const Box& b) { return b.mn[0] > b.mx[0]; }

Box BoxFromCentre(float cx, float cy, float cz, float sx, float sy, float sz) {
    return Box{ { cx - sx * 0.5f, cy - sy * 0.5f, cz - sz * 0.5f },
                { cx + sx * 0.5f, cy + sy * 0.5f, cz + sz * 0.5f } };
}

void Expand(Box& dst, const Box& src) {
    if (IsEmpty(src)) return;
    for (int i = 0; i < 3; i++) {
        if (src.mn[i] < dst.mn[i]) dst.mn[i] = src.mn[i];
        if (src.mx[i] > dst.mx[i]) dst.mx[i] = src.mx[i];
    }
}

// Strict overlap: boxes that merely touch — a container butted against its
// neighbour — are fine; boxes that share volume are not.
bool Overlaps(const Box& a, const Box& b, float eps = 1e-3f) {
    return a.mx[0] - b.mn[0] > eps && b.mx[0] - a.mn[0] > eps
        && a.mx[1] - b.mn[1] > eps && b.mx[1] - a.mn[1] > eps
        && a.mx[2] - b.mn[2] > eps && b.mx[2] - a.mn[2] > eps;
}

mapio::MapAABB AabbFrom(const Box& b, bool ground = false) {
    return mapio::MapAABB{ b.mn[0], b.mn[1], b.mn[2], b.mx[0], b.mx[1], b.mx[2],
                           static_cast<uint8_t>(ground ? 1 : 0), { 0, 0, 0 } };
}

//-----------------------------------------------------------------------------
// Asset measurement — mirrors Graphics/model.cpp exactly: the same import flags,
// and the node transform baked in, so what we measure is what ModelDraw renders.
//
// Orientation needs no per-asset correction any more. The pack's mixed axis
// conventions are normalised when tools/blender_build_assets.py re-exports
// every asset; the five that used to arrive on their back now do not.
//-----------------------------------------------------------------------------
struct AssetInfo {
    float mn[3], mx[3];   // metres, node transform and unit scale applied
    float unit;           // metres per FBX unit
    bool  ok;
};

void AccumulateNode(const aiScene* scene, const aiNode* node, aiMatrix4x4 acc,
                    aiVector3D& mn, aiVector3D& mx) {
    acc = acc * node->mTransformation;
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            const aiVector3D p = acc * mesh->mVertices[v];
            if (p.x < mn.x) mn.x = p.x;
            if (p.y < mn.y) mn.y = p.y;
            if (p.z < mn.z) mn.z = p.z;
            if (p.x > mx.x) mx.x = p.x;
            if (p.y > mx.y) mx.y = p.y;
            if (p.z > mx.z) mx.z = p.z;
        }
    }
    for (unsigned int c = 0; c < node->mNumChildren; c++)
        AccumulateNode(scene, node->mChildren[c], acc, mn, mx);
}

std::map<std::string, AssetInfo> g_AssetCache;

const AssetInfo& Measure(const std::string& stem) {
    auto it = g_AssetCache.find(stem);
    if (it != g_AssetCache.end()) return it->second;

    AssetInfo info{};
    info.ok = false;
    info.unit = 1.0f;

    const std::string path = "resource/model/" + stem + ".fbx";
    const aiScene* scene = aiImportFile(path.c_str(),
        static_cast<unsigned int>(aiProcessPreset_TargetRealtime_MaxQuality
                                | aiProcess_ConvertToLeftHanded
                                | aiProcess_GenBoundingBoxes));
    if (!scene) {
        Fail("cannot open asset " + path);
        return g_AssetCache.emplace(stem, info).first->second;
    }

    aiVector3D mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    AccumulateNode(scene, scene->mRootNode, aiMatrix4x4(), mn, mx);
    aiReleaseImport(scene);

    // Blender writes some of these assets in centimetres and some in metres.
    // Nothing authored in metres is larger than the 7.2 m watchtower and
    // nothing authored in centimetres is smaller than the 28 cm ammo box, so a
    // 10-unit threshold separates them cleanly. A mistake here would show up
    // immediately in the size report main() prints, and in the sanity check
    // below.
    const float biggest = std::max(std::max(mx.x - mn.x, mx.y - mn.y), mx.z - mn.z);
    info.unit = biggest > 10.0f ? 0.01f : 1.0f;

    info.mn[0] = mn.x * info.unit; info.mn[1] = mn.y * info.unit; info.mn[2] = mn.z * info.unit;
    info.mx[0] = mx.x * info.unit; info.mx[1] = mx.y * info.unit; info.mx[2] = mx.z * info.unit;

    const float height = info.mx[1] - info.mn[1];
    if (height < 0.02f || height > 12.0f)
        Fail(stem + ": implausible height " + std::to_string(height) +
             " m — unit detection or the asset export is wrong");

    info.ok = true;
    return g_AssetCache.emplace(stem, info).first->second;
}

//-----------------------------------------------------------------------------
// Placement
//-----------------------------------------------------------------------------

// World bounds of a prop placed at (x, baseY, z) with the given yaw.
Box PropBounds(const AssetInfo& a, float x, float baseY, float z, float yawDeg) {
    const float cy = std::cos(yawDeg * DEG), sy = std::sin(yawDeg * DEG);
    Box out = EmptyBox();
    for (int i = 0; i < 8; i++) {
        const float vx = (i & 1) ? a.mx[0] : a.mn[0];
        const float vy = (i & 2) ? a.mx[1] : a.mn[1];
        const float vz = (i & 4) ? a.mx[2] : a.mn[2];
        // Matches Map_Draw: scale, then XMMatrixRotationRollPitchYaw with only
        // yaw set, then translate.
        const float qx =  vx * cy + vz * sy;
        const float qz = -vx * sy + vz * cy;
        Box corner{ { qx, vy, qz }, { qx, vy, qz } };
        Expand(out, corner);
    }
    out.mn[0] += x;     out.mx[0] += x;
    out.mn[1] += baseY; out.mx[1] += baseY;
    out.mn[2] += z;     out.mx[2] += z;
    return out;
}

mapio::MapModelRef PropModel(const std::string& stem, const AssetInfo& a,
                             float x, float y, float z, float yawDeg) {
    mapio::MapModelRef m{};
    std::snprintf(m.asset, sizeof(m.asset), "resource/model/%s.fbx", stem.c_str());
    m.pos[0] = x; m.pos[1] = y; m.pos[2] = z;
    m.rotEuler[1] = yawDeg * DEG;
    m.scale[0] = m.scale[1] = m.scale[2] = a.unit;
    return m;
}

// Place a prop with its underside exactly on `baseY`. Returns its world bounds
// so callers can stack props or wrap a pile in one collider.
Box Prop(const std::string& stem, float x, float baseY, float z, float yawDeg = 0.0f) {
    const AssetInfo& a = Measure(stem);
    if (!a.ok) return EmptyBox();
    const float y = baseY - PropBounds(a, 0.0f, 0.0f, 0.0f, yawDeg).mn[1];
    g_Models.push_back(PropModel(stem, a, x, y, z, yawDeg));
    const Box b = PropBounds(a, x, y, z, yawDeg);
    g_PropBoxes.push_back(b);
    g_PropNames.push_back(stem);
    return b;
}

// Place a prop only where nothing solid already is. The perimeter wall run
// uses this: where a container forms the yard edge it IS the wall, and a panel
// dropped on top of it would be buried inside.
bool Prop_IfClear(const std::string& stem, float x, float baseY, float z,
                  float yawDeg = 0.0f) {
    const AssetInfo& a = Measure(stem);
    if (!a.ok) return false;
    const float y = baseY - PropBounds(a, 0.0f, 0.0f, 0.0f, yawDeg).mn[1];
    const Box b = PropBounds(a, x, y, z, yawDeg);
    for (const auto& solid : g_SolidBoxes)
        if (Overlaps(b, solid, 0.02f)) return false;
    Prop(stem, x, baseY, z, yawDeg);
    return true;
}

// A prop the player collides with. The collider comes from the same measured
// bounds the placement used, so the two cannot drift apart. Placement is
// axis-aligned, which makes the bounds exact rather than conservative.
Box SolidProp(const std::string& name, const std::string& stem,
              float x, float baseY, float z, float yawDeg = 0.0f) {
    const AssetInfo& a = Measure(stem);
    if (!a.ok) return EmptyBox();
    const float y = baseY - PropBounds(a, 0.0f, 0.0f, 0.0f, yawDeg).mn[1];
    g_Models.push_back(PropModel(stem, a, x, y, z, yawDeg));
    const Box b = PropBounds(a, x, y, z, yawDeg);
    g_Colliders.push_back(AabbFrom(b));
    g_SolidBoxes.push_back(b);
    g_SolidNames.push_back(name);
    return b;
}

// The ground slab, and nothing else, is still a box brush.
void EmitGroundSlab(float cx, float cy, float cz, float sx, float sy, float sz) {
    mapio::MapModelRef m{};
    std::strncpy(m.asset, "__box__", sizeof(m.asset) - 1);
    m.pos[0] = cx;   m.pos[1] = cy;   m.pos[2] = cz;
    m.scale[0] = sx; m.scale[1] = sy; m.scale[2] = sz;
    m.textureId = TEX_GROUND;
    g_Models.push_back(m);
}


// An invisible boundary clip. What the player sees is the wall run and the
// container stacks; this is the surface that actually stops them, kept
// axis-aligned so collision stays exact.
void EmitClip(const std::string& name, const Box& b) {
    g_Colliders.push_back(AabbFrom(b));
    g_SolidBoxes.push_back(b);
    g_SolidNames.push_back(name);
}

const char* kContainerTex[4] = {
    "SM_Container_Red", "SM_Container_Blue", "SM_Container_Green", "SM_Container_Rust",
};

// A container in the playable yard: solid, axis-aligned, sitting on the ground.
Box Container(const std::string& name, float cx, float cz, bool alongX,
              int variant, float baseY = 0.0f) {
    return SolidProp(name, kContainerTex[variant & 3], cx, baseY, cz,
                     alongX ? 0.0f : 90.0f);
}

// A container beyond the boundary: visual only.
void DecorContainer(float cx, float cz, bool alongX, int variant, int level) {
    Prop(kContainerTex[variant & 3], cx, CH * static_cast<float>(level), cz,
         alongX ? 0.0f : 90.0f);
}

//-----------------------------------------------------------------------------
// Crates
//-----------------------------------------------------------------------------

// A climbable step, sat flush against a centre block's face. Standing on its
// 1.31 m top and jumping 1.6 m clears the 2.59 m container roof; from the
// ground the same jump gets you onto it.
//
// This was four small cargo boxes at assorted yaws. Collision is a single AABB
// either way, so the slanted heap never matched the square volume the player
// stood on — and that volume was only 0.66 m deep, narrower than the player.
void StepCrate(const std::string& name, float cx, float cz, float yaw) {
    SolidProp(name, "SM_Crate_Small", cx, 0.0f, cz, yaw);
}

// Spawn cover: one 1.80 x 1.81 x 1.60 m crate, for the same reason.
//
// 1.81 m tall is deliberate: it breaks a standing sightline into the spawn and
// stays above the 1.6 m jump, so it cannot become a perch overlooking one.
void SpawnCover(const std::string& name, float cx, float cz, float yaw) {
    SolidProp(name, "SM_Crate_Large", cx, 0.0f, cz, yaw);
}

//-----------------------------------------------------------------------------
// Validation
//-----------------------------------------------------------------------------
bool IsClip(const std::string& n) { return n.rfind("clip", 0) == 0; }

void Validate() {
    char buf[256];

    for (size_t i = 0; i < g_SolidBoxes.size(); i++) {
        for (size_t j = i + 1; j < g_SolidBoxes.size(); j++) {
            if (Overlaps(g_SolidBoxes[i], g_SolidBoxes[j])) {
                std::snprintf(buf, sizeof(buf), "colliders overlap: %s x %s",
                              g_SolidNames[i].c_str(), g_SolidNames[j].c_str());
                Fail(buf);
            }
        }
    }

    for (size_t i = 0; i < g_SolidBoxes.size(); i++) {
        if (IsClip(g_SolidNames[i])) continue;      // the clips are the edge
        const Box& b = g_SolidBoxes[i];
        if (b.mn[0] < -HX - 1e-3f || b.mx[0] > HX + 1e-3f ||
            b.mn[2] < -HZ - 1e-3f || b.mx[2] > HZ + 1e-3f) {
            std::snprintf(buf, sizeof(buf),
                          "%s leaves the yard: x[%.2f %.2f] z[%.2f %.2f]",
                          g_SolidNames[i].c_str(), b.mn[0], b.mx[0], b.mn[2], b.mx[2]);
            Fail(buf);
        }
    }

    // A solid parked against the boundary must not leave a walkable slot behind
    // it: narrower than the player reads as a seam, wider turns into a lane
    // running around the outside of the map that the layout never intended.
    // Only geometry actually up against the wall is judged — anything further
    // in is fronting an open lane on purpose, not hiding a slot.
    const float kAgainstTheWall = 1.5f;
    for (size_t i = 0; i < g_SolidBoxes.size(); i++) {
        if (IsClip(g_SolidNames[i])) continue;
        const Box& b = g_SolidBoxes[i];
        const float gaps[4] = { b.mn[0] - (-HX), HX - b.mx[0],
                                b.mn[2] - (-HZ), HZ - b.mx[2] };
        const char* sides[4] = { "west", "east", "north", "south" };
        for (int k = 0; k < 4; k++) {
            if (gaps[k] > 0.01f && gaps[k] < kAgainstTheWall) {
                std::snprintf(buf, sizeof(buf),
                    "%s leaves a %.2f m gap against the %s boundary; perimeter "
                    "geometry must sit flush", g_SolidNames[i].c_str(), gaps[k], sides[k]);
                Fail(buf);
            }
        }
    }

    for (const auto& s : g_Spawns) {
        const Box capsule{ { s.x - PLAYER_RADIUS, s.y + 0.05f, s.z - PLAYER_RADIUS },
                           { s.x + PLAYER_RADIUS, s.y + PLAYER_HEIGHT, s.z + PLAYER_RADIUS } };
        for (size_t i = 0; i < g_SolidBoxes.size(); i++) {
            if (Overlaps(capsule, g_SolidBoxes[i])) {
                std::snprintf(buf, sizeof(buf), "spawn (%.1f, %.1f) team %u is inside %s",
                              s.x, s.z, s.team, g_SolidNames[i].c_str());
                Fail(buf);
            }
        }
    }

    // A decorative prop half-buried in a container reads as a bug. Anything the
    // player collides with is placed through SolidProp and never lands here.
    for (size_t p = 0; p < g_PropBoxes.size(); p++) {
        for (size_t i = 0; i < g_SolidBoxes.size(); i++) {
            if (IsClip(g_SolidNames[i])) continue;
            if (Overlaps(g_PropBoxes[p], g_SolidBoxes[i], 0.02f)) {
                std::snprintf(buf, sizeof(buf), "prop %s at (%.1f, %.1f) is buried in %s",
                              g_PropNames[p].c_str(),
                              (g_PropBoxes[p].mn[0] + g_PropBoxes[p].mx[0]) * 0.5f,
                              (g_PropBoxes[p].mn[2] + g_PropBoxes[p].mx[2]) * 0.5f,
                              g_SolidNames[i].c_str());
                Fail(buf);
            }
        }
    }
}

} // namespace

int main() {
    mapio::MapData d{};
    std::strncpy(d.name,   "shipment",  sizeof(d.name) - 1);
    std::strncpy(d.author, "converter", sizeof(d.author) - 1);

    //------------------------------------------------------------------
    // Ground: one collider for the world floor, one textured slab for the
    // yard, and a wider apron so the engine's grid plane never shows.
    //------------------------------------------------------------------
    g_Colliders.push_back(mapio::MapAABB{ -128.0f, -1.0f, -128.0f,
                                           128.0f,  0.0f,  128.0f, 1, {0,0,0} });
    // Two slabs, and both sit slightly PROUD of y = 0.
    //
    // Map_Draw calls MeshField_Draw(XMMatrixTranslation(0, -1, 0)) expecting the
    // engine's debug grid a metre below the floor, but MeshField_Draw marks that
    // parameter [[maybe_unused]] and builds its own world matrix at y = 0. The
    // grid therefore lands exactly on the play plane, co-planar with a ground
    // slab whose top is at 0, and wins the z-fight — which is why grid.png kept
    // showing through the concrete. default.map relies on that grid as its floor,
    // so the fix belongs here rather than in MeshField_Draw: lift the slab a few
    // centimetres so it is unambiguously in front. The player stands on the
    // collision plane at y = 0 either way.
    EmitGroundSlab(0.0f, -0.60f, 0.0f, 220.0f, 0.50f, 220.0f);                       // apron, top at -0.35
    EmitGroundSlab(0.0f, -0.21f, 0.0f, 2.0f * HX + 4.0f, 0.50f, 2.0f * HZ + 4.0f);   // yard,  top at +0.04

    //------------------------------------------------------------------
    // The centre: four blocks of two east-west containers each, arranged 2x2.
    // The 2.4 m north-south and 2.6 m east-west alleys between them are
    // Shipment's signature crossing.
    //------------------------------------------------------------------
    for (int ix = 0; ix < 2; ix++) {
        for (int iz = 0; iz < 2; iz++) {
            const float sx = ix ? 1.0f : -1.0f;
            const float sz = iz ? 1.0f : -1.0f;
            char n[64];
            std::snprintf(n, sizeof(n), "centre[%d%d]a", ix, iz);
            Container(n, sx * BX, sz * BZ - CW * 0.5f, true, ix * 2 + iz);
            std::snprintf(n, sizeof(n), "centre[%d%d]b", ix, iz);
            Container(n, sx * BX, sz * BZ + CW * 0.5f, true, iz * 2 + ix + 1);
        }
    }

    //------------------------------------------------------------------
    // North and south edges: one container square to the yard and a second
    // staggered behind it. The reference leans that second container against
    // the first; axis-aligned, the stagger is what preserves the gap you can
    // cut through. Repeated under 180 degree rotation so the east and west
    // halves stay identical for the two teams.
    //------------------------------------------------------------------
    Container("north-inner", -1.5f, -NS_INNER, true, 3);
    Container("north-outer",  1.5f, -NS_OUTER, true, 2);
    Container("south-inner",  1.5f,  NS_INNER, true, 3);
    Container("south-outer", -1.5f,  NS_OUTER, true, 2);

    //------------------------------------------------------------------
    // East and west edges: two north-south containers each, near the wall,
    // mirrored across x so neither team's half is favoured.
    //------------------------------------------------------------------
    const float EW_Z = -1.20f;
    Container("west-outer", -EW_OUTER, EW_Z, false, 0);
    Container("west-inner", -EW_INNER, EW_Z, false, 3);
    Container("east-outer",  EW_OUTER, EW_Z, false, 1);
    Container("east-inner",  EW_INNER, EW_Z, false, 3);

    //------------------------------------------------------------------
    // Roof access: a step pile flush against one face of each centre block.
    //------------------------------------------------------------------
    // Flush against the block faces, using the crate's own measured half-depth.
    // It is square in plan, so a 90 degree yaw does not change that offset and
    // its axis-aligned bounds stay exact.
    const AssetInfo& stepAsset = Measure("SM_Crate_Small");
    const float stepHalf = stepAsset.ok ? (stepAsset.mx[2] - stepAsset.mn[2]) * 0.5f : 0.65f;
    StepCrate("step-nw", -BX, -(BZ + CW) - stepHalf,  0.0f);
    StepCrate("step-se",  BX,  (BZ + CW) + stepHalf,  0.0f);
    StepCrate("step-ne",  BX + CL * 0.5f + stepHalf, -BZ, 90.0f);
    StepCrate("step-sw", -BX - CL * 0.5f - stepHalf,  BZ, 90.0f);

    //------------------------------------------------------------------
    // Spawn cover, one pile per corner pocket.
    //------------------------------------------------------------------
    // Yawed a little off-axis so the four do not read as a stamped-out set;
    // the collider follows the rotated bounds, so it stays honest.
    SpawnCover("cover-nw", -9.40f, -9.20f,   0.0f);
    SpawnCover("cover-ne",  9.40f, -9.20f,  90.0f);
    SpawnCover("cover-sw", -9.40f,  9.20f,  90.0f);
    SpawnCover("cover-se",  9.40f,  9.20f,   0.0f);

    //------------------------------------------------------------------
    // Boundary clips.
    //------------------------------------------------------------------
    EmitClip("clip-north", Box{ { -HX - 1.0f, 0.0f, -HZ - 1.0f },
                                {  HX + 1.0f, CLIP_H, -HZ } });
    EmitClip("clip-south", Box{ { -HX - 1.0f, 0.0f,  HZ },
                                {  HX + 1.0f, CLIP_H,  HZ + 1.0f } });
    EmitClip("clip-west",  Box{ { -HX - 1.0f, 0.0f, -HZ },
                                { -HX,        CLIP_H,  HZ } });
    EmitClip("clip-east",  Box{ {  HX,        0.0f, -HZ },
                                {  HX + 1.0f, CLIP_H,  HZ } });

    //------------------------------------------------------------------
    // The visible boundary: a run of wall panels just inside the clips, with
    // stacked containers behind them so the barrier still reads as solid from
    // a container roof.
    //------------------------------------------------------------------
    {
        const AssetInfo& wall = Measure("SM_Wall_001");
        const float wallLen = wall.ok ? (wall.mx[0] - wall.mn[0]) : 5.89f;
        for (float x = -HX + wallLen * 0.5f; x < HX; x += wallLen) {
            Prop_IfClear("SM_Wall_001", x, 0.0f, -HZ + 0.18f,   0.0f);
            Prop_IfClear("SM_Wall_001", x, 0.0f,  HZ - 0.18f, 180.0f);
        }
        for (float z = -HZ + wallLen * 0.5f; z < HZ; z += wallLen) {
            Prop_IfClear("SM_Wall_001", -HX + 0.18f, 0.0f, z,  90.0f);
            Prop_IfClear("SM_Wall_001",  HX - 0.18f, 0.0f, z, -90.0f);
        }
        // Corner returns, so the runs do not end in mid-air.
        Prop_IfClear("SM_Wall_002", -HX + 1.3f, 0.0f, -HZ + 1.3f,   0.0f);
        Prop_IfClear("SM_Wall_008",  HX - 1.3f, 0.0f, -HZ + 1.3f, -90.0f);
        Prop_IfClear("SM_Wall_008", -HX + 1.3f, 0.0f,  HZ - 1.3f,  90.0f);
        Prop_IfClear("SM_Wall_002",  HX - 1.3f, 0.0f,  HZ - 1.3f, 180.0f);
    }

    //------------------------------------------------------------------
    // Beyond the boundary: stacked containers, watchtowers and yard
    // buildings. Visual only.
    //------------------------------------------------------------------
    {
        int k = 0;
        for (float x = -15.0f; x <= 15.0f; x += CL + 0.2f, k++)
            for (int lv = 0; lv < 3; lv++)
                DecorContainer(x, -HZ - 2.4f, true, k + lv, lv);
        for (float z = -12.0f; z <= 12.0f; z += CL + 0.2f, k++)
            for (int lv = 0; lv < 4; lv++)
                DecorContainer(-HX - 2.4f, z, false, k + lv, lv);
        for (float z = -12.0f; z <= 12.0f; z += CL + 0.2f, k++)
            for (int lv = 0; lv < 2; lv++)
                DecorContainer(HX + 2.4f, z, false, k + lv, lv);
        for (float x = -12.0f; x <= 12.0f; x += CL + 0.2f, k++)
            for (int lv = 0; lv < 2; lv++)
                DecorContainer(x, HZ + 2.4f, true, k + lv, lv);

        Prop("SM_Watchtower",  HX + 7.5f, 0.0f, -HZ - 7.5f, -35.0f);
        Prop("SM_Watchtower", -HX - 9.0f, 0.0f,  HZ + 6.5f,  25.0f);
        Prop("SM_House_Standard",  HX + 13.0f, 0.0f,   4.0f,  -90.0f);
        Prop("SM_House_Small",    -HX - 14.0f, 0.0f,  -8.0f,   90.0f);
        Prop("SM_House_Small",           6.0f, 0.0f,  HZ + 12.0f, 180.0f);
        for (int i = 0; i < 6; i++)
            Prop("SM_Scaffolding", -HX - 6.2f, 0.0f, -12.0f + 5.0f * static_cast<float>(i));
    }

    //------------------------------------------------------------------
    // Yard dressing: decoration only, placed clear of the solids so nothing
    // looks half-buried. The validator enforces that.
    //------------------------------------------------------------------
    {
        Prop("SM_Explosive_Barrel", -13.4f, 0.0f, -13.6f,  12.0f);
        Prop("SM_Explosive_Barrel", -12.4f, 0.0f, -14.2f, -30.0f);
        Prop("SM_Explosive_Barrel",  13.4f, 0.0f,  13.6f,  12.0f);
        Prop("SM_Explosive_Barrel",  12.4f, 0.0f,  14.2f, -30.0f);
        Prop("SM_Explosive_Gas_Tank",  14.2f, 0.0f, -13.2f, 0.0f);
        Prop("SM_Explosive_Gas_Tank", -14.2f, 0.0f,  13.2f, 0.0f);

        Prop("SM_Sandbag", -11.9f, 0.0f, -11.2f, 25.0f);
        Prop("SM_Sandbag", -11.0f, 0.0f, -11.6f, 25.0f);
        Prop("SM_Sandbag",  11.9f, 0.0f,  11.2f, 25.0f);
        Prop("SM_Sandbag",  11.0f, 0.0f,  11.6f, 25.0f);

        Prop("SM_Box_Ammo", -15.2f, 0.0f, -6.8f, -12.0f);
        Prop("SM_Box_Ammo",  15.2f, 0.0f,  6.8f, -12.0f);
        Prop("SM_Box_Ammo",  -8.2f, 0.0f, 14.4f,  40.0f);
        Prop("SM_Box_Ammo",   8.2f, 0.0f,-14.4f,  40.0f);

        Prop("SM_Box_Wood_02", -6.6f, 0.0f, -14.8f,  8.0f);
        Prop("SM_Box_Wood_02",  6.6f, 0.0f,  14.8f,  8.0f);
        Prop("SM_Plank_Wooden_Big_01", -9.9f, 0.0f,  14.9f,  15.0f);
        Prop("SM_Plank_Wooden_001",     9.9f, 0.0f, -14.9f, -15.0f);

        Prop("SM_Workbench", -15.4f, 0.0f, -7.2f,  90.0f);
        Prop("SM_Workbench",  15.4f, 0.0f,  7.2f, -90.0f);

        Prop("SM_Tubes_005", -15.2f, 0.0f, -10.4f, 90.0f);
        Prop("SM_Tubes_005",  15.2f, 0.0f,  10.4f, 90.0f);
        // SM_Tubes_004 is avoided on purpose: Assimp splits it into two meshes
        // and one of them carries no UV set, so it renders untextured white.
        // tools/fbx_probe reports it.
        Prop("SM_Tubes_005",  -3.0f, 0.0f, -14.8f,  0.0f);
        Prop("SM_Tubes_005",   3.0f, 0.0f,  14.8f,  0.0f);

        Prop("SM_Stone_008", -10.6f, 0.0f, -15.0f, 30.0f);
        Prop("SM_Stone_008",  10.6f, 0.0f,  15.0f, 30.0f);

        Prop("SM_Fence_Wood_002", -13.0f, 0.0f,  -7.6f,  0.0f);
        Prop("SM_Fence_Wood_002",  13.0f, 0.0f,   7.6f,  0.0f);

        // Leaned against the inner face of the east/west container pairs.
        Prop("SM_Shutter_Large_01", -10.55f, 0.0f, -5.4f, 90.0f);
        Prop("SM_Shutter_Large_01",  10.55f, 0.0f,  5.4f, -90.0f);

        // Skips the middle of each side, where the east/west containers sit.
        for (float z : { -9.5f, -6.5f, 6.5f, 9.5f }) {
            Prop("SM_Wire_Barbed", -HX + 1.1f, 0.0f, z,  90.0f);
            Prop("SM_Wire_Barbed",  HX - 1.1f, 0.0f, z, -90.0f);
        }

        Prop("SM_Lamp_Construction_001", -15.6f, 0.0f, -14.8f,   40.0f);
        Prop("SM_Lamp_Construction_003",  15.6f, 0.0f, -14.8f,  -40.0f);
        Prop("SM_Lamp_Construction_003", -15.6f, 0.0f,  14.8f,  140.0f);
        Prop("SM_Lamp_Construction_001",  15.6f, 0.0f,  14.8f, -140.0f);
    }

    //------------------------------------------------------------------
    // Spawns: six per team, spread over that team's two corner pockets and
    // facing the middle. The engine builds its forward vector as
    // (sin yaw, 0, cos yaw) — Game/player_fps.cpp, Network/mock_server.cpp and
    // Network/remote_player.cpp all agree — so yaw 0 looks down +Z and +X is
    // yaw +90, which makes the angle toward the origin atan2(-x, -z).
    //------------------------------------------------------------------
    {
        const float kOffsets[3][2] = { { 14.80f, 13.20f },
                                       { 11.50f, 13.40f },
                                       { 14.80f,  9.40f } };
        for (int side = 0; side < 2; side++) {
            const float sx = side ? 1.0f : -1.0f;
            const uint8_t team = side ? mapio::TEAM_BLUE : mapio::TEAM_RED;
            for (int corner = 0; corner < 2; corner++) {
                const float sz = corner ? 1.0f : -1.0f;
                for (const auto& o : kOffsets) {
                    const float x = sx * o[0];
                    const float z = sz * o[1];
                    g_Spawns.push_back(mapio::MapSpawn{
                        x, 0.0f, z, std::atan2(-x, -z), team, {0,0,0} });
                }
            }
        }
    }

    //------------------------------------------------------------------
    // Lighting and environment: the cold overcast port the reference reads as.
    //------------------------------------------------------------------
    {
        mapio::MapLight sun{};
        sun.type = mapio::LIGHT_DIRECTIONAL;
        sun.dir[0] = -0.42f; sun.dir[1] = -1.0f; sun.dir[2] = 0.30f;
        // Map_GetDirectionalLight premultiplies colour by intensity, and the
        // engine adds its own ambient and specular on top, so a sun near 1.0
        // blows the yard out to white — every surface here faces the sky.
        sun.color[0] = 0.86f; sun.color[1] = 0.89f; sun.color[2] = 0.95f;
        sun.intensity = 0.52f;
        d.lights.push_back(sun);

        // A point light's `intensity` is its RANGE IN METRES once it reaches
        // Light_SetPointLightWorldByCount (see Map_UploadPointLights) — only a
        // directional light's colour gets premultiplied by that field. These
        // lamps first shipped as "1.0 warm white, intensity 1.1", i.e. a
        // full-brightness lamp, and washed the whole yard to white. The
        // brightness has to live in the colour.
        const float lamps[4][2] = { { -15.6f, -14.8f }, { 15.6f, -14.8f },
                                    { -15.6f,  14.8f }, { 15.6f,  14.8f } };
        for (const auto& l : lamps) {
            mapio::MapLight pt{};
            pt.type = mapio::LIGHT_POINT;
            pt.pos[0] = l[0]; pt.pos[1] = 2.0f; pt.pos[2] = l[1];
            pt.color[0] = 0.22f; pt.color[1] = 0.19f; pt.color[2] = 0.13f;
            pt.intensity = 5.0f;      // metres of reach, not brightness
            d.lights.push_back(pt);
        }
    }

    std::strncpy(d.env.skyAsset, "resource/model/sky.fbx", sizeof(d.env.skyAsset) - 1);
    d.env.ambient[0]  = 0.26f; d.env.ambient[1]  = 0.28f; d.env.ambient[2]  = 0.32f;
    d.env.fogColor[0] = 0.60f; d.env.fogColor[1] = 0.63f; d.env.fogColor[2] = 0.67f;
    // The yard is only 33 m across, so fog must not touch it: it starts past
    // the far wall and closes over the out-of-bounds scenery.
    d.env.fogStart = 38.0f;
    d.env.fogEnd   = 130.0f;

    //------------------------------------------------------------------
    // Validate, then write.
    //------------------------------------------------------------------
    Validate();
    if (!g_Errors.empty()) {
        std::fprintf(stderr, "shipment_convert: %zu problem(s), map NOT written\n",
                     g_Errors.size());
        for (const auto& e : g_Errors) std::fprintf(stderr, "  %s\n", e.c_str());
        return 1;
    }

    d.colliders = g_Colliders;
    d.models    = g_Models;
    d.spawns    = g_Spawns;

    if (!mapio::Write("resource/maps/shipment.map", d)) {
        std::fprintf(stderr, "shipment_convert: could not write resource/maps/shipment.map\n");
        return 1;
    }

    std::printf("wrote resource/maps/shipment.map\n");
    std::printf("  colliders %zu   models %zu   spawns %zu   lights %zu   checksum %08x\n",
                d.colliders.size(), d.models.size(), d.spawns.size(), d.lights.size(),
                mapio::CollisionChecksum(d));
    std::printf("  yard %.1f x %.1f m, centre alleys %.1f m (N-S) / %.1f m (E-W)\n",
                2.0f * HX, 2.0f * HZ, ALLEY_X, ALLEY_Z);
    std::printf("\n  measured assets, as the engine will render them:\n");
    for (const auto& kv : g_AssetCache) {
        const AssetInfo& a = kv.second;
        if (!a.ok) continue;
        std::printf("    %-28s %6.2f x %6.2f x %6.2f m   base %+5.2f   unit %.2f\n",
                    kv.first.c_str(),
                    a.mx[0] - a.mn[0], a.mx[1] - a.mn[1], a.mx[2] - a.mn[2],
                    a.mn[1], a.unit);
    }
    return 0;
}
