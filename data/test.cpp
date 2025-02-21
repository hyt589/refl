#include "reflect.h"

namespace name {
struct MyStruct {
    int x;
    float y;
    void method_sss() {}
};

struct Base {
    float a;
    int b;
};
struct Derived : Base {
    void wtf(int damn) {}
};

} // namespace name

struct whatever {};

template<typename T, typename T1, int v = 0>
struct MyTemplateStruct {
    T data;
    T1 data1;
    T get() {
        return data;
    }
    void nop() {}
    T1 gen() {
        T1 a;
        return a;
    }
    void params(const float p1, const name::Base& p2, const T1& p3) {}

    void operator+(){};
};

void f() {
    using MyStructReflection = reflect<name::MyStruct>;
    using BaseReflection = reflect<name::Base>;
    using MyTemplateStructReflection = reflect<MyTemplateStruct<name::Base, whatever>>;
    constexpr auto fields = MyTemplateStructReflection::fields();
    constexpr auto methods = MyTemplateStructReflection::methods();
    MyTemplateStruct<name::Base, whatever> instance;
    constexpr auto sz = std::tuple_size_v<decltype(fields)>;
    constexpr auto field = std::get<0>(fields);
    auto method = std::get<4>(methods);
    (instance.*method.func_ptr)();
    [[maybe_unused]] auto v = (instance.*field.ptr);

}
