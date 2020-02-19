#ifndef __FYLEE_MACRO_H__
#define __FYLEE_MACRO_H__

#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"

#if defined __GNUC__ || defined __llvm__

#   define LIKELY(x)       __builtin_expect(!!(x), 1)

#   define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define LIKELY(x)      (x)
#   define UNLIKELY(x)      (x)
#endif


#define ASSERT(x) \
    if(UNLIKELY(!(x))) { \
        LOG_ERROR(LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << fylee::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#define ASSERT2(x, w) \
    if(UNLIKELY(!(x))) { \
        LOG_ERROR(LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << fylee::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif
