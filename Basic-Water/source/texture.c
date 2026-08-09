#include <string.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include "texture.h"
#include "rsx3d.h"

#include "textures_bin.h"

typedef struct {
    char       name[32];
    gcmTexture tex;
} TexEntry;

static TexEntry entries[TEXTURE_MAX];
static int      entry_count = 0;

static unsigned int be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

/* Kanal yeniden esleme: veri ARGB sirasinda; RSX'in bekledigi duzene
 * cevrilir. Bu deger yanlis olursa renkler yer degistirir (kirmizi-mavi
 * takasi en sik gorulen belirti). */
static u32 argb_remap(void)
{
    return (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
           (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
           (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
           (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
           (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
           (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
           (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
           (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT);
}

int texture_init(void)
{
    const unsigned char *p = (const unsigned char *)textures_bin;
    unsigned int size = textures_bin_size;
    unsigned int off;
    int count, i;

    entry_count = 0;

    if (size < 8 || p[0] != 'B' || p[1] != 'W' || p[2] != 'T' || p[3] != '1')
        return -1;

    count = (int)be32(p + 4);
    if (count <= 0 || count > TEXTURE_MAX)
        return -2;

    off = 8;
    for (i = 0; i < count; i++) {
        unsigned int w, h, mips, bytes, m, mw, mh;
        u8 *dst;
        u32 gpu_off;

        if (off + 44 > size)
            return -3;

        memcpy(entries[i].name, p + off, 31);
        entries[i].name[31] = '\0';
        off += 32;

        w    = be32(p + off);
        h    = be32(p + off + 4);
        mips = be32(p + off + 8);
        off += 12;

        /* mip zincirinin toplam boyutu */
        bytes = 0;
        mw = w;
        mh = h;
        for (m = 0; m < mips; m++) {
            bytes += mw * mh * 4;
            if (mw > 1) mw >>= 1;
            if (mh > 1) mh >>= 1;
        }

        if (off + bytes > size)
            return -4;

        /* RSX yalnizca kendi belleğindeki veriyi ornekleyebilir; gomulu
         * veri ana bellekte oldugu icin kopyalanmasi gerekiyor. */
        dst = (u8 *)rsxMemalign(128, bytes);
        if (dst == NULL)
            return -5;
        memcpy(dst, p + off, bytes);

        if (rsxAddressToOffset(dst, &gpu_off) != 0)
            return -6;

        memset(&entries[i].tex, 0, sizeof(gcmTexture));
        entries[i].tex.format    = GCM_TEXTURE_FORMAT_A8R8G8B8
                                   | GCM_TEXTURE_FORMAT_LIN;
        entries[i].tex.mipmap    = (u8)mips;
        entries[i].tex.dimension = GCM_TEXTURE_DIMS_2D;
        entries[i].tex.cubemap   = GCM_FALSE;
        entries[i].tex.remap     = argb_remap();
        entries[i].tex.width     = (u16)w;
        entries[i].tex.height    = (u16)h;
        entries[i].tex.depth     = 1;
        entries[i].tex.location  = GCM_LOCATION_RSX;
        entries[i].tex.pitch     = w * 4;
        entries[i].tex.offset    = gpu_off;

        off += bytes;
        off = (off + 3u) & ~3u;     /* uretici parca sonunu hizaliyor */

        entry_count++;
    }

    return 0;
}

int texture_find(const char *name)
{
    int i;

    for (i = 0; i < entry_count; i++)
        if (strcmp(entries[i].name, name) == 0)
            return i;
    return -1;
}

void texture_bind(int unit, int index)
{
    gcmContextData *ctx = rsx3d_context();

    if (index < 0 || index >= entry_count)
        return;

    rsxInvalidateTextureCache(ctx, GCM_INVALIDATE_TEXTURE);
    rsxLoadTexture(ctx, (u8)unit, &entries[index].tex);

    /* Mip'ler arasi da suzulur (trilinear): pist yuzeyi uzakta titremesin. */
    rsxTextureControl(ctx, (u8)unit, GCM_TRUE, 0,
                      (u16)(entries[index].tex.mipmap << 8),
                      GCM_TEXTURE_MAX_ANISO_4);
    rsxTextureFilter(ctx, (u8)unit, 0,
                     GCM_TEXTURE_LINEAR_MIPMAP_LINEAR,
                     GCM_TEXTURE_LINEAR,
                     GCM_TEXTURE_CONVOLUTION_QUINCUNX);
    rsxTextureWrapMode(ctx, (u8)unit,
                       GCM_TEXTURE_REPEAT, GCM_TEXTURE_REPEAT,
                       GCM_TEXTURE_REPEAT, 0, GCM_TEXTURE_ZFUNC_NEVER, 0);
}

int texture_count(void)
{
    return entry_count;
}
