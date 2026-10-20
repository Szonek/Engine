#!/usr/bin/env python3
"""Generates the models in this folder: tiny glTF 2.0 files the engine tests load instead of the game's real models
(which will be replaced), so a test never breaks because an artist changed an asset. Run it from anywhere:

    python generate_test_models.py

The output is deterministic (running it again changes no file) and uses only the standard library. The files are
committed; edit this script and regenerate them rather than editing them by hand. The models:

  crate.gltf              static: a root with two children sharing one mesh, one plain coloured material
  textured_a.gltf         one node with a texture in its own file (test_texture.png) ...
  textured_b.gltf         ... which this model uses too
  embedded_texture.gltf   one node with a texture embedded in the model
  skinned_arm.gltf        skin "Rig" (joints base -> tip), animations "Wave" (1 s) and "Idle" (0.5 s), and a
                          child node of the tip joint, which is attached to that joint
  two_skins.gltf          broken: two skins (unsupported)
  duplicate_meshes.gltf   broken: two meshes with the same name (their asset IDs collide)
  not_a_model.gltf        broken: not a glTF file
"""
import base64
import json
import math
import os
import struct
import zlib

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

FLOAT, UBYTE, USHORT = 5126, 5121, 5123
ARRAY_BUFFER, ELEMENT_ARRAY_BUFFER = 34962, 34963


class Gltf:
    """Collects the buffer data of a model; accessors are added with the helpers below."""

    def __init__(self):
        self.data = bytearray()
        self.buffer_views = []
        self.accessors = []

    def accessor(self, values, component_type, type_name, target=None, with_range=False):
        """values: a list of tuples (or numbers for SCALAR). Returns the accessor index."""
        fmt = {FLOAT: "f", UBYTE: "B", USHORT: "H"}[component_type]
        flat = []
        for value in values:
            flat.extend(value if isinstance(value, tuple) else (value,))
        while len(self.data) % 4:
            self.data.append(0)
        offset = len(self.data)
        self.data += struct.pack("<%d%s" % (len(flat), fmt), *flat)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(self.data) - offset}
        if target:
            view["target"] = target
        self.buffer_views.append(view)
        components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[type_name]
        accessor = {"bufferView": len(self.buffer_views) - 1, "componentType": component_type,
                    "count": len(flat) // components, "type": type_name}
        if with_range:
            columns = list(zip(*[flat[i:i + components] for i in range(0, len(flat), components)]))
            accessor["min"] = [min(c) for c in columns]
            accessor["max"] = [max(c) for c in columns]
        self.accessors.append(accessor)
        return len(self.accessors) - 1

    def document(self, **fields):
        doc = {"asset": {"version": "2.0", "generator": "generate_test_models.py"}}
        doc.update(fields)
        doc["buffers"] = [{"byteLength": len(self.data),
                           "uri": "data:application/octet-stream;base64," + base64.b64encode(bytes(self.data)).decode()}]
        doc["bufferViews"] = self.buffer_views
        doc["accessors"] = self.accessors
        return doc


def write(name, text):
    with open(os.path.join(OUT_DIR, name), "w", newline="\n") as f:
        f.write(text)


def write_json(name, doc):
    write(name, json.dumps(doc, indent=1, sort_keys=True) + "\n")


def png(width, height, pixel):
    """An RGBA PNG. The pixel data is stored (not compressed), so the bytes don't depend on the zlib version."""
    raw = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, payload):
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 0)) + chunk(b"IEND", b""))


def checker(x, y):
    return (255, 255, 255, 255) if (x + y) % 2 == 0 else (40, 40, 200, 255)


# --- meshes ---------------------------------------------------------------------------------------------------------

QUAD_POSITIONS = [(-0.5, 0.0, 0.0), (0.5, 0.0, 0.0), (0.5, 1.0, 0.0), (-0.5, 1.0, 0.0)]
QUAD_NORMALS = [(0.0, 0.0, 1.0)] * 4
QUAD_UVS = [(0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)]
QUAD_INDICES = [0, 1, 2, 0, 2, 3]


