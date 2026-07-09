//=============================================================================
// editor_map.cpp — EditorMap <-> mapio::MapData conversion + .map IO.
//=============================================================================
// map_io.h uses std::fopen (portable C IO shared with the Linux server);
// silence MSVC's fopen_s deprecation (C4996, promoted to error by /sdl).
#define _CRT_SECURE_NO_WARNINGS

#include "editor_map.h"
#include <cstring>

using namespace DirectX;

namespace {

constexpr char kBoxAsset[] = "__box__";

// Build a bounded std::string from a fixed char buffer that may not be
// NUL-terminated (defends against a hostile .map header/asset field).
std::string StrFromFixed(const char* buf, size_t cap) {
    return std::string(buf, ::strnlen(buf, cap));
}

void CopyFixed(char* dst, size_t cap, const std::string& src) {
    std::memset(dst, 0, cap);
    std::strncpy(dst, src.c_str(), cap - 1);   // leaves dst[cap-1] == 0
}

} // namespace

namespace editor {

mapio::MapData EditorMap_ToMapData(const EditorMap& m) {
    mapio::MapData d{};
    CopyFixed(d.name,   sizeof(d.name),   m.meta.name);
    CopyFixed(d.author, sizeof(d.author), m.meta.author);

    // Visual section: box brushes first (as "__box__"), then FBX props.
    for (const auto& b : m.boxes) {
        mapio::MapModelRef r{};
        std::strncpy(r.asset, kBoxAsset, sizeof(r.asset) - 1);
        r.pos[0] = b.pos.x; r.pos[1] = b.pos.y; r.pos[2] = b.pos.z;
        r.rotEuler[0] = b.rotEuler.x; r.rotEuler[1] = b.rotEuler.y; r.rotEuler[2] = b.rotEuler.z;
        r.scale[0] = b.scale.x; r.scale[1] = b.scale.y; r.scale[2] = b.scale.z;
        r.textureId = b.textureId;
        d.models.push_back(r);
    }
    for (const auto& p : m.models) {
        mapio::MapModelRef r{};
        std::strncpy(r.asset, p.asset.c_str(), sizeof(r.asset) - 1);
        r.pos[0] = p.pos.x; r.pos[1] = p.pos.y; r.pos[2] = p.pos.z;
        r.rotEuler[0] = p.rotEuler.x; r.rotEuler[1] = p.rotEuler.y; r.rotEuler[2] = p.rotEuler.z;
        r.scale[0] = p.scale.x; r.scale[1] = p.scale.y; r.scale[2] = p.scale.z;
        r.textureId = p.textureId;
        d.models.push_back(r);
    }

    // Collision section.
    for (const auto& c : m.colliders) {
        mapio::MapAABB a{};
        a.minX = c.min.x; a.minY = c.min.y; a.minZ = c.min.z;
        a.maxX = c.max.x; a.maxY = c.max.y; a.maxZ = c.max.z;
        a.isGround = c.isGround ? 1 : 0;
        d.colliders.push_back(a);
    }
    for (const auto& s : m.spawns) {
        mapio::MapSpawn sp{};
        sp.x = s.pos.x; sp.y = s.pos.y; sp.z = s.pos.z;
        sp.yaw = s.yaw; sp.team = s.team;
        d.spawns.push_back(sp);
    }

    // Lights.
    for (const auto& l : m.lights) {
        mapio::MapLight ml{};
        ml.type = l.type;
        ml.pos[0] = l.pos.x; ml.pos[1] = l.pos.y; ml.pos[2] = l.pos.z;
        ml.dir[0] = l.dir.x; ml.dir[1] = l.dir.y; ml.dir[2] = l.dir.z;
        ml.color[0] = l.color.x; ml.color[1] = l.color.y; ml.color[2] = l.color.z;
        ml.intensity = l.intensity;
        d.lights.push_back(ml);
    }

    // Environment.
    CopyFixed(d.env.skyAsset, sizeof(d.env.skyAsset), m.env.skyAsset);
    d.env.ambient[0]  = m.env.ambient.x;  d.env.ambient[1]  = m.env.ambient.y;  d.env.ambient[2]  = m.env.ambient.z;
    d.env.fogColor[0] = m.env.fogColor.x; d.env.fogColor[1] = m.env.fogColor.y; d.env.fogColor[2] = m.env.fogColor.z;
    d.env.fogStart = m.env.fogStart;
    d.env.fogEnd   = m.env.fogEnd;
    return d;
}

EditorMap EditorMap_FromMapData(const mapio::MapData& d) {
    EditorMap m;
    m.meta.name   = StrFromFixed(d.name,   sizeof(d.name));
    m.meta.author = StrFromFixed(d.author, sizeof(d.author));

    for (const auto& r : d.models) {
        const bool isBox = std::strncmp(r.asset, kBoxAsset, sizeof(kBoxAsset)) == 0;
        if (isBox) {
            PlacedBox b;
            b.pos      = { r.pos[0], r.pos[1], r.pos[2] };
            b.rotEuler = { r.rotEuler[0], r.rotEuler[1], r.rotEuler[2] };
            b.scale    = { r.scale[0], r.scale[1], r.scale[2] };
            b.textureId = r.textureId;
            m.boxes.push_back(b);
        } else {
            PlacedModel p;
            p.asset    = StrFromFixed(r.asset, sizeof(r.asset));
            p.pos      = { r.pos[0], r.pos[1], r.pos[2] };
            p.rotEuler = { r.rotEuler[0], r.rotEuler[1], r.rotEuler[2] };
            p.scale    = { r.scale[0], r.scale[1], r.scale[2] };
            p.textureId = r.textureId;
            m.models.push_back(p);
        }
    }

    for (const auto& a : d.colliders) {
        EditorCollider c;
        c.min = { a.minX, a.minY, a.minZ };
        c.max = { a.maxX, a.maxY, a.maxZ };
        c.isGround   = a.isGround != 0;
        c.ownerModel = -1;   // link is not persisted in v1
        m.colliders.push_back(c);
    }
    for (const auto& s : d.spawns) {
        SpawnPoint sp;
        sp.pos  = { s.x, s.y, s.z };
        sp.yaw  = s.yaw;
        sp.team = s.team;
        m.spawns.push_back(sp);
    }
    for (const auto& l : d.lights) {
        EditorLight el;
        el.type  = l.type;
        el.pos   = { l.pos[0], l.pos[1], l.pos[2] };
        el.dir   = { l.dir[0], l.dir[1], l.dir[2] };
        el.color = { l.color[0], l.color[1], l.color[2] };
        el.intensity = l.intensity;
        m.lights.push_back(el);
    }

    m.env.skyAsset = StrFromFixed(d.env.skyAsset, sizeof(d.env.skyAsset));
    m.env.ambient  = { d.env.ambient[0], d.env.ambient[1], d.env.ambient[2] };
    m.env.fogColor = { d.env.fogColor[0], d.env.fogColor[1], d.env.fogColor[2] };
    m.env.fogStart = d.env.fogStart;
    m.env.fogEnd   = d.env.fogEnd;
    return m;
}

bool EditorMap_Load(const char* path, EditorMap& out) {
    mapio::MapData d;
    if (!mapio::Read(path, d)) return false;
    out = EditorMap_FromMapData(d);
    return true;
}

bool EditorMap_Save(const char* path, const EditorMap& m) {
    mapio::MapData d = EditorMap_ToMapData(m);
    return mapio::Write(path, d);
}

} // namespace editor
