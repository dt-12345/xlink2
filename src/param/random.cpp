#include "param/random.hpp"

#include <algorithm>

namespace mango {
    
auto RandomTable::IsValidPower(float value) -> bool {
    return std::find(cValidPowers.begin(), cValidPowers.end(), value) != cValidPowers.end();
}

} // namespace mango