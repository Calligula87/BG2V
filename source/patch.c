/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  patch.c
 * @brief Patching some of the .so internal functions or bridging them to native
 *        for better compatibility.
 */

#include <kubridge.h>
#include <so_util/so_util.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#ifdef __cplusplus
extern "C"
{
#endif
	extern so_module so_mod;
#ifdef __cplusplus
};
#endif

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RX (0x0C20D050)
#define KUSER_HELPER_BASE ((uintptr_t)0x97000000)
#define KUSER_HELPER_SIZE ((size_t)0x1000)
#define KUSER_MEMORY_BARRIER (KUSER_HELPER_BASE + 0xFA0)
#define KUSER_CMPXCHG (KUSER_HELPER_BASE + 0xFC0)

#include "utils/logger.h"
#include "utils/dialog.h"
#include "reimpl/sys.h"
#include <stdbool.h>

/*
 * The engine embeds Lua 5.2 and exports its C API.  Keep the declarations
 * local so the diagnostic hooks do not require a second Lua implementation.
 */
typedef struct lua_State lua_State;
typedef int (*lua_CFunction)(lua_State *state);
typedef int (*lua_pcallk_fn)(lua_State *state, int nargs, int nresults,
							int errfunc, int ctx, lua_CFunction continuation);
typedef int (*luaL_loadbufferx_fn)(lua_State *state, const char *buffer,
								  size_t size, const char *name,
								  const char *mode);
typedef int (*luaL_loadfilex_fn)(lua_State *state, const char *filename,
								const char *mode);
typedef const char *(*lua_tolstring_fn)(lua_State *state, int index,
									   size_t *length);
typedef int (*lua_gettop_fn)(lua_State *state);
typedef void (*lua_settop_fn)(lua_State *state, int index);
typedef void (*luaL_traceback_fn)(lua_State *state, lua_State *source,
								  const char *message, int level);

static so_hook lua_pcallk_hook;
static so_hook lua_loadbuffer_hook;
static so_hook lua_loadfile_hook;
static lua_tolstring_fn engine_lua_tolstring;
static lua_gettop_fn engine_lua_gettop;
static lua_settop_fn engine_lua_settop;
static luaL_traceback_fn engine_luaL_traceback;

static int hooked_luaL_loadbufferx(lua_State *state, const char *buffer,
								   size_t size, const char *name,
								   const char *mode) {
	bg2v_log_printf(
		"[BG2V][LUA] loadbuffer name=%s size=%u mode=%s\n",
		name ? name : "(null)", (unsigned int)size, mode ? mode : "(null)");
	int result = SO_CONTINUE(
		int, lua_loadbuffer_hook, state, buffer, size, name, mode);
	if (result != 0 && engine_lua_tolstring != NULL) {
		const char *error = engine_lua_tolstring(state, -1, NULL);
		bg2v_log_printf("[BG2V][LUA] loadbuffer failed status=%d: %s\n",
					   result, error ? error : "(non-string error)");
	}
	return result;
}

static int hooked_luaL_loadfilex(lua_State *state, const char *filename,
								 const char *mode) {
	bg2v_log_printf("[BG2V][LUA] loadfile name=%s mode=%s\n",
				   filename ? filename : "(null)",
				   mode ? mode : "(null)");
	int result = SO_CONTINUE(
		int, lua_loadfile_hook, state, filename, mode);
	if (result != 0 && engine_lua_tolstring != NULL) {
		const char *error = engine_lua_tolstring(state, -1, NULL);
		bg2v_log_printf("[BG2V][LUA] loadfile failed status=%d: %s\n",
					   result, error ? error : "(non-string error)");
	}
	return result;
}

static int hooked_lua_pcallk(lua_State *state, int nargs, int nresults,
							 int errfunc, int ctx,
							 lua_CFunction continuation) {
	int result = SO_CONTINUE(
		int, lua_pcallk_hook, state, nargs, nresults, errfunc, ctx,
		continuation);
	if (result == 0 || engine_lua_tolstring == NULL) {
		return result;
	}

	const char *error = engine_lua_tolstring(state, -1, NULL);
	bg2v_log_printf(
		"[BG2V][LUA] pcall failed status=%d nargs=%d errfunc=%d: %s\n",
		result, nargs, errfunc, error ? error : "(non-string error)");

	/*
	 * Preserve the Lua stack exactly.  The traceback is diagnostic only and
	 * must not alter the engine's normal error handling.
	 */
	if (engine_lua_gettop != NULL && engine_lua_settop != NULL &&
		engine_luaL_traceback != NULL) {
		int top = engine_lua_gettop(state);
		engine_luaL_traceback(
			state, state, error ? error : "(non-string error)", 1);
		const char *traceback = engine_lua_tolstring(state, -1, NULL);
		bg2v_log_printf("[BG2V][LUA] traceback:\n%s\n",
					   traceback ? traceback : "(unavailable)");
		engine_lua_settop(state, top);
	}
	return result;
}

