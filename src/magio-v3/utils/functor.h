#ifndef MAGIO_UTILS_FUNCTOR_H_
#define MAGIO_UTILS_FUNCTOR_H_

#include <cstdio>

#include "magio-v3/utils/traits.h"
#include "magio-v3/utils/noncopyable.h"

namespace magio {

template<typename F>
class Functor;

template<typename Ret, typename...Args>
class Functor<Ret(Args...)>: Noncopyable {
    // int->int&&, int&->int&, int&&->int&&
    template<typename T>
    using BeRef = std::conditional_t<std::is_reference_v<T>, T, T&&>;

    class FunctorBase {
    public:
        virtual ~FunctorBase() = default;
        virtual Ret operator()(BeRef<Args>...args) = 0;
    };

    template<typename...>
    class FunctorImpl;

    template<typename Class>
    class FunctorImpl<Class>: public FunctorBase {
    public:
        FunctorImpl(const Class& other)
            : obj_(other) 
        { }

        FunctorImpl(Class&& other)
            : obj_(std::move(other))
        { }

        Ret operator()(BeRef<Args>...args) override {
            return obj_(std::forward<Args>(args)...);
        }

    private:
        Class obj_;
    };
    
public:
    using Self = Functor<Ret(Args...)>;

    enum Strategy {
        CFunction,
        ClassFunction
    };

    Functor() { 
        impl_.ptr = nullptr;
    }

    ~Functor() {
        if (impl_.ptr && strategy_ == ClassFunction) {
            delete impl_.functorbase;
        }
    }

    Functor(Ret(*cfunc)(Args...)) {
        strategy_ = CFunction;
        impl_.cfunc = cfunc;
    }

    template<
        typename Class, 
        constraint<
            std::is_class_v<Class> &&
            !std::is_same_v<Class, Self> &&
            std::is_invocable_v<Class, Args...>
        > = 0
    >
    Functor(const Class& lobj) {
        strategy_ = ClassFunction;
        impl_.functorbase = new FunctorImpl<Class>(lobj);
    }

    template<
        typename Class, 
        constraint<
            std::is_class_v<Class> &&
            !std::is_same_v<Class, Self> &&
            std::is_invocable_v<Class, Args...>
        > = 0
    >
    Functor(Class&& robj) {
        strategy_ = ClassFunction;
        impl_.functorbase = new FunctorImpl<Class>(std::move(robj));
    }

    Functor(Functor&& other) noexcept // init
        : strategy_(other.strategy_)
        , impl_(other.impl_) 
    {
        if (impl_.ptr) {
            other.impl_.ptr = nullptr;
        }
    }

    Functor& operator=(Functor&& other) noexcept {
        if (impl_.ptr == other.impl_.ptr) { // self
            return *this;
        }

        strategy_ = other.strategy_;
        impl_ = other.impl_;
        if (impl_.ptr) {
            other.impl_.ptr = nullptr;
        }
        return *this;
    }

    void swap(Functor& other) {
        std::swap(strategy_, other.strategy_);
        std::swap(impl_, other.impl_);
    }

    Ret operator()(Args...args) {
        switch(strategy_) {
        case CFunction:
            impl_.cfunc(std::forward<Args>(args)...);
            break;
        case ClassFunction:
            (*impl_.functorbase)(std::forward<Args>(args)...);
            break;
        }
    }

    operator bool() {
        return impl_.ptr;
    }

private:
    Strategy strategy_;
    union {
        Ret(*cfunc)(Args...);
        FunctorBase* functorbase;
        void* ptr;
    } impl_;
};

}

#endif