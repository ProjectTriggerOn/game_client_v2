#!/usr/bin/env python3
"""Author and re-export every FBX resource/maps/shipment.map depends on.

Run headless from the client repo root:

    blender --background --python tools/blender_build_assets.py

It does two jobs:

1. Builds a low-poly ISO 20 ft shipping container in four colours. The Low Poly
   Shooter Pack has no container, and Shipment is made of nothing else, so the
   asset is authored here rather than faked with a textured cube. Flat material
   colours match the pack's look; ModelDraw renders an untextured material as
   the white texture tinted by AI_MATKEY_COLOR_DIFFUSE.

2. Re-exports the pack's environment props with their palette atlas attached.
   The pack's FBX reference no texture at all, so straight out of the box every
   prop renders white in this engine. Importing through Blender and re-exporting
   also normalises the pack's mixed axis conventions: five assets (the barrel,
   the gas tank, the scaffolding and the two houses) arrive on their back
   otherwise, because ModelDraw does not walk the node hierarchy.

Two Blender pitfalls this script avoids, both of which produced silently wrong
assets on the way here:

  * ``bpy.ops.object.transform_apply`` re-applied the object scale of the
    centimetre-authored assets, shrinking them a hundredfold. The world matrix
    is baked into the vertices explicitly instead.
  * ``material.diffuse_color`` is a viewport-only property and is not exported.
    The FBX diffuse colour comes from the Principled BSDF's Base Color.

Verify the output with tools/fbx_probe (bounds) and the material dump: every
asset must come back upright, with its base near y = 0 or symmetric about it,
and props must reference their atlas by bare filename.
"""

import math
import os
import sys

import bpy
import mathutils

# --------------------------------------------------------------------------
# Paths. The atlases have to sit beside the FBX: ModelLoad resolves a sidecar
# texture as <directory of the FBX>/<filename in the material>.
# --------------------------------------------------------------------------
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "resource", "model")


def _find_pack(start):
    """The pack sits beside the client checkout, but a git worktree nests one
    level deeper than a normal clone, so walk up until it turns up."""
    d = start
    for _ in range(5):
        d = os.path.dirname(d)
        candidate = os.path.join(d, "Low Poly Shooter Pack")
        if os.path.isdir(candidate):
            return candidate
    raise SystemExit("cannot find 'Low Poly Shooter Pack' above " + start)


PACK_ROOT = _find_pack(REPO)
PACK = os.path.join(PACK_ROOT, "Art", "Meshes", "Environment")
PACK_TEX = os.path.join(PACK_ROOT, "Art", "Textures", "Environment")

# T_Demo_Warzone_D is the pack's environment atlas. Two props are placed
# directly in a demo scene rather than through a prefab, and the scene YAML
# assigns both of them that atlas; sampling each prop's UVs against every
# candidate agrees (a wooden crate reads brown there and pure white against
# T_TP_CH_Props_D, which is the third-person *character* props palette).
# The other three are named by the FBX's own material, which is authoritative.
PROPS = "T_Demo_Warzone_D.png"
EXPL = "T_Explosives_D.png"          # FBX material "Explosive Props"
DESERT = "T_Demo_Desert_D.png"       # FBX material "M_Desert_Demo_Map"
PLAY = "T_Demo_Playground_D.png"     # FBX material "Playground_Demo_Map"
ATLASES = (PROPS, EXPL, DESERT, PLAY)

