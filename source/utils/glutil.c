/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/glutil.h"

#include "utils/utils.h"
#include "utils/dialog.h"
#include "utils/logger.h"

#include <stdio.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/io/stat.h>

// Helpers for our handling of shaders
GLboolean skip_next_compile = GL_FALSE;
char next_shader_fname[256];
void load_shader(GLuint shader, const char * string, size_t length);

static volatile int pointer_visible;
static volatile int pointer_screen_x = 480;
static volatile int pointer_screen_y = 272;

#define BG2V_GL_COMPRESSED_RGB8_ETC2 0x9274
#define BG2V_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276

/*
 * vitaGL already links detex's ETC2 block decoder for ETC2/EAC textures, but
 * its public compressed-texture switch does not accept the RGB or one-bit
 * alpha ETC2 enums used by BG2's area pages. Reuse the linked decoder here so
 * the port stays reproducible without carrying a modified vitaGL submodule.
 */
extern bool detexDecompressBlockETC2(
    const uint8_t *bitstream, uint32_t mode_mask,
    uint32_t flags, uint8_t *pixels);
extern bool detexDecompressBlockETC2_PUNCHTHROUGH(
    const uint8_t *bitstream, uint32_t mode_mask,
    uint32_t flags, uint8_t *pixels);

#define BG2V_DETEX_MODE_MASK_ALL 0xFFFFFFFFu

static unsigned int compressed_texture_count;

static GLuint bg2v_bound_texture_2d(void) {
    GLint texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    return texture > 0 ? (GLuint)texture : 0;
}

static void *bg2v_decode_etc2(
    GLenum internal_format, GLsizei width, GLsizei height,
    GLsizei image_size, const GLvoid *data) {
    if (data == NULL || width <= 0 || height <= 0) {
        return NULL;
    }

    unsigned int blocks_x = ((unsigned int)width + 3) / 4;
    unsigned int blocks_y = ((unsigned int)height + 3) / 4;
    size_t expected_size = (size_t)blocks_x * blocks_y * 8;
    size_t output_size = (size_t)width * height * 4;
    if (image_size < 0 || (size_t)image_size < expected_size ||
        output_size > UINT32_MAX) {
        return NULL;
    }

    uint32_t *output = (uint32_t *)vglMalloc((uint32_t)output_size);
    if (output == NULL) {
        return NULL;
    }

    const uint8_t *source = (const uint8_t *)data;
    bool (*decode_block)(const uint8_t *, uint32_t, uint32_t, uint8_t *) =
        internal_format ==
                BG2V_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2
            ? detexDecompressBlockETC2_PUNCHTHROUGH
            : detexDecompressBlockETC2;

    for (unsigned int block_y = 0; block_y < blocks_y; ++block_y) {
        for (unsigned int block_x = 0; block_x < blocks_x; ++block_x) {
            uint32_t block[16] = {0};
            bool decoded = decode_block(
                source, BG2V_DETEX_MODE_MASK_ALL, 0,
                (uint8_t *)block);
            source += 8;
            if (!decoded) {
                for (int i = 0; i < 16; ++i) {
                    block[i] = 0xFFFF00FFu;
                }
            }

            for (unsigned int y = 0; y < 4; ++y) {
                unsigned int destination_y = block_y * 4 + y;
                if (destination_y >= (unsigned int)height) break;
                for (unsigned int x = 0; x < 4; ++x) {
                    unsigned int destination_x = block_x * 4 + x;
                    if (destination_x >= (unsigned int)width) break;
                    output[(size_t)destination_y * width + destination_x] =
                        block[y * 4 + x];
                }
            }
        }
    }
    return output;
}

void gl_pointer_set(float x, float y, GLboolean visible) {
    pointer_screen_x = (int)x;
    pointer_screen_y = (int)y;
    __sync_synchronize();
    pointer_visible = visible ? 1 : 0;
}

