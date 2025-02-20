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

// Trigger reflection for these types
reflect<MyStruct> dummy1;
reflect<Derived> dummy2;
useless::reflect<MyStruct> dummy12;
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
};

reflect<MyTemplateStruct<name::Base, whatever>> dummy3;

using t1 = decltype(&MyTemplateStruct<name::Base, whatever>::params);

// MyTemplateStruct<name::Base, whatever> template_data;
// reflect<decltype(template_data)> dummy4;
reflect<float> dummy4;
