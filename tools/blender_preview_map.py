#!/usr/bin/env python3
"""Rebuild a TriggerOn .map inside Blender and render preview images.

    blender --background --python tools/blender_preview_map.py -- shipment

This reads the binary map and instantiates the real FBX assets it references,
so the preview shows what ModelDraw will draw rather than a schematic. Use it to
check a map before spending a client build on it: props floating or sunk, props
buried in geometry, container stacks that do not line up, sightlines.

Axis note: the engine is Y-up (X east, Y up, Z south) and Blender is Z-up, so a
map position (x, y, z) becomes (x, z, y) here and a map yaw becomes a rotation
of -yaw about Blender's Z.
"""

import math
import os
import struct
import sys

import bpy
import mathutils

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------------------------------------------------------------------------
# .map reader — mirrors Game/map_io.h. Keep in step with MAP_VERSION.
# ---------------------------------------------------------------------------
def read_map(path):
    b = open(path, "rb").read()
    magic, version, checksum, coff, csize, voff, vsize, toff, tsize = \
        struct.unpack_from("<4sIIIIIIII", b, 0)
    if magic != b"TMAP":
        raise SystemExit(f"{path}: not a TMAP file")
    name = b[36:100].split(b"\0")[0].decode()

    cur = coff
    (n_aabb,) = struct.unpack_from("<I", b, cur); cur += 4
    aabbs = []
    for _ in range(n_aabb):
        aabbs.append(struct.unpack_from("<6fB3x", b, cur)); cur += 28
    (n_spawn,) = struct.unpack_from("<I", b, cur); cur += 4
    spawns = []
    for _ in range(n_spawn):
        spawns.append(struct.unpack_from("<4fB3x", b, cur)); cur += 20

    cur = voff
    (n_model,) = struct.unpack_from("<I", b, cur); cur += 4
    models = []
    for _ in range(n_model):
        asset = b[cur:cur + 64].split(b"\0")[0].decode()
        pos = struct.unpack_from("<3f", b, cur + 64)
        rot = struct.unpack_from("<3f", b, cur + 76)
        scl = struct.unpack_from("<3f", b, cur + 88)
        (tex,) = struct.unpack_from("<I", b, cur + 100)
        cur += 108
        models.append((asset, pos, rot, scl, tex))
    (n_light,) = struct.unpack_from("<I", b, cur); cur += 4
    lights = []
    for _ in range(n_light):
        lights.append((b[cur], struct.unpack_from("<3f", b, cur + 4),
                       struct.unpack_from("<3f", b, cur + 16),
                       struct.unpack_from("<3f", b, cur + 28),
                       struct.unpack_from("<f", b, cur + 40)[0]))
        cur += 44
    return dict(name=name, checksum=checksum, aabbs=aabbs, spawns=spawns,
                models=models, lights=lights)


# ---------------------------------------------------------------------------
# Scene construction
# ---------------------------------------------------------------------------
def fresh_scene(name="MAP_PREVIEW"):
    sc = bpy.data.scenes.get(name) or bpy.data.scenes.new(name)
    bpy.context.window.scene = sc
    for ob in list(sc.collection.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    return sc


_TEMPLATES = {}


def build_templates(sc, data):
    """Import every distinct asset once, before anything is placed.

    Templates must all be built up front: bpy.ops.object.join() acts on the
    selection, so importing a multi-mesh asset while placed copies are already
    in the scene merges them into the template.
    """
    for asset, *_ in data["models"]:
        if asset != "__box__":
            template_for(sc, asset)


def template_for(sc, asset_path):
    """Import an asset once and keep it off to the side; every placement is a
    linked copy of it, so a 200-object map does not re-parse 200 FBX."""
    if asset_path in _TEMPLATES:
        return _TEMPLATES[asset_path]
    full = os.path.join(REPO, asset_path)
    if not os.path.exists(full):
        _TEMPLATES[asset_path] = None
        return None
    bpy.ops.object.select_all(action='DESELECT')
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=full)
    new = [o for o in set(bpy.data.objects) - before if o.type == 'MESH']
    for o in set(bpy.data.objects) - before:
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    if not new:
        _TEMPLATES[asset_path] = None
        return None
    if len(new) > 1:
        bpy.ops.object.select_all(action='DESELECT')
        for o in new:
            o.select_set(True)
        bpy.context.view_layer.objects.active = new[0]
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active or new[0]
    # Bake the import transform into the mesh. Blender's importer expresses a
    # centimetre-authored asset as a metre-scale mesh under an 0.01 object
    # scale; placements copy the mesh, so the scale has to live in the vertices
    # or every copy comes out a hundred times too big.
    ob.data.transform(ob.matrix_world)
    ob.matrix_world = mathutils.Matrix.Identity(4)
    ob.name = "TPL_" + os.path.basename(asset_path)
    ob.hide_render = True
    ob.hide_viewport = True
    _TEMPLATES[asset_path] = ob
    return ob


