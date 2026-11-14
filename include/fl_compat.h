#ifndef FL_COMPAT_H
#define FL_COMPAT_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <fcntl.h>

/* GCC8's glibc2.28 sysroot redirects fcntl to the newly versioned fcntl64
 * symbol when large-file support is enabled. Onion's original firmware can
 * have older glibc. We only use F_SETFL/F_SETFD with integer arguments, which
 * require no large-file locking structures; bind those calls to legacy fcntl.
 * Keep _FILE_OFFSET_BITS=64 for open/stat and all ROM sizes. */
#if defined(__arm__) && defined(__GLIBC__)
extern int fl_fcntl_compat(int descriptor, int operation, ...);
__asm__(".symver fl_fcntl_compat,fcntl@GLIBC_2.4");
#define fcntl fl_fcntl_compat
#endif
#endif
