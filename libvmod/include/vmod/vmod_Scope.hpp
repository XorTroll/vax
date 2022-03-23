
#pragma once
#include <vmod/vmod_Attributes.hpp>
#include <utility>

namespace vmod {

    template<class F>
    class ScopeGuard {
        private:
            F f;
            bool active;
        public:
            inline constexpr ScopeGuard(F f) : f(std::move(f)), active(true) { }
            inline constexpr ~ScopeGuard() { if (active) { f(); } }
            inline constexpr void Cancel() { active = false; }

            inline constexpr ScopeGuard(ScopeGuard&& rhs) : f(std::move(rhs.f)), active(rhs.active) {
                rhs.Cancel();
            }

            ScopeGuard &operator=(ScopeGuard&& rhs) = delete;
    };

    template<class F>
    inline constexpr ScopeGuard<F> MakeScopeGuard(F f) {
        return ScopeGuard<F>(std::move(f));
    }

    enum class ScopeGuardOnExit {};

    template <typename F>
    inline constexpr ScopeGuard<F> operator+(ScopeGuardOnExit, F&& f) {
        return ScopeGuard<F>(std::forward<F>(f));
    }

}

#define _VMOD_CONCATENATE_IMPL(s1, s2) s1##s2
#define _VMOD_CONCATENATE(s1, s2) _VMOD_CONCATENATE_IMPL(s1, s2)
#define _VMOD_ANONYMOUS_VARIABLE(pref) _VMOD_CONCATENATE(pref, __COUNTER__)

#define VMOD_SCOPE_GUARD ::vmod::ScopeGuardOnExit() + [&]() VMOD_ALWAYS_INLINE
#define VMOD_ON_SCOPE_EXIT auto _VMOD_ANONYMOUS_VARIABLE(__vmod_scope_guard_) = VMOD_SCOPE_GUARD