static void pointer_clear_rect(int x, int y, int width, int height,
                               float red, float green, float blue) {
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > 960) width = 960 - x;
    if (y + height > 544) height = 544 - y;
    if (width <= 0 || height <= 0) return;

    glScissor(x, y, width, height);
    glClearColor(red, green, blue, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void gl_draw_pointer(void) {
    if (!pointer_visible) return;

    GLint framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    if (framebuffer != 0) return;

    GLboolean had_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint old_scissor[4];
    GLfloat old_clear[4];
    GLboolean old_mask[4];
    glGetIntegerv(GL_SCISSOR_BOX, old_scissor);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear);
    glGetBooleanv(GL_COLOR_WRITEMASK, old_mask);

    int x = pointer_screen_x;
    int y = 543 - pointer_screen_y;
    glEnable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    /* Black outline plus a bright cyan cross, readable on light/dark scenes. */
    pointer_clear_rect(x - 7, y - 1, 15, 3, 0.0f, 0.0f, 0.0f);
    pointer_clear_rect(x - 1, y - 7, 3, 15, 0.0f, 0.0f, 0.0f);
    pointer_clear_rect(x - 5, y, 11, 1, 0.1f, 1.0f, 1.0f);
    pointer_clear_rect(x, y - 5, 1, 11, 0.1f, 1.0f, 1.0f);

    glColorMask(old_mask[0], old_mask[1], old_mask[2], old_mask[3]);
    glClearColor(old_clear[0], old_clear[1], old_clear[2], old_clear[3]);
    glScissor(old_scissor[0], old_scissor[1],
              old_scissor[2], old_scissor[3]);
    if (!had_scissor) glDisable(GL_SCISSOR_TEST);
}

void gl_preload() {
    if (!file_exists("ur0:/data/libshacccg.suprx")
        && !file_exists("ur0:/data/external/libshacccg.suprx")) {
        fatal_error("Error: libshacccg.suprx is not installed. "
                    "Google \"ShaRKBR33D\" for quick installation.");
    }

#ifdef USE_GLSL_SHADERS
    vglSetSemanticBindingMode(VGL_MODE_POSTPONED);
#endif
}

