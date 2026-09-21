# Dynamic SDF test assets

`bend_bar.gltf` is a watertight 2-bone cylinder used to smoke-test per-frame CPU skinning + SDF update.

It is **not** a paper experiment case. Replace `Config.json5` `modelPath` with a thick, watertight character (10k–100k tris, skins + animations, no hair/cloth) for the actual dynamic-geometry experiment.

Regenerate the smoke mesh:

```
python Renderer/Asset/Models/dynamic/generate_bend_bar.py
```
