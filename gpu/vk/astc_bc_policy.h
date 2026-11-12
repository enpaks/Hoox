// Which cards keep every large decoded ASTC texture as BC3, the finer blocks (UI, characters) too (vk_texture.cpp), and
// which of those textures can be BC1 instead (half of BC3).
// tests/test_astc_bc_policy.py
#pragma once
#include <cstdint>
#include <cstring>

namespace astc_bc_policy {

// The VRAM the driver allows the process, in bytes: under 8.5 GB, every large ASTC texture as BC3 (a quarter of RGBA8).
// Issue 519 set it under 6 GB (an RTX 3050 Laptop, 3.3 GB allowed: 1.75 GB of decoded ASTC in Goron City, paged, 4 fps).
// 8 GB cards are allowed ~7-7.6 GB and fill up the same way: an RX 7700S (7.1 GB allowed) held 2.5 GB of decoded ASTC in
// Hyrule Castle Town's market with the VRAM at the limit; as BC3, 1.3 GB. 10 GB cards and up keep the finer ones as RGBA8
inline bool all_by_vram(uint64_t allowed) {
    return allowed && allowed < (8704ull << 20);
}

// (hookshot) Whether ASTC blocks may hold an alpha other than one: what keeps a big decoded ASTC texture BC3 (16 bytes a
// block) rather than BC1 (8, opaque; RX_VK_ASTC_BC1). An RTX 3060 Ti (7.25 GB allowed) sat at 7.0-7.3 GB in Kokiri Forest
// with 2.5 GB of textures, and its turns there hitched when the VRAM went over. Each 16-byte block's colour endpoint mode
// says whether it carries alpha: LDR luminance+alpha (4, 5), RGB base+scale plus two alpha (10), RGBA (12-15). A
// void-extent block is one colour: opaque when LDR and its alpha (bits 112-127) is 0xFFFF. Counted as alpha, so kept BC3:
// a reserved block mode, several partitions whose endpoint classes differ (their modes lie partly below the weights, not
// decoded here), and storage mostly of all-zero blocks (not written yet). All-zero blocks alone are skipped: the padding
// of levels and layers
inline bool astc_may_have_alpha(const uint8_t* p, uint64_t bytes) {
    const uint64_t n = bytes / 16;
    uint64_t data = 0;
    for (uint64_t i = 0; i < n; i++, p += 16) {
        uint64_t lo, hi;
        std::memcpy(&lo, p, 8);
        std::memcpy(&hi, p + 8, 8);
        if (!lo && !hi) continue;
        data++;
        if ((lo & 0x1FF) == 0x1FC) {   // (void extent: one colour, RGBA 16 bits each from bit 64)
            if ((lo >> 9) & 1) return true;   // (HDR)
            if ((hi >> 48) != 0xFFFF) return true;
            continue;
        }
        if ((lo & 3) == 0 && ((lo & 0xC) == 0 || ((lo >> 6) & 7) == 7)) return true;   // (reserved block modes)
        uint32_t cem;
        if (((lo >> 11) & 3) == 0)
            cem = (uint32_t)(lo >> 13) & 0xF;        // (one partition: its mode in bits 13-16)
        else if (((lo >> 23) & 3) == 0)
            cem = (uint32_t)(lo >> 25) & 0xF;        // (partitions sharing one mode, in bits 25-28)
        else
            return true;
        if (cem == 4 || cem == 5 || cem == 10 || cem >= 12) return true;
    }
    return n == 0 || data * 2 < n;
}

} // namespace astc_bc_policy
