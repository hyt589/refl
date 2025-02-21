#pragma once

#include <string>

template<typename T, typename V>
struct member_info {
    using value_type = V;
    std::string_view name;
    V T::* ptr = nullptr;
};

template<typename T, typename F>
struct method_info {
    using method_type = F;
    std::string_view name;
    F func_ptr;
};

template<typename T, typename Enable = void>
struct reflect {
    static constexpr const char* name() {
        return "";
    }
    static constexpr auto fields() ;
    static constexpr auto methods() ;

private:
    static constexpr auto size = sizeof(T); // always trigger instantiation if T is templated
};


