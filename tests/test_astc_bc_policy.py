"""Compile and run the rules for the cards that keep every large ASTC texture as BC3, and for which of those textures are BC1
(gpu/vk/astc_bc_policy.h; no game/GPU)."""
import argparse
import subprocess
import tempfile
from pathlib import Path

RUN_ALL_ARGS = ["--cxx", "g++"]

ROOT = Path(__file__).resolve().parents[1]


def check_source():
    """The texture setup still asks the rules, the switches that override them are still read, a BC1 image is encoded,
    viewed and given raw views as BC1, and a copy between BC1 and BC3 images goes through the encoder"""
    src = (ROOT / "gpu/vk/vk_texture.cpp").read_text(encoding="utf-8")
    for needle in ('#include "astc_bc_policy.h"', "astc_bc_policy::all_by_vram(allowed)", 'getenv("RX_VK_ASTC_BC_ALL")',
                   "(bw * bh >= 36 || all) ? low : 64ull << 20",
                   "astc_bc_policy::astc_may_have_alpha(p, x.guest_size)", 'getenv("RX_VK_ASTC_BC1")',
                   "dst.format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ? 0u : 1u",
                   "image == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ? (srgb ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK",
                   "x.bc ? x.format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK : bpb == 8"):
        if needle not in src:
            raise SystemExit("vk_texture.cpp no longer has: " + needle)
    xfer = (ROOT / "gpu/vk/vk_xfer.cpp").read_text(encoding="utf-8")
    if "(s.tex->bc && s.tex->format != d.tex->format)" not in xfer:
        raise SystemExit("vk_xfer.cpp no longer sends a copy between BC1 and BC3 images through the encoder")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    check_source()
    with tempfile.TemporaryDirectory(prefix="astc-bc-policy-") as tmp:
        exe = Path(tmp) / "astc-bc-policy"
        subprocess.run([args.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "tests/test_astc_bc_policy.cpp"), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
