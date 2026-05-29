#pragma once

#include <string>

namespace mango {

enum class PropertyType {
    Enum,
    S32,
    F32,
    Bool,
    // there's two more but they seem to be the same as S32 and F32?
};

enum class PropertyScope {
    Local,
    Global,
};

struct PropertyInfo {
    std::string name    = "";
    PropertyScope scope = PropertyScope::Local;
};

} // namespace mango