def quad_primitive(g, material=None, skinned=False, uvs=False):
    attributes = {
        "POSITION": g.accessor(QUAD_POSITIONS, FLOAT, "VEC3", ARRAY_BUFFER, with_range=True),
        "NORMAL": g.accessor(QUAD_NORMALS, FLOAT, "VEC3", ARRAY_BUFFER),
    }
    if uvs:
        attributes["TEXCOORD_0"] = g.accessor(QUAD_UVS, FLOAT, "VEC2", ARRAY_BUFFER)
    if skinned:
        # the two lower vertices follow joint 0 (base), the two upper ones joint 1 (tip)
        attributes["JOINTS_0"] = g.accessor([(0, 0, 0, 0), (0, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 0)], UBYTE, "VEC4", ARRAY_BUFFER)
        attributes["WEIGHTS_0"] = g.accessor([(1.0, 0.0, 0.0, 0.0)] * 4, FLOAT, "VEC4", ARRAY_BUFFER)
    primitive = {"attributes": attributes, "indices": g.accessor(QUAD_INDICES, USHORT, "SCALAR", ELEMENT_ARRAY_BUFFER)}
    if material is not None:
        primitive["material"] = material
    return primitive


def triangle_primitive(g, material=None):
    positions = [(0.0, 0.0, 0.0), (0.2, 0.0, 0.0), (0.0, 0.2, 0.0)]
    primitive = {"attributes": {"POSITION": g.accessor(positions, FLOAT, "VEC3", ARRAY_BUFFER, with_range=True),
                                "NORMAL": g.accessor([(0.0, 0.0, 1.0)] * 3, FLOAT, "VEC3", ARRAY_BUFFER)},
                 "indices": g.accessor([0, 1, 2], USHORT, "SCALAR", ELEMENT_ARRAY_BUFFER)}
    if material is not None:
        primitive["material"] = material
    return primitive


def plain_material(name, color):
    return {"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(color), "metallicFactor": 0.0}}


# --- models ---------------------------------------------------------------------------------------------------------

def crate():
    g = Gltf()
    doc = g.document(
        scene=0, scenes=[{"nodes": [0]}],
        nodes=[{"name": "Crate", "children": [1, 2]},
               {"name": "Lid", "mesh": 0, "translation": [0.0, 1.0, 0.0]},
               {"name": "Base", "mesh": 0}],
        meshes=[{"name": "Plate", "primitives": [quad_primitive(g, material=0)]}],
        materials=[plain_material("wood", (0.6, 0.4, 0.2, 1.0))])
    write_json("crate.gltf", doc)


def textured(file_name, image):
    g = Gltf()
    doc = g.document(
        scene=0, scenes=[{"nodes": [0]}],
        nodes=[{"name": "Panel", "mesh": 0}],
        meshes=[{"name": "Plate", "primitives": [quad_primitive(g, material=0, uvs=True)]}],
        materials=[{"name": "painted", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0.0}}],
        textures=[{"source": 0}], images=[image])
    write_json(file_name, doc)


def textures():
    png_bytes = png(4, 4, checker)
    with open(os.path.join(OUT_DIR, "test_texture.png"), "wb") as f:
        f.write(png_bytes)
    textured("textured_a.gltf", {"uri": "test_texture.png"})
    textured("textured_b.gltf", {"uri": "test_texture.png"})
    textured("embedded_texture.gltf", {"name": "checker", "mimeType": "image/png",
                                       "uri": "data:image/png;base64," + base64.b64encode(png_bytes).decode()})


def rotation_z(degrees):
    half = math.radians(degrees) / 2.0
    return (0.0, 0.0, round(math.sin(half), 6), round(math.cos(half), 6))


