#pragma once

#include <string>

template<class T, class V = void>
struct member_info {
    using value_type = V;
    std::string name;
    V T::* ptr = nullptr;
};

template<typename T, typename Enable = void>
struct reflect {
private:
    static constexpr auto size = sizeof(T); // always trigger instantiation if T is templated
};

namespace useless {

template<typename T>
struct reflect {
private:
    static constexpr auto size = sizeof(T); // always trigger instantiation if T is templated
};

}
