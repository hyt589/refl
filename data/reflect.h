#pragma once

#include <string>

template <class T, class V>
struct field_description_t {
    using value_type = V;
    std::size_t id;
    std::string name;
    V T::* ptr;
};

template <typename T> struct reflect {};
