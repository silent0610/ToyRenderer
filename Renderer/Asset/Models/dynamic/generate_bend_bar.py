"""Generate a watertight 2-bone skinned cylinder for dynamic SDF smoke tests.

This is a pipeline test asset, not a paper experiment case.
"""
from __future__ import annotations

import json
import math
import struct
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parent
RADIUS = 0.22
Y_MIN = -0.8
Y_MAX = 0.8
RINGS = 12
SLICES = 24


def align4(n: int) -> int:
    return (n + 3) & ~3


def quat_z(angle: float) -> tuple[float, float, float, float]:
    half = angle * 0.5
    return (0.0, 0.0, math.sin(half), math.cos(half))


def build_mesh() -> tuple[list, list]:
    positions: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    joints: list[tuple[int, int, int, int]] = []
    weights: list[tuple[float, float, float, float]] = []
    height = Y_MAX - Y_MIN

    def add_vertex(x: float, y: float, z: float, nx: float, ny: float, nz: float) -> int:
        t = (y - Y_MIN) / height
        t = min(1.0, max(0.0, t))
        positions.append((x, y, z))
        normals.append((nx, ny, nz))
        joints.append((0, 1, 0, 0))
        weights.append((1.0 - t, t, 0.0, 0.0))
        return len(positions) - 1

    rings: list[list[int]] = []
    for iy in range(SLICES + 1):
        y = Y_MIN + height * iy / SLICES
        ring = []
        for ir in range(RINGS):
            theta = 2.0 * math.pi * ir / RINGS
            c, s = math.cos(theta), math.sin(theta)
            ring.append(add_vertex(RADIUS * c, y, RADIUS * s, c, 0.0, s))
        rings.append(ring)

    indices: list[int] = []
    for iy in range(SLICES):
        for ir in range(RINGS):
            irn = (ir + 1) % RINGS
            a = rings[iy][ir]
            b = rings[iy][irn]
            c = rings[iy + 1][irn]
            d = rings[iy + 1][ir]
            indices.extend([a, c, b, a, d, c])

    bottom = add_vertex(0.0, Y_MIN, 0.0, 0.0, -1.0, 0.0)
    top = add_vertex(0.0, Y_MAX, 0.0, 0.0, 1.0, 0.0)
    for ir in range(RINGS):
        irn = (ir + 1) % RINGS
        indices.extend([bottom, rings[0][ir], rings[0][irn]])
        indices.extend([top, rings[-1][irn], rings[-1][ir]])

    vertices = {
        "positions": positions,
        "normals": normals,
        "joints": joints,
        "weights": weights,
    }
    return vertices, indices


def pack_bin(vertices, indices: list[int]) -> tuple[bytes, dict]:
    chunks: list[bytes] = []
    views = []

    def add(data: bytes, target: int | None) -> int:
        offset = sum(len(c) for c in chunks)
        pad = align4(offset) - offset
        if pad:
            chunks.append(b"\x00" * pad)
            offset += pad
        chunks.append(data)
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        views.append(view)
        return len(views) - 1

    pos = b"".join(struct.pack("<3f", *p) for p in vertices["positions"])
    nrm = b"".join(struct.pack("<3f", *n) for n in vertices["normals"])
    jnt = b"".join(struct.pack("<4H", *j) for j in vertices["joints"])
    wgt = b"".join(struct.pack("<4f", *w) for w in vertices["weights"])
    idx = b"".join(struct.pack("<I", i) for i in indices)

    ibm_root = [
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0.8, 0, 1,
    ]
    ibm_bend = [
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    ]
    ibm = struct.pack("<16f", *ibm_root) + struct.pack("<16f", *ibm_bend)
    times = struct.pack("<3f", 0.0, 1.0, 2.0)
    rot = b"".join(struct.pack("<4f", *quat_z(a)) for a in (-0.7, 0.7, -0.7))

    pos_view = add(pos, 34962)
    nrm_view = add(nrm, 34962)
    jnt_view = add(jnt, 34962)
    wgt_view = add(wgt, 34962)
    idx_view = add(idx, 34963)
    ibm_view = add(ibm, None)
    time_view = add(times, None)
    rot_view = add(rot, None)

    xs = [p[0] for p in vertices["positions"]]
    ys = [p[1] for p in vertices["positions"]]
    zs = [p[2] for p in vertices["positions"]]
    accessors = [
        {"bufferView": pos_view, "componentType": 5126, "count": len(vertices["positions"]), "type": "VEC3",
         "min": [min(xs), min(ys), min(zs)], "max": [max(xs), max(ys), max(zs)]},
        {"bufferView": nrm_view, "componentType": 5126, "count": len(vertices["normals"]), "type": "VEC3"},
        {"bufferView": jnt_view, "componentType": 5123, "count": len(vertices["joints"]), "type": "VEC4"},
        {"bufferView": wgt_view, "componentType": 5126, "count": len(vertices["weights"]), "type": "VEC4"},
        {"bufferView": idx_view, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
        {"bufferView": ibm_view, "componentType": 5126, "count": 2, "type": "MAT4"},
        {"bufferView": time_view, "componentType": 5126, "count": 3, "type": "SCALAR", "min": [0.0], "max": [2.0]},
        {"bufferView": rot_view, "componentType": 5126, "count": 3, "type": "VEC4"},
    ]
    return b"".join(chunks), {"accessors": accessors, "bufferViews": views}


def main() -> None:
    vertices, indices = build_mesh()
    blob, packed = pack_bin(vertices, indices)
    bin_name = "bend_bar.bin"
    (OUT_DIR / bin_name).write_bytes(blob)

    gltf = {
        "asset": {"version": "2.0", "generator": "ToyRenderer dynamic smoke test"},
        "scene": 0,
        "scenes": [{"nodes": [0, 2]}],
        "nodes": [
            {"name": "Root", "translation": [0.0, -0.8, 0.0], "children": [1]},
            {"name": "Bend", "translation": [0.0, 0.8, 0.0]},
            {"name": "Mesh", "mesh": 0, "skin": 0},
        ],
        "meshes": [{
            "name": "BendBar",
            "primitives": [{
                "attributes": {"POSITION": 0, "NORMAL": 1, "JOINTS_0": 2, "WEIGHTS_0": 3},
                "indices": 4,
                "material": 0,
            }],
        }],
        "skins": [{
            "name": "BendSkin",
            "joints": [0, 1],
            "inverseBindMatrices": 5,
            "skeleton": 0,
        }],
        "animations": [{
            "name": "BendLoop",
            "samplers": [{"input": 6, "output": 7, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}],
        }],
        "materials": [{"name": "Default", "pbrMetallicRoughness": {"baseColorFactor": [0.75, 0.72, 0.68, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.8}}],
        "buffers": [{"uri": bin_name, "byteLength": len(blob)}],
        "bufferViews": packed["bufferViews"],
        "accessors": packed["accessors"],
    }
    (OUT_DIR / "bend_bar.gltf").write_text(json.dumps(gltf, indent=2), encoding="utf-8")
    print(f"Wrote {OUT_DIR / 'bend_bar.gltf'} verts={len(vertices['positions'])} tris={len(indices)//3}")


if __name__ == "__main__":
    main()
