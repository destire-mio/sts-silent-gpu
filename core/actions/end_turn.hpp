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
struct EndTurnSequence : ActionHooks {
    SPIRE_SNAPSHOT("this$0", room_reference)
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        // AbstractRoom.endTurn's anonymous action queues the turn transition after end-turn damage.
        Effect end{};
        end.kind = EffectKind::EndTurn;
        add_bottom(g, end);
        Effect start{};
        start.kind = EffectKind::MonsterStart;
        add_bottom(g, start);
        g.manager.monster_attacks_queued = false;
    }
};
}
