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
	// Sample hook with symbol name
	// hook_addr((uintptr_t)so_symbol(&so_mod, "_ZN6glitch2os7Printer5printEPKcz"), (uintptr_t)&hookedFunction);
	// Or with offset
	// hook_addr((uintptr_t)so_mod.text_base + 0xdeadbabe, (uintptr_t)&hookedFunction);
	// If you use SO_CONTINUE, define a so_hook before the function and assign to it
	// function_hook = hook_addr(...);
}
