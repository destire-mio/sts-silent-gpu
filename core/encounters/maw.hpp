#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct Maw {
    SPIRE_FN static void build(Game& g) { append_monster(g, construct_monster(g, MonsterId::Maw), -70.0f); }
};
}
