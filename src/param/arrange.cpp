#include "param/arrange.hpp"

namespace mango {

auto ArrangeGroup::setName(std::string_view name) -> void {
    mName = name;
}

} // namespace mango