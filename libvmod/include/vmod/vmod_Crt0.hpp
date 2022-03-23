
#pragma once
#include <vmod/dyn/dyn_Module.hpp>

namespace vmod::crt0 {

    struct LoaderContext {
        Handle main_thread_h;
        const dyn::ModuleInfo *builtin_modules;
        size_t builtin_module_count;
        const dyn::ModuleInfo *modules;
        size_t module_count;
        u64 code_start_addr;
        size_t code_size;
    };

    LoaderContext *GetLoaderContext();

    LoaderReturnFn GetReturnFunction();
    void SetReturnFunction(LoaderReturnFn ret_fn);

}