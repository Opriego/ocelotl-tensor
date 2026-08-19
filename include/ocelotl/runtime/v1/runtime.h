/* Copyright (C) 2026 Oscar Priego Verdugo */
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef OCELOTL_RUNTIME_V1_RUNTIME_H
#define OCELOTL_RUNTIME_V1_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#define OCELOTL_RUNTIME_ABI_VERSION_MAJOR 1
#define OCELOTL_RUNTIME_ABI_VERSION_MINOR 0
#define OCELOTL_RT_V1_ALLOC_NAME "ocelotl_rt_v1_alloc"
#define OCELOTL_RT_V1_FREE_NAME "ocelotl_rt_v1_free"
#define OCELOTL_RT_V1_LAST_ERROR_NAME "ocelotl_rt_v1_last_error"

#if defined(_WIN32) && defined(OCELOTLRT_SHARED)
#  if defined(OCELOTLRT_BUILDING)
#    define OCELOTLRT_API __declspec(dllexport)
#  else
#    define OCELOTLRT_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define OCELOTLRT_API __attribute__((visibility("default")))
#else
#  define OCELOTLRT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Returns suitably aligned storage, or NULL and records a diagnostic. */
OCELOTLRT_API void *ocelotl_rt_v1_alloc(uint64_t size, uint64_t alignment);

/* Releases storage returned by ocelotl_rt_v1_alloc. NULL is accepted. */
OCELOTLRT_API void ocelotl_rt_v1_free(void *pointer);

/* Returns a borrowed, thread-local diagnostic string. Never returns NULL. */
OCELOTLRT_API const char *ocelotl_rt_v1_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
