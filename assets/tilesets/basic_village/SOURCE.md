# Basic Village Tileset

- Creator: Forchild
- Source: https://forchild.itch.io/village-tileset
- License: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)
- Imported: 2026-07-16
- Base tile unit: 16x16 pixels

The creator's source page publishes this pack under CC0 and says attribution is
optional. The downloaded archive did not include a license or metadata file, so
this file preserves that provenance locally. Midnight retains the credit even
though CC0 does not require it.

## Import record

Source archive: `BasicVillageTileset.zip`

Archive SHA-256:
`b4297a432566699e7a2858b067b4050c8631d8a96aa3cca99d29e97cae782b9a`

The PNG contents are byte-for-byte identical to the archive. Only their names
were normalized to lowercase snake case for stable runtime paths.

| Repository file | Upstream file | Dimensions | SHA-256 |
|---|---|---:|---|
| `furniture.png` | `Furniture.png` | 192x128 | `757cd54dc0bbfdb7efa13590f3785b5532797d6c5079ee6b3473921254a6631e` |
| `house_tileset.png` | `House_tileset.png` | 192x160 | `89b48d140121ddec253b50d7e36c7bcae0c5b8e1168ae47b7cfeb5439b584085` |
| `outdoor_tileset.png` | `Outdoor_tileset.png` | 192x128 | `967806f572267b87787d05414e98350c9cb19f5eab426db6d4889d99b123f89c` |
| `trees_and_bushes.png` | `Trees_and_bushes.png` | 144x96 | `55be641f0a0f8461c9bdd5f1a1fc2fef607428194df152197335eabc96dd9b5a` |

All four files are non-interlaced, 8-bit RGBA PNG images. Some objects span
multiple 16x16 cells, so the grid alone does not define object or collision
semantics. Preserve partial alpha when decoding; the trees sheet contains
translucent shadow pixels.
