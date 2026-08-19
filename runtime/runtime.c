#define _POSIX_C_SOURCE 200112L

#include "ocelotl/runtime/v1/runtime.h"

#include <errno.h>
#include <stdlib.h>

static _Thread_local const char *ocelotl_rt_error = "";

void *ocelotl_rt_v1_alloc(uint64_t size, uint64_t alignment)
{
    void *pointer = NULL;

    if (size == 0) {
        ocelotl_rt_error = "allocation size must be greater than zero";
        return NULL;
    }
    if (size > (uint64_t)SIZE_MAX) {
        ocelotl_rt_error = "allocation size exceeds the target address space";
        return NULL;
    }
    if (alignment < sizeof(void *) ||
        (alignment & (alignment - 1)) != 0 ||
        alignment > (uint64_t)SIZE_MAX) {
        ocelotl_rt_error =
            "alignment must be a power of two and at least pointer-sized";
        return NULL;
    }

    const int result = posix_memalign(
        &pointer, (size_t)alignment, (size_t)size);
    if (result != 0) {
        ocelotl_rt_error = result == ENOMEM
            ? "runtime allocation failed: out of memory"
            : "runtime allocation failed";
        return NULL;
    }

    ocelotl_rt_error = "";
    return pointer;
}

void ocelotl_rt_v1_free(void *pointer)
{
    free(pointer);
}

const char *ocelotl_rt_v1_last_error(void)
{
    return ocelotl_rt_error;
}
