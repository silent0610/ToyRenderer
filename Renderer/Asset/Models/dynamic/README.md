# Dynamic SDF demo assets

## Showcase demos (paper / video)

Use these two **simple skinned** characters for dynamic-geometry demos:

| Asset | Path |
|-------|------|
| Sugar teddy bear | `Models/dynamic/sugar_teddy_bear/scene.gltf` |
| Cute home robot | `Models/dynamic/cute_home_robot/scene.gltf` |

**Scope claim (keep narrow):** the pipeline can demonstrate **per-frame SDF updates on some simple dynamic models** (watertight / thick-ish, few bones, no hair/cloth). Do **not** claim general dynamic characters, topology-changing deformation, or thin shells.

Suggested `Config.json5`:

```json5
"modelPath": [ "Models/dynamic/sugar_teddy_bear/scene.gltf" ],
"DynamicGeometry": { "enable": true, "animationIndex": 0, "speed": 1.0, "loop": true }
```

Swap `modelPath` to `cute_home_robot` for the second clip. Prefer hierarchical camera selection with **Stable Camera Selection** on for less flicker; turn it off to compare cost.

## Smoke test only

`bend_bar.gltf` is a watertight 2-bone cylinder for CPU skinning + SDF smoke tests. It is **not** a paper showcase case.

```
python Renderer/Asset/Models/dynamic/generate_bend_bar.py
```
