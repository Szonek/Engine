"""Generates the particle textures in this folder (run it from anywhere: python generate_particle_textures.py).

Standard library only and deterministic: the same script gives the same bytes on every machine (its own integer
hash instead of a random generator, IEEE double arithmetic, PNGs stored uncompressed so the bytes don't depend on the
zlib version). Edit this script and run it again instead of editing the images.

  smoke_puff.png   a puff of smoke as a flipbook of 4 x 4 frames of 64 x 64 pixels, left to right, top to bottom:
                   the billows turn slowly through the 16 frames and the last one leads back into the first (an effect
                   may loop it at flipbook_fps or play it once over a particle's life). White, with the density in
                   alpha: thick in the middle, thin and ragged at the edge, so erosion eats it from the edge inwards
                   (the particle's colour and the scene's light shade it).
"""
import math
import os
import struct
import zlib

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

FRAME = 64
COLUMNS = 4
ROWS = 4
FRAMES = COLUMNS * ROWS


def png(width, height, pixel):
    """An RGBA PNG of pixel(x, y) -> (r, g, b, a) bytes. Stored, not compressed, so the bytes don't depend on zlib."""
    def chunk(kind, payload):
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(height):
        raw.append(0)  # no filter
        for x in range(width):
            raw.extend(pixel(x, y))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 0)) + chunk(b"IEND", b"")


def hash01(x, y, z):
    """A value in [0, 1) for a lattice point: an integer hash (the same everywhere)."""
    h = (x * 374761393 + y * 668265263 + z * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return h / 4294967296.0


def smooth(t):
    # quintic: no creases along the lattice lines
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)


def value_noise(x, y, z, period_z):
    """Value noise in [0, 1), periodic in z with period_z lattice cells (so the frames loop)."""
    xi, yi, zi = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = smooth(x - xi), smooth(y - yi), smooth(z - zi)

    def at(dx, dy, dz):
        return hash01(xi + dx, yi + dy, (zi + dz) % period_z)

    def lerp(a, b, t):
        return a + (b - a) * t

    x00 = lerp(at(0, 0, 0), at(1, 0, 0), fx)
    x10 = lerp(at(0, 1, 0), at(1, 1, 0), fx)
    x01 = lerp(at(0, 0, 1), at(1, 0, 1), fx)
    x11 = lerp(at(0, 1, 1), at(1, 1, 1), fx)
    return lerp(lerp(x00, x10, fy), lerp(x01, x11, fy), fz)


def fbm(x, y, z, octaves, period_z):
    """Fractal noise in about [0, 1): octaves of value noise, each twice as fine and half as strong."""
    total = 0.0
    amplitude = 0.5
    weight = 0.0
    for octave in range(octaves):
        scale = 2 ** octave
        # every octave keeps the same period in frames: its z is scaled with it
        total += amplitude * value_noise(x * scale + octave * 17.0, y * scale + octave * 31.0, z * scale, period_z * scale)
        weight += amplitude
        amplitude *= 0.5
    return total / weight


def puff_pixel(frame, x, y):
    """The puff of one frame at pixel (x, y) of it: (luminance, density), both 0..1."""
    # -1..1 across the frame, pixel centres
    u = (x + 0.5) / FRAME * 2.0 - 1.0
    v = (y + 0.5) / FRAME * 2.0 - 1.0
    r = math.sqrt(u * u + v * v)
    # z runs once around 4 lattice cells over the 16 frames
    z = frame / FRAMES * 4.0
    # lumps: the outline bulges out and in, so it is a cauliflower and not a blurred circle
    lumps = fbm(u * 1.3 + 3.0, v * 1.3 + 9.0, z, 2, 4)
    edge = r / (0.62 + 0.45 * lumps)
    # the billows inside: noise warped by noise, so they curl
    warp_u = fbm(u * 1.5 + 3.0, v * 1.5, z, 2, 4) - 0.5
    warp_v = fbm(u * 1.5, v * 1.5 + 7.0, z, 2, 4) - 0.5
    billows = fbm(u * 2.2 + warp_u * 0.9 + 11.0, v * 2.2 + warp_v * 0.9 + 5.0, z, 3, 4)
    # thick in the middle, nothing at the frame's edge (so neighbouring frames never bleed in)
    body = max(0.0, min(1.0, (1.0 - edge) * 1.5)) ** 1.5
    border = max(0.0, 1.0 - r * 1.08)
    density = body * (0.2 + 1.1 * billows) * min(1.0, border * 6.0)
    density = max(0.0, min(1.0, density))
    # a little light and shade inside it: the billows' ridges are brighter
    luminance = 0.72 + 0.28 * max(0.0, min(1.0, (billows - 0.25) * 2.0))
    return luminance, density


def smoke_puff():
    def pixel(x, y):
        frame = (y // FRAME) * COLUMNS + x // FRAME
        luminance, density = puff_pixel(frame, x % FRAME, y % FRAME)
        l = int(round(luminance * 255.0))
        return (l, l, l, int(round(density * 255.0)))

    with open(os.path.join(OUT_DIR, "smoke_puff.png"), "wb") as f:
        f.write(png(FRAME * COLUMNS, FRAME * ROWS, pixel))


if __name__ == "__main__":
    smoke_puff()
