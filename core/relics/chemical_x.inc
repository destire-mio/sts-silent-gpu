#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct ChemicalX : RelicHooks {
    static constexpr int BOOST = 2;
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
};
}
