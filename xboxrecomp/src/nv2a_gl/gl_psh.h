/* gl_psh.h -- NV2A register combiners as GLSL (see gl_psh.c). */
#ifndef NV2A_GL_PSH_H
#define NV2A_GL_PSH_H

#include <stddef.h>
#include <stdint.h>

/* Everything that changes the generated code, and nothing that does not
 * (factors, fog colour and the alpha reference are uniforms). Compared with
 * memcmp, so fill every field. */
typedef struct {
    uint32_t color_icw[8], color_ocw[8];
    uint32_t alpha_icw[8], alpha_ocw[8];
    uint32_t control;          /* NV097_SET_COMBINER_CONTROL */
    uint32_t final0, final1;   /* SPECULAR_FOG_CW0 / CW1 */
    uint32_t shader_program;   /* NV097_SET_SHADER_STAGE_PROGRAM */
    uint32_t other_input;      /* NV097_SET_SHADER_OTHER_STAGE_INPUT */
} Nv2aPshKey;

/* Nonzero: emit Vulkan GLSL (nv2a_vk) instead of GLSL 3.30 (gl_vsh.c). */
extern int nv2a_shader_vk;

/* The fog unit's uniform u_fogp for a batch: SET_FOG_PARAMS 0 and 1, the
 * mode (0 linear, 1 exp, 2 exp2, +3 for the _ABS forms) and how vFog is
 * read (0 raw, 1 fog disabled: factor 1, 2 through the mode). xform is the
 * batch's transform path (2 = vertex program). */
void nv2a_fog_params(const uint32_t *regs, uint32_t xform, float out[4]);

/* Fragment shader source for `k`. Returns bytes written, -1 on overflow. */
int nv2a_gl_psh(const Nv2aPshKey *k, char *buf, size_t cap);

#endif
