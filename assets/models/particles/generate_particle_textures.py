#!/usr/bin/env python3
"""Generates the particle textures in this folder. Run it from anywhere:

    python generate_particle_textures.py

The output is deterministic (running it again changes no file) and uses only the standard library: the noise is
value noise with its own integer hash, never Python's random module. The files are committed; edit this script and
regenerate them rather than editing them by hand. Images are named by their path under assets/models, like every image
file the engine loads (e.g. "particles/smoke_puff.png"). The textures:

  smoke_puff.png   a flipbook of 4 x 4 frames of 64 x 64 smoke puffs, each a little different (the noise moves from
                   frame to frame), for a particle effect with texture_channels "packed":
                     R  brightness: billowy detail the particle's colour is multiplied by
                     G  erosion noise: the erosion threshold dissolves the puff where it is below it, the edge first
                     B  unused (0)
                     A  alpha: a round puff with a ragged, noisy edge
"""
import math
import os
import struct
import zlib

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

FRAME_SIZE = 64
COLUMNS = 4
ROWS = 4


def hash2(x, y, seed):
    """A deterministic value in [0, 1) for an integer lattice point."""
    h = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF) / float(0x1000000)


def smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y, seed):
    """Smoothly interpolated lattice noise in [0, 1)."""
    x0 = math.floor(x)
    y0 = math.floor(y)
    fx = smooth(x - x0)
    fy = smooth(y - y0)
    a = hash2(x0, y0, seed)
    b = hash2(x0 + 1, y0, seed)
    c = hash2(x0, y0 + 1, seed)
    d = hash2(x0 + 1, y0 + 1, seed)
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy


def fbm(x, y, seed, octaves=4):
    """Fractal noise in about [0, 1): octaves of value noise, each twice as fine and half as strong."""
    total = 0.0
    amplitude = 0.5
    norm = 0.0
    for octave in range(octaves):
        total += amplitude * value_noise(x, y, seed + octave * 101)
        norm += amplitude
        x *= 2.0
        y *= 2.0
        amplitude *= 0.5
    return total / norm


def clamp01(v):
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


def to_byte(v):
    return int(round(clamp01(v) * 255.0))


def puff_frame(frame):
    """The RGBA pixels of one frame, rows from the top, as a list of rows of (r, g, b, a)."""
    # the noise drifts a little from frame to frame, so the puff boils
    drift = frame * 0.35
    rows = []
    for py in range(FRAME_SIZE):
        row = []
        for px in range(FRAME_SIZE):
            # -1..1 across the frame
            u = (px + 0.5) / FRAME_SIZE * 2.0 - 1.0
            v = (py + 0.5) / FRAME_SIZE * 2.0 - 1.0
            distance = math.sqrt(u * u + v * v)
            # a billowy edge: the radius varies with the noise around the puff
            edge_noise = fbm(u * 2.5 + drift, v * 2.5 - drift * 0.5, 11)
            radius = 0.72 + 0.25 * (edge_noise - 0.5) * 2.0
            body = clamp01(1.0 - distance / radius)
            alpha = smooth(clamp01(body * 1.8)) * (0.75 + 0.25 * fbm(u * 4.0 - drift, v * 4.0, 23))
            # billowy brightness detail, a little darker toward the edge
            detail = fbm(u * 3.0 + 7.0 + drift * 0.6, v * 3.0 + drift, 37)
            brightness = 0.7 + 0.3 * detail - 0.12 * clamp01(distance)
            # erosion: low at the edge and in the noise's valleys, so the puff breaks into wisps from its edge
            erosion = 0.55 * fbm(u * 5.0 + drift, v * 5.0 + drift * 0.3, 53, 5) + 0.45 * body
            row.append((to_byte(brightness), to_byte(erosion), 0, to_byte(alpha)))
        rows.append(row)
    return rows


def write_png(path, width, height, rows):
    """An 8-bit RGBA PNG; rows from the top, each a list of (r, g, b, a)."""
    raw = bytearray()
    for row in rows:
        raw.append(0)  # no filter
        for pixel in row:
            raw.extend(pixel)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def smoke_puff():
    """The 4 x 4 flipbook: frame 0 at the top left, then left to right and top to bottom."""
    frames = [puff_frame(frame) for frame in range(COLUMNS * ROWS)]
    rows = []
    for frame_row in range(ROWS):
        for py in range(FRAME_SIZE):
            row = []
            for frame_column in range(COLUMNS):
                row.extend(frames[frame_row * COLUMNS + frame_column][py])
            rows.append(row)
    write_png(os.path.join(OUT_DIR, "smoke_puff.png"), FRAME_SIZE * COLUMNS, FRAME_SIZE * ROWS, rows)


if __name__ == "__main__":
    smoke_puff()