static void install_lua_diagnostics(void) {
	uintptr_t pcallk = so_symbol(&so_mod, "lua_pcallk");
	uintptr_t loadbuffer = so_symbol(&so_mod, "luaL_loadbufferx");
	uintptr_t loadfile = so_symbol(&so_mod, "luaL_loadfilex");

	engine_lua_tolstring =
		(lua_tolstring_fn)so_symbol(&so_mod, "lua_tolstring");
	engine_lua_gettop = (lua_gettop_fn)so_symbol(&so_mod, "lua_gettop");
	engine_lua_settop = (lua_settop_fn)so_symbol(&so_mod, "lua_settop");
	engine_luaL_traceback =
		(luaL_traceback_fn)so_symbol(&so_mod, "luaL_traceback");

	if (pcallk == 0 || loadbuffer == 0 || loadfile == 0 ||
		engine_lua_tolstring == NULL) {
		fatal_error("BG2V could not install Lua diagnostics.");
	}

	lua_pcallk_hook = hook_addr(pcallk, (uintptr_t)&hooked_lua_pcallk);
	lua_loadbuffer_hook =
		hook_addr(loadbuffer, (uintptr_t)&hooked_luaL_loadbufferx);
	lua_loadfile_hook =
		hook_addr(loadfile, (uintptr_t)&hooked_luaL_loadfilex);
	l_success("Lua diagnostics installed.");
}

void __kuser_memory_barrier(void) {
	__sync_synchronize();
}

static bool ranges_overlap(uintptr_t a, size_t a_size,
						   uintptr_t b, size_t b_size) {
	return a < b + b_size && b < a + a_size;
}

static void verify_kuser_page(void) {
	if (ranges_overlap(KUSER_HELPER_BASE, KUSER_HELPER_SIZE,
					   so_mod.text_base, so_mod.text_size)) {
		fatal_error("BG2V internal error: atomic helper page overlaps engine text.");
	}

	for (int i = 0; i < so_mod.n_data; ++i) {
		if (ranges_overlap(KUSER_HELPER_BASE, KUSER_HELPER_SIZE,
						   so_mod.data_base[i], so_mod.data_size[i])) {
			fatal_error("BG2V internal error: atomic helper page overlaps "
						"engine data segment %d.", i);
		}
	}
}

void kuser_patch(void) {
	verify_kuser_page();

	SceKernelAllocMemBlockKernelOpt opt;
	memset(&opt, 0, sizeof(SceKernelAllocMemBlockKernelOpt));
	opt.size = sizeof(SceKernelAllocMemBlockKernelOpt);
	opt.attr = 0x1;
	opt.field_C = (SceUInt32)KUSER_HELPER_BASE;
	int block_id = kuKernelAllocMemBlock(
		"bg2v_atomic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RX,
		KUSER_HELPER_SIZE, &opt);
	if (block_id < 0) {
		fatal_error("BG2V could not allocate atomic helper page at 0x%08x "
					"(error 0x%08x).", KUSER_HELPER_BASE, block_id);
	}
	kuKernelMemProtect((void *)KUSER_HELPER_BASE, KUSER_HELPER_SIZE,
					   KU_KERNEL_PROT_EXEC | KU_KERNEL_PROT_READ |
					   KU_KERNEL_PROT_WRITE);

	hook_addr(KUSER_MEMORY_BARRIER, (uintptr_t)__kuser_memory_barrier);
	hook_addr(KUSER_CMPXCHG, (uintptr_t)__atomic_cmpxchg);

	uint32_t patched_addr;
	for (uint32_t addr = so_mod.text_base; addr < so_mod.text_base + so_mod.text_size; addr += 4) {
		uint32_t *a = (uint32_t *)addr;
		if (*a == 0xFFFF0FC0) {
			l_debug("Patching 0x%x -> __kuser_cmpxchg", a);
			patched_addr = KUSER_CMPXCHG;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
		else if (*a == 0xFFFF0FA0) {
			l_debug("Patching 0x%x -> __kuser_memory_barrier", a);
			patched_addr = KUSER_MEMORY_BARRIER;
			kuKernelCpuUnrestrictedMemcpy((void *)(addr), &patched_addr, sizeof(uint32_t));
		}
	}
}

void so_patch(void) {
	kuser_patch();
	install_lua_diagnostics();
	// Sample hook with symbol name
	// hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6glitch2os7Printer5printEPKcz"), (uintptr_t)&hookedFunction);
	// Or with offset
	// hook_addr((uintptr_t)so_mod.text_base + 0xdeadbabe, (uintptr_t)&hookedFunction);
	// If you use SO_CONTINUE, define a so_hook before the function and assign to it
	// function_hook = hook_addr(...);
}
