
#pragma once
#include <switch.h>

#define VMOD_RC_TRY(rc) ({ \
    const auto _tmp_rc = (rc); \
    if(R_FAILED(_tmp_rc)) { \
        return _tmp_rc; \
    } \
})

#define VMOD_RC_ASSERT(rc) ({ \
    const auto _tmp_rc = (rc); \
    if(R_FAILED(_tmp_rc)) { \
        svcBreak(0, reinterpret_cast<uintptr_t>(&_tmp_rc), sizeof(_tmp_rc)); \
    } \
})

#define VMOD_ASSERT_TRUE(expr) ({ \
    const auto _tmp_expr = (expr); \
    if(!_tmp_expr) { \
        svcBreak(0, 0xCAFEDEAD, __LINE__); \
    } \
})
