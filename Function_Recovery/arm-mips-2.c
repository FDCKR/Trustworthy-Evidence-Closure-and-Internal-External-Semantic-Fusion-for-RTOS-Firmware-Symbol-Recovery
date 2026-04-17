/*
 * eCos inline/noinline experiment driver
 *
 * Purpose:
 *   Generate stable differences between:
 *     1) O2 default
 *     2) O2 inline-more
 *     3) O2 noinline
 *
 * Design:
 *   - Use official eCos program structure: cyg_user_start() + kernel thread.
 *   - Put the hot computation entirely inside this translation unit.
 *   - Use direct calls to small helper functions, not function pointers.
 *   - Use volatile sinks so the optimizer cannot discard the computation.
 */

#include <pkgconf/system.h>
#ifdef CYGPKG_KERNEL
# include <pkgconf/kernel.h>
#endif
#ifdef CYGPKG_LIBC
# include <pkgconf/libc.h>
#endif

#ifndef CYGFUN_KERNEL_API_C
# error Kernel API must be enabled to build this example
#endif

#ifndef CYGPKG_LIBC_STDIO
# error C library standard I/O must be enabled to build this example
#endif

#include <stdio.h>
#include <cyg/kernel/kapi.h>
#include <cyg/infra/cyg_type.h>
#include <cyg/hal/hal_arch.h>

#define NTHREADS  1
#define STACKSIZE (CYGNUM_HAL_STACK_SIZE_TYPICAL + 4096)

static cyg_handle_t thread_handle[NTHREADS];
static cyg_thread   thread_obj[NTHREADS];
static char         thread_stack[NTHREADS][STACKSIZE];

static volatile cyg_uint32 g_sink0 = 0;
static volatile cyg_uint32 g_sink1 = 0;

/*
 * Build-mode control:
 *
 *   default:
 *       no extra macro
 *
 *   inline-more:
 *       compile with -DINLINE_MORE
 *
 *   noinline:
 *       compile with -DNOINLINE_MODE
 */

#if defined(INLINE_MORE)

#define HOT_HELPER static inline __attribute__((always_inline))
#define HOT_HELPER_LARGE static inline __attribute__((always_inline))

#elif defined(NOINLINE_MODE)

#define HOT_HELPER static __attribute__((noinline))
#define HOT_HELPER_LARGE static __attribute__((noinline))

#else

#define HOT_HELPER static
#define HOT_HELPER_LARGE static

#endif

HOT_HELPER cyg_uint32 mix1(cyg_uint32 x)
{
    return (x << 5) ^ (x >> 3) ^ 0x9e3779b9U;
}

HOT_HELPER cyg_uint32 mix2(cyg_uint32 x)
{
    x += 0x7f4a7c15U;
    x ^= (x >> 7);
    return x;
}

HOT_HELPER cyg_uint32 mix3(cyg_uint32 x)
{
    return (x * 33U) + 17U;
}

HOT_HELPER cyg_uint32 mix4(cyg_uint32 x)
{
    x ^= (x << 11);
    x += 0x13579bdfU;
    return x;
}

HOT_HELPER cyg_uint32 mix5(cyg_uint32 x, cyg_uint32 i)
{
    return x ^ (i * 2654435761U);
}

/*
 * Slightly larger helper:
 * default O2 may or may not inline this depending on target/cost model;
 * INLINE_MORE forces it in;
 * NOINLINE_MODE forces it out.
 */
HOT_HELPER_LARGE cyg_uint32 round_block(cyg_uint32 x, cyg_uint32 i)
{
    x = mix1(x);
    x = mix2(x);
    x = mix3(x);
    x = mix4(x);
    x = mix5(x, i);

    x ^= (x >> 13);
    x += 0x85ebca6bU;
    x ^= (x << 9);

    return x;
}

HOT_HELPER_LARGE cyg_uint32 run_kernel(cyg_uint32 seed)
{
    cyg_uint32 x = seed;
    cyg_uint32 i;

    for (i = 0; i < 4000U; ++i) {
        x = round_block(x, i);
    }

    return x;
}

static void worker(cyg_addrword_t data)
{
    cyg_uint32 seed = (cyg_uint32)data + 0x12345678U;
    cyg_uint32 a, b, c;

    a = run_kernel(seed);
    b = run_kernel(seed ^ 0xabcdef01U);
    c = run_kernel(seed + 0x31415926U);

    g_sink0 = a ^ b;
    g_sink1 = b ^ c;

    printf("inline-demo: sink0=%08lx sink1=%08lx\n",
           (unsigned long)g_sink0,
           (unsigned long)g_sink1);

    cyg_thread_exit();
}

void cyg_user_start(void)
{
    cyg_thread_create(4,
                      worker,
                      (cyg_addrword_t)0,
                      "inline-demo",
                      (void *)thread_stack[0],
                      STACKSIZE,
                      &thread_handle[0],
                      &thread_obj[0]);

    cyg_thread_resume(thread_handle[0]);
}
