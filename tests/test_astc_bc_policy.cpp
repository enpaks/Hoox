// astc_bc_policy (gpu/vk/astc_bc_policy.h): the cards that keep every large ASTC texture as BC3 (all_by_vram), and which of
// those textures may be BC1 (astc_may_have_alpha, from the blocks' colour endpoint modes).
// tests/test_astc_bc_policy.py
#include "../gpu/vk/astc_bc_policy.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int failures = 0;
static void expect(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        failures++;
    }
}

// One ASTC block: bits 0-63, 64-127
struct Block {
    uint64_t lo, hi;
};
static bool may_have_alpha(const std::vector<Block>& blocks) {
    std::vector<uint8_t> b(blocks.size() * 16);
    for (size_t i = 0; i < blocks.size(); i++) {
        std::memcpy(b.data() + i * 16, &blocks[i].lo, 8);
        std::memcpy(b.data() + i * 16 + 8, &blocks[i].hi, 8);
    }
    return astc_bc_policy::astc_may_have_alpha(b.data(), b.size());
}
// A block of one partition with this colour endpoint mode (block mode 1: bits 0-1 not zero, not reserved)
static Block one(uint32_t cem) {
    return {1ull | (uint64_t)cem << 13, 0x0123456789abcdefull};
}
// Two partitions sharing this mode (the selector, bits 23-24, zero; the mode in bits 25-28)
static Block shared(uint32_t cem) {
    return {1ull | 1ull << 11 | (uint64_t)cem << 25, 0x0123456789abcdefull};
}
// A void-extent block (one colour) of this alpha; hdr: the HDR flag (bit 9)
static Block void_extent(uint16_t alpha, bool hdr = false) {
    return {0x1FCull | (uint64_t)hdr << 9 | 3ull << 10, (uint64_t)alpha << 48 | 0x0000FFFF8000ull};
}

int main() {
    const uint64_t MB = 1ull << 20;
    using astc_bc_policy::all_by_vram;
    expect(!all_by_vram(0), "unknown: as before (the finer ones as RGBA8)");
    expect(all_by_vram(3379 * MB), "a 4 GB laptop card (3.3 GB allowed, issue 519)");
    expect(all_by_vram(5900 * MB), "a 6 GB card");
    expect(all_by_vram(7133 * MB), "an 8 GB RX 7700S (7.1 GB allowed)");
    expect(all_by_vram(7700 * MB), "an 8 GB card allowed more");
    expect(!all_by_vram(9400 * MB), "a 10 GB card");
    expect(!all_by_vram(11200 * MB), "a 12 GB card");
    expect(!all_by_vram(15000 * MB), "a 16 GB card");

    // the opaque endpoint modes: luminance (0-3), RGB (6-9, 11)
    for (uint32_t cem : {0u, 1u, 2u, 3u, 6u, 7u, 8u, 9u, 11u})
        expect(!may_have_alpha({one(cem), one(cem), one(cem), one(cem)}), "an opaque endpoint mode: BC1");
    // the ones with alpha: luminance+alpha (4, 5), RGB scale plus alpha (10), RGBA (12-15)
    for (uint32_t cem : {4u, 5u, 10u, 12u, 13u, 14u, 15u})
        expect(may_have_alpha({one(cem), one(cem)}), "an endpoint mode with alpha: BC3");
    expect(may_have_alpha({one(8), one(8), one(8), one(12)}), "one alpha block among opaque ones: BC3");
    expect(!may_have_alpha({shared(9), shared(8)}), "partitions sharing an opaque mode: BC1");
    expect(may_have_alpha({shared(13)}), "partitions sharing an RGBA mode: BC3");
    expect(may_have_alpha({{1ull | 1ull << 11 | 1ull << 23, 0}}), "partitions of different classes (not decoded): BC3");
    expect(!may_have_alpha({void_extent(0xFFFF), one(8)}), "an opaque void-extent block: BC1");
    expect(may_have_alpha({void_extent(0x8000), one(8)}), "a half-transparent void-extent block: BC3");
    expect(may_have_alpha({void_extent(0xFFFF, true), one(8)}), "an HDR void-extent block: BC3");
    expect(may_have_alpha({{0x10, 0}, one(8)}), "a reserved block mode: BC3");
    expect(may_have_alpha({{0x1C4, 0}, one(8)}), "a reserved block mode (bits 6-8 set, bits 0-1 clear): BC3");
    expect(!may_have_alpha({one(8), one(8), one(8), {0, 0}}), "a little all-zero padding: skipped, BC1");
    expect(may_have_alpha({one(8), {0, 0}, {0, 0}, {0, 0}}), "mostly all-zero (not written yet): BC3");
    expect(may_have_alpha({}), "no blocks: BC3");
    if (failures) return EXIT_FAILURE;
    std::printf("astc bc policy: all checks passed\n");
    return EXIT_SUCCESS;
}