# Atlas per asset. Keyed by asset rather than by material name because Blender
# creates no material at all for the pack's "DefaultMaterial" meshes.
ASSETS = [
    ("Objects/Boxes", "SM_Box_Wood_02", PROPS),
    ("Objects/Boxes", "SM_Box_Ammo", PROPS),
    ("Objects/SandBags", "SM_Sandbag", PROPS),
    ("Objects/Explosives", "SM_Explosive_Barrel", EXPL),
    ("Objects/Explosives", "SM_Explosive_Gas_Tank", EXPL),
    ("Objects/Lamps", "SM_Lamp_Construction_001", PROPS),
    ("Objects/Lamps", "SM_Lamp_Construction_003", PROPS),
    ("Objects/Fences", "SM_Wire_Barbed", PROPS),
    ("Objects/Fences", "SM_Fence_Wood_002", PROPS),
    ("Objects/Shutters", "SM_Shutter_Large_01", PROPS),
    ("Objects/Tubes", "SM_Tubes_005", PROPS),
    ("Objects/Planks", "SM_Plank_Wooden_001", PROPS),
    ("Objects/Planks", "SM_Plank_Wooden_Big_01", PROPS),
    ("Objects/Tables", "SM_Workbench", PROPS),
    ("Nature", "SM_Stone_008", PROPS),
    ("Buildings", "SM_Wall_001", PROPS),
    ("Buildings", "SM_Wall_002", PROPS),
    ("Buildings", "SM_Wall_008", PROPS),
    ("Buildings", "SM_Watchtower", PROPS),
    ("Buildings", "SM_Scaffolding", PLAY),
    ("Buildings", "SM_House_Small", DESERT),
    ("Buildings", "SM_House_Standard", DESERT),
]

# ISO 20 ft container, and the trim that gives it a silhouette.
CL, CW, CH = 6.06, 2.44, 2.59
RIB_PITCH, RIB_DEPTH, RIB_WIDTH = 0.2757, 0.035, 0.10
CAST, RAIL = 0.16, 0.11

CONTAINERS = [
    ("SM_Container_Red",   (0.404, 0.129, 0.106), (0.176, 0.157, 0.149)),
    ("SM_Container_Blue",  (0.106, 0.204, 0.325), (0.149, 0.157, 0.176)),
    ("SM_Container_Green", (0.129, 0.259, 0.161), (0.141, 0.161, 0.141)),
    ("SM_Container_Rust",  (0.353, 0.212, 0.125), (0.204, 0.153, 0.114)),
]

# Wooden crates, authored because the pack's boxes top out around half a metre.
#   large — spawn cover. 1.81 m breaks a standing sightline and stays above the
#           1.6 m jump, so it cannot become a perch overlooking a spawn.
#   small — the step onto a container roof. Square in plan so a 90 degree yaw
#           does not change how it sits flush against a container face, and
#           1.30 m wide so there is room to actually stand on it (the heap it
#           replaces was only 0.66 m deep, narrower than the player).
#           1.30 m up plus a 1.6 m jump clears the 2.59 m roof.
CRATES = [
    #  name               W     D     H    frame
    ("SM_Crate_Large",  1.80, 1.60, 1.80, 0.10),
    ("SM_Crate_Small",  1.30, 1.30, 1.30, 0.08),
]
CRATE_BODY_RGB = (0.353, 0.235, 0.129)
CRATE_FRAME_RGB = (0.220, 0.141, 0.075)

# bake_anim=False is not optional. It defaults True, and with
# bake_anim_use_all_actions it writes every action present in the Blender
# session into each exported file — importing the pack's rigged assets earlier
# in a session is enough to put ~73,000 AnimationCurve nodes into a 28-triangle
# wall, taking it from 20 KB to 33 MB. These are static props; they have no
# animation to carry.
EXPORT = dict(use_selection=False, apply_unit_scale=True, global_scale=1.0,
              axis_forward='-Z', axis_up='Y', object_types={'MESH'},
              use_mesh_modifiers=True, mesh_smooth_type='FACE',
              bake_space_transform=False, path_mode='STRIP',
              bake_anim=False, use_custom_props=False)


def scene():
    sc = bpy.data.scenes.get("SHIPMENT_BUILD") or bpy.data.scenes.new("SHIPMENT_BUILD")
    bpy.context.window.scene = sc
    return sc


