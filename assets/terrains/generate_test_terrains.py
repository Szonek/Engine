#!/usr/bin/env python3
"""Generates the heightmaps and terrain files in this folder. Standard library only; the output is deterministic
(integer maths), so running it again changes nothing. Edit this script and run it instead of editing the files:

    python3 assets/terrains/generate_test_terrains.py

  test_fixtures/hills.png   65 x 65, 16-bit grayscale: what the engine's heightmap import is tested with. Pixel (x, y)
                            is 1000 * x + 10 * y, so a test knows every height.
  outdoor_test.png          65 x 65, 16-bit grayscale: rolling hills around a flat clearing, the source of
  outdoor_test.terrain      the terrain of assets/scenes/outdoor_test.json (1 m cells, heights 0 .. 6 m), written as
                            the engine's import would (engineTerrainImportHeightmap, then engineTerrainSave). Small on
                            purpose: the nav mesh is built in one piece when the level loads.
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def write_png16(path, width, height, pixels):
    """A 16-bit grayscale PNG; pixels are row by row, 0..65535."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    rows = b"".join(b"\x00" + struct.pack(">%dH" % width, *pixels[y * width:(y + 1) * width]) for y in range(height))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 16, 0, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(rows, 9)))
        f.write(chunk(b"IEND", b""))


def write_terrain(path, width, height, cell_size, pixels, height_min, height_max):
    """The engine's terrain file (TerrainData::serialize): little endian, heights as floats."""
    scale = (height_max - height_min) / 65535.0
    with open(path, "wb") as f:
        f.write(b"ENGTERR\x00")
        f.write(struct.pack("<IIIf", 1, width, height, cell_size))
        f.write(struct.pack("<%df" % len(pixels), *(height_min + p * scale for p in pixels)))


def hills(size):
    """Smooth bumps (each one (1 - d^2 / r^2)^2 high, so they fade out without an edge) around a flat clearing in the
    middle. Integers only."""
    bumps = [  # center x, center z, radius, height (of 65535)
        (11, 13, 15, 42000), (49, 10, 17, 52000), (52, 46, 15, 36000), (15, 50, 18, 60000),
        (32, 7, 9, 20000), (58, 29, 8, 24000), (5, 31, 9, 30000), (33, 59, 10, 26000),
    ]
    middle = size // 2
    clearing = 8
    pixels = []
    for z in range(size):
        for x in range(size):
            value = 0
            for cx, cz, radius, top in bumps:
                d2 = (x - cx) ** 2 + (z - cz) ** 2
                r2 = radius * radius
                if d2 < r2:
                    value += top * (r2 - d2) ** 2 // (r2 * r2)
            # flat in the clearing, rising to the hills over the next cells
            distance2 = (x - middle) ** 2 + (z - middle) ** 2
            inner = clearing * clearing
            outer = (clearing + 8) * (clearing + 8)
            if distance2 <= inner:
                value = 0
            elif distance2 < outer:
                value = value * (distance2 - inner) // (outer - inner)
            pixels.append(min(value, 65535))
    return pixels


def main():
    os.makedirs(os.path.join(HERE, "test_fixtures"), exist_ok=True)
    write_png16(os.path.join(HERE, "test_fixtures", "hills.png"), 65, 65, [1000 * x + 10 * y for y in range(65) for x in range(65)])

    size = 65
    pixels = hills(size)
    write_png16(os.path.join(HERE, "outdoor_test.png"), size, size, pixels)
    write_terrain(os.path.join(HERE, "outdoor_test.terrain"), size, size, 1.0, pixels, 0.0, 6.0)


if __name__ == "__main__":
    main()
