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
// reflect<MyStruct> dummy1;
// reflect<Derived> dummy2;
} // namespace name

struct whatever {};

template <typename T, typename T1> struct MyTemplateStruct {
  T data;
  T get() { return data; }
  void nop() {}
  T1 gen() {
    T1 a;
    return a;
  }
  void params(const float p1, const name::Base &p2, const T1& p3) {}
};

reflect<MyTemplateStruct<name::Base, whatever>> dummy3;
// reflect<float> dummy4;
