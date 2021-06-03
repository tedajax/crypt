#pragma once

#include "Remotery.h"

#define PROFILING_ENABLED 0

#if PROFILING_ENABLED
#define PROFILE_INIT()                                                                             \
    static Remotery* s_profiling_rmt_instance;                                                     \
    rmt_CreateGlobalInstance(&s_profiling_rmt_instance)
#define PROFILE_TERMINATE() rmt_DestroyGlobalInstance(s_profiling_rmt_instance)
#define PROFILE_BEGIN(name) rmt_BeginCPUSample(name, 0)
#define PROFILE_END() rmt_EndCPUSample();
#else
#define PROFILE_INIT()
#define PROFILE_TERMINATE()
#define PROFILE_BEGIN(name)
#define PROFILE_END()
#endif
