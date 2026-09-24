#!/usr/bin/env python3
"""从 Thingi10K 筛出水密子集，写成归一化 glTF 和批量测试清单。"""

import argparse
import json
import math
import random
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[4]
ASSET_ROOT = REPO_ROOT / "Renderer" / "Asset"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Prepare a watertight Thingi10K subset for the bench.")
    parser.add_argument("--cache", type=Path, default=REPO_ROOT / ".thingi10k")
    parser.add_argument("--out-dir", type=Path, default=ASSET_ROOT / "Models" / "thingi10k")
    parser.add_argument("--list", type=Path, default=Path(__file__).resolve().parent / "thingi10k_models.txt")
    parser.add_argument("--manifest", type=Path, default=Path(__file__).resolve().parent / "thingi10k_manifest.csv")
    parser.add_argument("--summary", type=Path, default=Path(__file__).resolve().parent / "thingi10k_summary.json")
    parser.add_argument("--count", type=int, default=200)
    parser.add_argument("--bins", type=int, default=10)
    parser.add_argument("--min-facets", type=int, default=100)
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()
    if args.count < 1 or args.bins < 1 or args.min_facets < 1:
        parser.error("--count, --bins, and --min-facets must be >= 1")
    if args.bins > args.count:
        parser.error("--bins cannot exceed --count")
    return args


def log_edges(min_facets: int, max_facets: int, bins: int) -> list[float]:
    if max_facets <= min_facets or bins == 1:
        return [float(min_facets), float(max_facets) + 1.0]
    log_min = math.log(min_facets)
    log_max = math.log(max_facets)
    edges = [math.exp(log_min + (log_max - log_min) * i / bins) for i in range(bins + 1)]
    edges[0] = float(min_facets)
    edges[-1] = float(max_facets) + 1.0
    return edges


def bin_index(num_facets: int, edges: list[float]) -> int:
    for index in range(len(edges) - 1):
        if edges[index] <= num_facets < edges[index + 1]:
            return index
    return len(edges) - 2


def quotas(count: int, bins: int) -> list[int]:
    base, extra = divmod(count, bins)
    return [base + (1 if index < extra else 0) for index in range(bins)]


def normalize_vertices(vertices: np.ndarray) -> np.ndarray | None:
    lo = vertices.min(axis=0)
    hi = vertices.max(axis=0)
    extent = float((hi - lo).max())
    if extent <= 0.0:
        return None
    center = (lo + hi) * 0.5
    return ((vertices - center) * (2.0 / extent)).astype(np.float32, copy=False)


def write_gltf(path: Path, vertices: np.ndarray, facets: np.ndarray) -> None:
    positions = np.ascontiguousarray(vertices, dtype=np.float32)
    indices = np.ascontiguousarray(facets.reshape(-1), dtype=np.uint32)
    position_bytes = positions.tobytes()
    index_bytes = indices.tobytes()
    bin_path = path.with_suffix(".bin")
    bin_path.write_bytes(position_bytes + index_bytes)

    vmin = positions.min(axis=0)
    vmax = positions.max(axis=0)
    document = {
        "asset": {"version": "2.0", "generator": "prepare_dataset.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": path.stem}],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {"POSITION": 0},
                        "indices": 1,
                        "mode": 4,
                    }
                ]
            }
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": int(positions.shape[0]),
                "type": "VEC3",
                "min": [float(vmin[0]), float(vmin[1]), float(vmin[2])],
                "max": [float(vmax[0]), float(vmax[1]), float(vmax[2])],
            },
            {
                "bufferView": 1,
                "componentType": 5125,
                "count": int(indices.size),
                "type": "SCALAR",
            },
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(position_bytes), "target": 34962},
            {
                "buffer": 0,
                "byteOffset": len(position_bytes),
                "byteLength": len(index_bytes),
                "target": 34963,
            },
        ],
        "buffers": [{"uri": bin_path.name, "byteLength": len(position_bytes) + len(index_bytes)}],
    }
    path.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")


def load_mesh(file_path: str):
    import thingi10k

    vertices, facets = thingi10k.load_file(file_path)
    vertices = np.asarray(vertices, dtype=np.float64)
    facets = np.asarray(facets, dtype=np.int64)
    if vertices.ndim != 2 or vertices.shape[1] != 3 or vertices.shape[0] == 0:
        raise ValueError("vertices must be N x 3")
    if facets.ndim != 2 or facets.shape[1] != 3 or facets.shape[0] == 0:
        raise ValueError("facets must be M x 3")
    if int(facets.min()) < 0 or int(facets.max()) >= vertices.shape[0]:
        raise ValueError("facet index out of range")
    return vertices, facets


