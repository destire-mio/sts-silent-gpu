#pragma once
#include "three_shapes.hpp"

namespace spire::units::encounters {
struct FourShapes {
    SPIRE_FN static void build(Game& g) { ThreeShapes::spawn(g, 4); }
};
}