def skinned_nodes(with_sword=False):
    """The model root "Rig" (it owns the skin) with the skinned mesh and the root joint. Joint nodes come first, the
    root joint (base) last of them: the skeleton is built from the joint with the highest node index."""
    nodes = [{"name": "arm_mesh", "mesh": 0, "skin": 0},       # 0
             {"name": "tip", "translation": [0.0, 1.0, 0.0]},  # 1
             {"name": "base", "children": [1]},                # 2
             {"name": "Rig", "children": [0, 2]}]              # 3
    if with_sword:
        nodes.append({"name": "sword", "mesh": 1, "translation": [0.0, 0.1, 0.0]})  # 4
        nodes[1]["children"] = [4]  # attached to the tip joint
    return nodes


def inverse_bind_matrices(g):
    identity = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
    tip = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 1.0)
    return g.accessor([identity, tip], FLOAT, "MAT4")  # joints [base, tip]


def skinned_arm():
    g = Gltf()
    ibm = inverse_bind_matrices(g)
    meshes = [{"name": "Arm", "primitives": [quad_primitive(g, material=0, skinned=True)]},
              {"name": "Sword", "primitives": [triangle_primitive(g, material=0)]}]

    def clip(name, node, path, times, values):
        return {"name": name,
                "samplers": [{"input": g.accessor(times, FLOAT, "SCALAR", with_range=True),
                              "output": g.accessor(values, FLOAT, "VEC4" if path == "rotation" else "VEC3"),
                              "interpolation": "LINEAR"}],
                "channels": [{"sampler": 0, "target": {"node": node, "path": path}}]}

    animations = [clip("Wave", 1, "rotation", [0.0, 0.5, 1.0], [rotation_z(0), rotation_z(90), rotation_z(0)]),
                  clip("Idle", 2, "translation", [0.0, 0.25, 0.5], [(0.0, 0.0, 0.0), (0.0, 0.1, 0.0), (0.0, 0.0, 0.0)])]
    doc = g.document(
        scene=0, scenes=[{"nodes": [3]}], nodes=skinned_nodes(with_sword=True), meshes=meshes,
        materials=[plain_material("metal", (0.7, 0.7, 0.75, 1.0))],
        skins=[{"name": "Rig", "joints": [2, 1], "inverseBindMatrices": ibm}],
        animations=animations)
    write_json("skinned_arm.gltf", doc)


def two_skins():
    g = Gltf()
    ibm = inverse_bind_matrices(g)
    nodes = skinned_nodes()
    nodes.append({"name": "arm_mesh_2", "mesh": 0, "skin": 1})  # 4
    nodes[3]["children"] = [0, 2, 4]
    doc = g.document(
        scene=0, scenes=[{"nodes": [3]}], nodes=nodes,
        meshes=[{"name": "Arm", "primitives": [quad_primitive(g, material=0, skinned=True)]}],
        materials=[plain_material("metal", (0.7, 0.7, 0.75, 1.0))],
        skins=[{"name": "Rig", "joints": [2, 1], "inverseBindMatrices": ibm},
               {"name": "Rig2", "joints": [2, 1], "inverseBindMatrices": ibm}])
    write_json("two_skins.gltf", doc)


def duplicate_meshes():
    g = Gltf()
    doc = g.document(
        scene=0, scenes=[{"nodes": [0]}],
        nodes=[{"name": "Pair", "children": [1, 2]}, {"name": "First", "mesh": 0}, {"name": "Second", "mesh": 1}],
        meshes=[{"name": "Same", "primitives": [quad_primitive(g, material=0)]},
                {"name": "Same", "primitives": [quad_primitive(g, material=0)]}],
        materials=[plain_material("wood", (0.6, 0.4, 0.2, 1.0))])
    write_json("duplicate_meshes.gltf", doc)


if __name__ == "__main__":
    crate()
    textures()
    skinned_arm()
    two_skins()
    duplicate_meshes()
    write("not_a_model.gltf", "this is not a glTF file\n")
