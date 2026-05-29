#include "asset/switch.hpp"

namespace mango {

auto Switch::setSwitchVariable(std::string_view var) -> void {
    mSwitchVariable = var;
}

} // namespace mango