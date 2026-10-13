#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EndTurn : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        g.manager.turn_has_ended = true;
        g.manager.player_hp_last_turn = g.player.creature.hp;
    }
};
}
