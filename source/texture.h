#ifndef TEXTURE_H
#define TEXTURE_H

/* RSX texture support.
 *
 * Textures are converted to the "BWT1" format by tools/make_textures.py
 * and embedded in the EBOOT: power-of-two sizes, big-endian ARGB, full mip
 * chain.
 *
 * At start-up the module copies every texture into RSX memory and sets up
 * a gcmTexture for each; drawing only calls texture_bind. */

#define TEXTURE_MAX 8

/* Parses the embedded texture pack and uploads it to RSX. 0: success */
int texture_init(void);

/* Texture index by name; -1 if not found */
int texture_find(const char *name);

/* Binds a texture to a texture unit (sampler order in the fragment shader).
 * tile: how many times the texture repeats (the UV multiplier is passed to
 * the shader separately). */
void texture_bind(int unit, int index);

/* Number of loaded textures (diagnostics) */
int texture_count(void);

#endif