def main() -> int:
    args = parse_args()
    try:
        import thingi10k
    except ImportError:
        print("thingi10k is not installed. Run: python -m pip install thingi10k", file=sys.stderr)
        return 1

    args.cache.mkdir(parents=True, exist_ok=True)
    print(f"thingi10k cache: {args.cache}", flush=True)
    thingi10k.init(variant="npz", cache_dir=str(args.cache))

    total = len(thingi10k.dataset())
    filtered = thingi10k.dataset(
        solid=True,
        num_components=1,
        closed=True,
        manifold=True,
        num_facets=(args.min_facets, None),
    )
    rows = [
        {
            "file_id": int(entry["file_id"]),
            "file_path": entry["file_path"],
            "num_facets": int(entry["num_facets"]),
            "license": entry["license"] or "",
            "name": entry["name"] or "",
        }
        for entry in filtered
    ]
    if not rows:
        print("no models passed the filter", file=sys.stderr)
        return 1

    max_facets = max(row["num_facets"] for row in rows)
    edges = log_edges(args.min_facets, max_facets, args.bins)
    groups: list[list[dict]] = [[] for _ in range(len(edges) - 1)]
    for row in rows:
        groups[bin_index(row["num_facets"], edges)].append(row)

    rng = random.Random(args.seed)
    for group in groups:
        rng.shuffle(group)
    target = quotas(args.count, len(groups))

    args.out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    failures = []
    bin_stats = []
    for index, group in enumerate(groups):
        taken = 0
        for row in group:
            if taken >= target[index]:
                break
            relative = f"Models/thingi10k/{row['file_id']}.gltf"
            destination = args.out_dir / f"{row['file_id']}.gltf"
            try:
                vertices, facets = load_mesh(row["file_path"])
                normalized = normalize_vertices(vertices)
                if normalized is None:
                    raise ValueError("zero bounding-box extent")
                write_gltf(destination, normalized, facets)
            except (OSError, ValueError) as error:
                failures.append({"file_id": row["file_id"], "error": str(error)})
                print(f"skip {row['file_id']}: {error}", flush=True)
                continue
            written.append(
                {
                    "file_id": row["file_id"],
                    "triangles": int(facets.shape[0]),
                    "license": row["license"],
                    "name": row["name"],
                    "bin": index,
                    "relative_path": relative,
                }
            )
            taken += 1
            print(
                f"wrote {len(written)} bin {index + 1}/{len(groups)} {relative} triangles={facets.shape[0]}",
                flush=True,
            )
        bin_stats.append(
            {
                "bin": index,
                "min_facets": edges[index],
                "max_facets": edges[index + 1],
                "available": len(group),
                "target": target[index],
                "written": taken,
            }
        )

    written.sort(key=lambda item: (item["triangles"], item["file_id"]))
    args.list.parent.mkdir(parents=True, exist_ok=True)
    list_lines = [
        f"# thingi10k solid=True num_components=1 closed=True manifold=True num_facets>={args.min_facets}",
        f"# dataset={total} filtered={len(rows)} selected={len(written)} bins={len(groups)} seed={args.seed}",
    ]
    list_lines.extend(item["relative_path"] for item in written)
    args.list.write_text("\n".join(list_lines) + "\n", encoding="utf-8")

    manifest_lines = ["file_id,triangles,bin,license,relative_path"]
    for item in written:
        license_text = item["license"].replace('"', "'")
        manifest_lines.append(
            f'{item["file_id"]},{item["triangles"]},{item["bin"]},"{license_text}",{item["relative_path"]}'
        )
    args.manifest.write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")

    summary = {
        "dataset_total": total,
        "filtered": len(rows),
        "selected": len(written),
        "min_facets": args.min_facets,
        "max_facets": max_facets,
        "bins": bin_stats,
        "failures": failures,
        "seed": args.seed,
        "filters": {
            "solid": True,
            "num_components": 1,
            "closed": True,
            "manifold": True,
        },
    }
    args.summary.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(
        f"filtered {len(rows)}/{total}, wrote {len(written)} models\n"
        f"list: {args.list}\nmanifest: {args.manifest}",
        flush=True,
    )
    return 0 if written else 1


if __name__ == "__main__":
    sys.exit(main())
