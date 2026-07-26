/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/mem.h"
#include "utils/logger.h"

#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <psp2/kernel/clib.h>
#include <unistd.h>

void *sceClibMemclr(void *dst, size_t len) {
    return sceClibMemset(dst, 0, len);
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offs) {
    l_warn("mmap(%p, %i, %i, %i, %i, %li)", addr, length, prot, flags, fd, offs);

    if (length == 0 || offs < 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }

    void *ret = malloc(length);
    if (ret == NULL) {
        errno = ENOMEM;
        l_error("mmap allocation failed for %u bytes", (unsigned int)length);
        return MAP_FAILED;
    }
    memset(ret, 0, length);

    /*
     * Android's ZIP/OBB reader maps stored archive members directly from the
     * expansion-file descriptor.  Anonymous mappings need only the zeroed
     * allocation above; file-backed mappings must contain the requested bytes.
     */
    if (fd >= 0) {
        off_t original = lseek(fd, 0, SEEK_CUR);
        if (lseek(fd, offs, SEEK_SET) < 0) {
            l_error("mmap could not seek fd %d to %li", fd, offs);
            free(ret);
            return MAP_FAILED;
        }

        size_t total = 0;
        while (total < length) {
            ssize_t count = read(fd, (char *)ret + total, length - total);
            if (count > 0) {
                total += (size_t)count;
                continue;
            }
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0) {
                l_error("mmap read failed on fd %d after %u/%u bytes",
                        fd, (unsigned int)total, (unsigned int)length);
                if (original >= 0) {
                    lseek(fd, original, SEEK_SET);
                }
                free(ret);
                return MAP_FAILED;
            }
            break;
        }

        if (original >= 0) {
            lseek(fd, original, SEEK_SET);
        }
        l_debug("mmap loaded %u/%u bytes from fd %d at %li",
                (unsigned int)total, (unsigned int)length, fd, offs);
    }

    return ret;
}

int munmap(void *addr, size_t length) {
    if (addr != NULL && addr != MAP_FAILED) free(addr);
    return 0;
}
