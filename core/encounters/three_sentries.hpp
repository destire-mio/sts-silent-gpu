#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ThreeSentries {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::Sentry), -330.0f);
        append_monster(g, construct_monster(g, MonsterId::Sentry), -85.0f);
        append_monster(g, construct_monster(g, MonsterId::Sentry), 140.0f);
    }
};
}