void gl_init() {
    static GLboolean initialized;
    if (initialized) {
        return;
    }

    /*
     * BG2 renders at the Vita's native resolution and performs full-screen
     * texture uploads for movies. 4x MSAA multiplies that bandwidth without
     * helping the pre-rendered video, so keep the default framebuffer
     * single-sampled.
     */
    GLboolean fallback = vglInitExtended(
        0, 960, 544, 6 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    initialized = GL_TRUE;

    bg2v_log_printf(
        "[BG2V][VGL] initialized%s; pool free=%u KiB total=%u KiB; "
        "in-place texture updates enabled\n",
        fallback ? " with resolution fallback" : "",
        (unsigned int)(vglMemFree(VGL_MEM_ALL) / 1024),
        (unsigned int)(vglMemTotal(VGL_MEM_ALL) / 1024));
}

void gl_swap() {
    /*
     * GL_TRUE lets VitaGL composite a system common dialog when the Vita IME
     * keyboard is active. With no dialog open this follows the normal swap
     * path.
     */
    vglSwapBuffers(GL_TRUE);
}

void glCompressedTexImage2D_soloader(
    GLenum target, GLint level, GLenum internal_format,
    GLsizei width, GLsizei height, GLint border,
    GLsizei image_size, const GLvoid *data) {
    GLuint texture =
        target == GL_TEXTURE_2D ? bg2v_bound_texture_2d() : 0;

    size_t free_before = vglMemFree(VGL_MEM_ALL);
    void *decoded = NULL;
    if (internal_format == BG2V_GL_COMPRESSED_RGB8_ETC2 ||
        internal_format ==
            BG2V_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2) {
        decoded = bg2v_decode_etc2(
            internal_format, width, height, image_size, data);
    }

    if (decoded != NULL) {
        glTexImage2D(target, level, GL_RGBA, width, height, border,
                     GL_RGBA, GL_UNSIGNED_BYTE, decoded);
        vglFree(decoded);
    } else {
        glCompressedTexImage2D(
            target, level, internal_format, width, height, border,
            image_size, data);
    }
    size_t free_after = vglMemFree(VGL_MEM_ALL);

    ++compressed_texture_count;
    if (compressed_texture_count <= 96 ||
        internal_format == BG2V_GL_COMPRESSED_RGB8_ETC2 ||
        internal_format ==
            BG2V_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2) {
        const unsigned char *bytes = (const unsigned char *)data;
        bg2v_log_printf(
            "[BG2V][CTEX] image #%u id=%u level=%d %dx%d "
            "internal=0x%x bytes=%d data=%s prefix=%02x%02x%02x%02x "
            "path=%s pool %u -> %u KiB\n",
            compressed_texture_count, texture, level, width, height,
            internal_format, image_size, data != NULL ? "yes" : "no",
            data != NULL && image_size > 0 ? bytes[0] : 0,
            data != NULL && image_size > 1 ? bytes[1] : 0,
            data != NULL && image_size > 2 ? bytes[2] : 0,
            data != NULL && image_size > 3 ? bytes[3] : 0,
            decoded != NULL ? "ETC2-RGBA" : "vitaGL",
            (unsigned int)(free_before / 1024),
            (unsigned int)(free_after / 1024));
    }
}

/*
 * SDL resolves the EGL entry points dynamically. Keep the bridge visible in
 * bootstrap.log so a device test can distinguish "the engine is drawing
 * black" from "the engine never presents a frame".
 *
 * The first-swap trace proved the VitaGL display path on hardware. Leave the
 * game's framebuffer untouched from now on.
 */
EGLBoolean eglSwapBuffers_soloader(EGLDisplay display, EGLSurface surface) {
    (void)display;
    (void)surface;

    static unsigned int swap_count;
    static uint64_t timing_window_start_ms;
    ++swap_count;
    uint64_t now_ms = current_timestamp_ms();
    if (swap_count == 1) {
        timing_window_start_ms = now_ms;
    }
    if (swap_count <= 5) {
        bg2v_log_printf("[BG2V][EGL] swap #%u\n", swap_count);
    } else if ((swap_count % 120) == 0) {
        uint64_t elapsed_ms = now_ms - timing_window_start_ms;
        unsigned int fps_x10 = elapsed_ms > 0
            ? (unsigned int)(120ULL * 10000ULL / elapsed_ms)
            : 0;
        bg2v_log_printf(
            "[BG2V][EGL] swap #%u: last 120 frames in %llu ms "
            "(%u.%u fps)\n",
            swap_count, (unsigned long long)elapsed_ms,
            fps_x10 / 10, fps_x10 % 10);
        bg2v_log_printf(
            "[BG2V][VGL] swap #%u: pool free=%u KiB / %u KiB\n",
            swap_count,
            (unsigned int)(vglMemFree(VGL_MEM_ALL) / 1024),
            (unsigned int)(vglMemTotal(VGL_MEM_ALL) / 1024));
        if ((swap_count % 600) == 0) {
            struct mallinfo heap = mallinfo();
            bg2v_log_printf(
                "[BG2V][MEM] swap #%u: CPU heap arena=%u KiB "
                "used=%u KiB free=%u KiB\n",
                swap_count,
                (unsigned int)(heap.arena / 1024),
                (unsigned int)(heap.uordblks / 1024),
                (unsigned int)(heap.fordblks / 1024));
        }
        timing_window_start_ms = now_ms;
    }

    gl_draw_pointer();
    gl_swap();
    return EGL_TRUE;
}

EGLBoolean eglSwapInterval_soloader(EGLDisplay display, EGLint interval) {
    bg2v_log_printf("[BG2V][EGL] swap interval=%d\n", interval);
    return eglSwapInterval(display, interval);
}

EGLBoolean eglWaitGL_soloader(void) {
    bg2v_log_printf("[BG2V][EGL] eglWaitGL\n");
    return EGL_TRUE;
}

EGLBoolean eglWaitNative_soloader(EGLint engine) {
    bg2v_log_printf("[BG2V][EGL] eglWaitNative engine=%d\n", engine);
    return EGL_TRUE;
}

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glShaderSource<%p>(shader: %i, count: %i, string: %p, length: %p)\n", __builtin_return_address(0), shader, count, string, _length);
#endif
    if (!string) {
        l_error("<%p> Shader source string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    } else if (!*string) {
        l_error("<%p> Shader source *string is NULL, count: %i",
                   __builtin_return_address(0), count);
        skip_next_compile = GL_TRUE;
        return;
    }

    size_t total_length = 0;

    for (int i = 0; i < count; ++i) {
        if (!_length) {
            total_length += strlen(string[i]);
        } else {
            total_length += _length[i];
        }
    }

    char * str = malloc(total_length+1);
    size_t l = 0;

    for (int i = 0; i < count; ++i) {
        if (!_length) {
            memcpy(str + l, string[i], strlen(string[i]));
            l += strlen(string[i]);
        } else {
            memcpy(str + l, string[i], _length[i]);
            l += _length[i];
        }
    }
    str[total_length] = '\0';

    load_shader(shader, str, total_length);

    free(str);
}

void glCompileShader_soloader(GLuint shader) {
#ifdef DEBUG_OPENGL
    sceClibPrintf("[gl_dbg] glCompileShader<%p>(shader: %i)\n", __builtin_return_address(0), shader);
#endif

#ifndef USE_GXP_SHADERS
    if (!skip_next_compile) {
        glCompileShader(shader);
#ifdef DUMP_COMPILED_SHADERS
        void *bin = vglMalloc(32 * 1024);
        GLsizei len;
        vglGetShaderBinary(shader, 32 * 1024, &len, bin);
        file_save(next_shader_fname, bin, len);
        vglFree(bin);
#endif
    }
    skip_next_compile = GL_FALSE;
#endif
}

#if defined(USE_GLSL_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else {
        glShaderSource(shader, 1, &string, &length);
        strcpy(next_shader_fname, gxp_path);
    }

    free(sha_name);
}
#elif defined(USE_GLSL_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    glShaderSource(shader, 1, &string, &length);
}
#elif defined(USE_CG_SHADERS) && defined(DUMP_COMPILED_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char gxp_path[256];
    char cg_path[256];
    snprintf(gxp_path, sizeof(gxp_path), DATA_PATH"gxp/%s.gxp", sha_name);
    snprintf(cg_path, sizeof(cg_path), DATA_PATH"cg/%s.cg", sha_name);

    if (file_exists(gxp_path)) {
        uint8_t *buffer;
        size_t size;

        file_load(gxp_path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
        skip_next_compile = GL_TRUE;
    } else if (file_exists(cg_path)) {
        char *buffer;
        size_t size;

        file_load(cg_path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);
        strcpy(next_shader_fname, gxp_path);

        free(buffer);
        skip_next_compile = GL_FALSE;
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }

        skip_next_compile = GL_FALSE;
    }

    free(sha_name);
}
#elif defined(USE_CG_SHADERS) || defined(USE_GXP_SHADERS)
void load_shader(GLuint shader, const char * string, size_t length) {
    char* sha_name = str_sha1sum(string, length);

    char path[256];
#ifdef USE_CG_SHADERS
    snprintf(path, sizeof(path), DATA_PATH"cg/%s.cg", sha_name);
#else
    snprintf(path, sizeof(path), DATA_PATH"gxp/%s.gxp", sha_name);
#endif

    if (file_exists(path)) {
#ifdef USE_CG_SHADERS
        char *buffer;
        size_t size;

        file_load(path, (uint8_t **) &buffer, &size);

        glShaderSource(shader, 1, &string, &size);

        free(buffer);
#else
        uint8_t *buffer;
        size_t size;

        file_load(path, &buffer, &size);

        glShaderBinary(1, &shader, 0, buffer, (int32_t) size);

        free(buffer);
#endif
    } else {
        l_warn("Encountered an untranslated shader %s, saving GLSL "
               "and using a dummy shader.", sha_name);

        char glsl_path[256];
        snprintf(glsl_path, sizeof(glsl_path), DATA_PATH"glsl/%s.glsl", sha_name);
        file_mkpath(glsl_path, 0777);
        file_save(glsl_path, (const uint8_t *) string, length);

        if (strstr(string, "gl_FragColor")) {
            const char *dummy_shader = "float4 main() { return float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        } else {
            const char *dummy_shader = "void main(float4 out gl_Position : POSITION ) { gl_Position = float4(1.0,1.0,1.0,1.0); }";
            int32_t dummy_shader_len = (int32_t) strlen(dummy_shader);
            glShaderSource(shader, 1, &dummy_shader, &dummy_shader_len);
        }
    }

    free(sha_name);
}
#else
#error "Define one of (USE_GLSL_SHADERS, USE_CG_SHADERS, USE_GXP_SHADERS)"
#endif