def build(sc, data):
    ground_mat = bpy.data.materials.new("preview_ground")
    ground_mat.use_nodes = True
    ground_mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = \
        (0.17, 0.17, 0.165, 1.0)

    build_templates(sc, data)

    placed, missing = 0, set()
    for asset, pos, rot, scl, tex in data["models"]:
        # Engine Y-up -> Blender Z-up.
        loc = (pos[0], pos[2], pos[1])
        rz = -rot[1]
        if asset == "__box__":
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
            ob = bpy.context.active_object
            ob.scale = (scl[0], scl[2], scl[1])
            ob.data.materials.append(ground_mat)
            placed += 1
            continue
        tpl = template_for(sc, asset)
        if tpl is None:
            missing.add(asset)
            continue
        ob = tpl.copy()
        ob.data = tpl.data           # share the mesh; this is a preview
        ob.hide_render = False
        ob.hide_viewport = False
        sc.collection.objects.link(ob)
        ob.location = loc
        ob.rotation_euler = (0.0, 0.0, rz)
        ob.scale = (1.0, 1.0, 1.0)   # Blender's importer already applied units
        placed += 1

    # Spawn markers: a cone per point, red or blue, pointing where it faces.
    for x, y, z, yaw, team in data["spawns"]:
        bpy.ops.mesh.primitive_cone_add(radius1=0.32, depth=1.7,
                                        location=(x, z, y + 0.85))
        c = bpy.context.active_object
        c.name = f"spawn_{'RED' if team == 0 else 'BLUE'}"
        m = bpy.data.materials.new(c.name)
        m.use_nodes = True
        m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = \
            (0.8, 0.05, 0.05, 1.0) if team == 0 else (0.05, 0.2, 0.9, 1.0)
        c.data.materials.append(m)

    return placed, sorted(missing)


def add_lighting(sc, data):
    for ltype, pos, direction, colour, intensity in data["lights"]:
        if ltype == 0:
            bpy.ops.object.light_add(type='SUN', location=(0, 0, 30))
            lt = bpy.context.active_object
            lt.data.energy = 3.0 * max(intensity, 0.1)
            lt.data.color = colour
            d = mathutils.Vector((direction[0], direction[2], direction[1]))
            if d.length > 0:
                lt.rotation_euler = (-d).to_track_quat('Z', 'Y').to_euler()
        else:
            bpy.ops.object.light_add(type='POINT', location=(pos[0], pos[2], pos[1]))
            lt = bpy.context.active_object
            lt.data.energy = 400.0 * max(intensity, 0.1)
            lt.data.color = colour
    sc.world = bpy.data.worlds.new("preview_world") if sc.world is None else sc.world
    sc.world.use_nodes = True
    sc.world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.45, 0.5, 0.56, 1)
    sc.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.1


def camera(sc, location, target, ortho_scale=None, lens=35.0):
    cam = bpy.data.objects.get("PREVIEW_CAM")
    if cam is None:
        cam = bpy.data.objects.new("PREVIEW_CAM", bpy.data.cameras.new("PREVIEW_CAM"))
        sc.collection.objects.link(cam)
    cam.data.type = 'ORTHO' if ortho_scale else 'PERSP'
    if ortho_scale:
        cam.data.ortho_scale = ortho_scale
    else:
        cam.data.lens = lens
    cam.location = mathutils.Vector(location)
    cam.rotation_euler = (mathutils.Vector(target) - cam.location) \
        .to_track_quat('-Z', 'Y').to_euler()
    sc.camera = cam
    return cam


def render(sc, path, res=(1600, 1200)):
    # The realtime engine is named differently across Blender versions; take
    # whichever this build accepts and fall back to Workbench.
    for engine in ('BLENDER_EEVEE_NEXT', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'):
        try:
            sc.render.engine = engine
            break
        except TypeError:
            continue
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.resolution_percentage = 100
    sc.render.filepath = path
    sc.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True)
    return path


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["shipment"]
    map_name = argv[0]
    out_dir = argv[1] if len(argv) > 1 else os.path.join(REPO, "build_tmp")
    os.makedirs(out_dir, exist_ok=True)

    data = read_map(os.path.join(REPO, "resource", "maps", map_name + ".map"))
    print(f"{map_name}.map  checksum {data['checksum']:08x}  "
          f"models {len(data['models'])}  colliders {len(data['aabbs'])}  "
          f"spawns {len(data['spawns'])}")

    sc = fresh_scene()
    placed, missing = build(sc, data)
    add_lighting(sc, data)
    print(f"placed {placed} objects" + (f", MISSING {missing}" if missing else ""))

    camera(sc, (0, 0, 60), (0, 0, 0), ortho_scale=40)
    render(sc, os.path.join(out_dir, f"{map_name}_top.png"), (1400, 1400))

    camera(sc, (26, -30, 20), (0, 0, 2), lens=40)
    render(sc, os.path.join(out_dir, f"{map_name}_iso.png"), (1600, 1000))

    camera(sc, (-14.8, -13.2, 1.7), (0, 0, 1.6), lens=28)
    render(sc, os.path.join(out_dir, f"{map_name}_spawn.png"), (1600, 900))

    print("previews written to", out_dir)


if __name__ == "__main__":
    main()