def clear(sc):
    for ob in list(sc.collection.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def join_objects(parts, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:
        o.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = name
    return ob


def flat_material(name, rgb):
    """Untextured flat colour. The colour must live on the Principled BSDF —
    material.diffuse_color is viewport-only and never reaches the FBX."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (rgb[0], rgb[1], rgb[2], 1.0)
    bsdf.inputs["Roughness"].default_value = 0.85
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    m.diffuse_color = (rgb[0], rgb[1], rgb[2], 1.0)
    return m


def atlas_material(name, atlas_file):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    tex = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.get(atlas_file)
    if img is None:
        img = bpy.data.images.load(os.path.join(OUT, atlas_file))
    tex.image = img
    tex.interpolation = 'Closest'          # palette cells must not blend
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return m


def add_box(cx, cy, cz, sx, sy, sz):
    """A box whose scale is baked into its vertices immediately, so nothing
    downstream can apply it a second time."""
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(cx, cy, cz))
    o = bpy.context.active_object
    o.data.transform(mathutils.Matrix.Diagonal((sx, sy, sz, 1.0)))
    return o


def build_crate(sc, name, W, D, H, F):
    """A big wooden shipping crate: planked body, corner posts, top and bottom
    rails, and a diagonal brace per face. Replaces a heap of small cargo boxes
    that was placed at assorted yaws — the heap looked slanted while its AABB
    collider was square, so the two never agreed."""
    clear(sc)
    body, frame = [], []

    # Planked body, inset so the frame stands proud of it.
    planks = 5
    ph = (H - 2 * F) / planks
    for i in range(planks):
        z = F + ph * (i + 0.5)
        body.append(add_box(0, 0, z, W - 2 * F, D - 2 * F, ph - 0.02))

    # Four corner posts.
    for sx in (-1, 1):
        for sy in (-1, 1):
            frame.append(add_box(sx * (W * 0.5 - F * 0.5), sy * (D * 0.5 - F * 0.5),
                                 H * 0.5, F, F, H))
    # Top and bottom rails all the way round.
    for z in (F * 0.5, H - F * 0.5):
        frame.append(add_box(0, 0, z, W, F, F * 1.1))
        frame.append(add_box(0, 0, z, W, F, F * 1.1))
        for sy in (-1, 1):
            frame.append(add_box(0, sy * (D * 0.5 - F * 0.5), z, W, F, F))
        for sx in (-1, 1):
            frame.append(add_box(sx * (W * 0.5 - F * 0.5), 0, z, F, D, F))

    # One diagonal brace per face, the detail that makes it read as a crate.
    # add_box leaves the mesh centred on its own origin and puts the placement
    # on the object, so the brace is rotated about that origin directly — an
    # extra translate-to-origin round trip would push it out of the crate.
    for sy in (-1, 1):
        length = math.hypot(W - 2 * F, H - 2 * F)
        o = add_box(0, sy * (D * 0.5 - F * 0.5), H * 0.5, length, F, F * 0.9)
        o.data.transform(mathutils.Matrix.Rotation(
            math.atan2(H - 2 * F, W - 2 * F) * sy, 4, 'Y'))
        frame.append(o)
    for sx in (-1, 1):
        length = math.hypot(D - 2 * F, H - 2 * F)
        o = add_box(sx * (W * 0.5 - F * 0.5), 0, H * 0.5, F, length, F * 0.9)
        o.data.transform(mathutils.Matrix.Rotation(
            math.atan2(H - 2 * F, D - 2 * F) * sx, 4, 'X'))
        frame.append(o)

    b = join_objects(body, "crate_body")
    b.data.materials.clear()
    b.data.materials.append(flat_material(name + "_body", CRATE_BODY_RGB))
    for p in b.data.polygons:
        p.material_index = 0

    f = join_objects(frame, "crate_frame")
    f.data.materials.clear()
    f.data.materials.append(flat_material(name + "_frame", CRATE_FRAME_RGB))
    for p in f.data.polygons:
        p.material_index = 0

    ob = join_objects([b, f], name)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), **EXPORT)
    return len(ob.data.materials)


def build_container(sc, name, body_rgb, trim_rgb):
    clear(sc)
    body, trim = [], []

    body.append(add_box(0, 0, CH * 0.5, CL - 2 * CAST, CW - 2 * RIB_DEPTH, CH - 2 * RAIL))
    ribs = int((CL - 2 * CAST) / RIB_PITCH)
    span = (ribs - 1) * RIB_PITCH
    for i in range(ribs):
        x = -span * 0.5 + i * RIB_PITCH
        for sy in (-1, 1):
            body.append(add_box(x, sy * (CW * 0.5 - RIB_DEPTH * 0.5), CH * 0.5,
                                RIB_WIDTH, RIB_DEPTH, CH - 2 * RAIL))
    for sx in (-1, 1):
        body.append(add_box(sx * (CL * 0.5 - CAST * 0.5), 0, CH * 0.5,
                            CAST, CW - 2 * RIB_DEPTH, CH - 2 * RAIL))

    # Door leaves and locking bars, kept inside the 6.06 m envelope so
    # containers can be butted together without the bars poking through.
    for sy in (-1, 1):
        trim.append(add_box(CL * 0.5 - 0.06, sy * CW * 0.245, CH * 0.5,
                            0.08, CW * 0.44, CH - 2.6 * RAIL))
        for k in (-1, 1):
            trim.append(add_box(CL * 0.5 - 0.025, sy * CW * 0.245 + k * 0.24,
                                CH * 0.5, 0.05, 0.06, CH - 3.0 * RAIL))
    for level in (0, 1):
        z = RAIL * 0.5 if level == 0 else CH - RAIL * 0.5
        trim.append(add_box(0, 0, z, CL, CW, RAIL))
    for sx in (-1, 1):
        for sy in (-1, 1):
            for level in (0, 1):
                z = CAST * 0.5 if level == 0 else CH - CAST * 0.5
                trim.append(add_box(sx * (CL * 0.5 - CAST * 0.5),
                                    sy * (CW * 0.5 - CAST * 0.5), z,
                                    CAST, CAST, CAST))

    b = join_objects(body, "body")
    b.data.materials.clear()
    b.data.materials.append(flat_material(name + "_body", body_rgb))
    for p in b.data.polygons:
        p.material_index = 0

    t = join_objects(trim, "trim")
    t.data.materials.clear()
    t.data.materials.append(flat_material(name + "_trim", trim_rgb))
    for p in t.data.polygons:
        p.material_index = 0

    ob = join_objects([b, t], name)
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), **EXPORT)
    return len(ob.data.materials)


def prep_asset(sc, folder, name, atlas):
    clear(sc)
    src = os.path.join(PACK, folder.replace("/", os.sep), name + ".fbx")
    if not os.path.exists(src):
        return "missing source"
    bpy.ops.import_scene.fbx(filepath=src)
    meshes = [o for o in sc.collection.objects if o.type == 'MESH']
    if not meshes:
        return "no mesh"

    # Bake the world matrix straight into the vertices. transform_apply would
    # re-apply the object scale the importer already accounts for.
    for o in meshes:
        o.data.transform(o.matrix_world)
        o.matrix_world = mathutils.Matrix.Identity(4)

    ob = join_objects(meshes, name)
    ob.data.materials.clear()
    ob.data.materials.append(atlas_material(name + "_mat", atlas))
    for p in ob.data.polygons:
        p.material_index = 0

    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), **EXPORT)
    return None


def main():
    os.makedirs(OUT, exist_ok=True)
    missing = [a for a in ATLASES if not os.path.exists(os.path.join(OUT, a))]
    if missing:
        for a in missing:
            src = os.path.join(PACK_TEX, a)
            if os.path.exists(src):
                import shutil
                shutil.copy2(src, os.path.join(OUT, a))
                print("copied atlas", a)
            else:
                print("MISSING atlas", a, "expected at", src)

    sc = scene()
    for name, body_rgb, trim_rgb in CONTAINERS:
        slots = build_container(sc, name, body_rgb, trim_rgb)
        print(f"container {name}: {slots} material slots")
    for name, cw, cd, ch, cf in CRATES:
        print(f"crate {name}: {build_crate(sc, name, cw, cd, ch, cf)} material slots")

    failures = []
    for folder, name, atlas in ASSETS:
        err = prep_asset(sc, folder, name, atlas)
        print(f"prop {name}: {'ok' if err is None else err}  ({atlas})")
        if err:
            failures.append(name)
    clear(sc)

    print(f"\n{len(CONTAINERS)} containers + {len(ASSETS) - len(failures)} props written to {OUT}")
    if failures:
        print("FAILED:", ", ".join(failures))
        sys.exit(1)


if __name__ == "__main__":
    main()
