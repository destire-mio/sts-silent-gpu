#pragma once
#include "../hooks.hpp"
#include "wait.hpp"

namespace spire::units::actions {
struct EndTurn : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        g.manager.turn_has_ended = true;
        g.manager.player_hp_last_turn = g.player.creature.hp;
    }
};
struct RoomEndTurn : ActionHooks {
    SPIRE_SNAPSHOT("AbstractRoom$1.this$0", reference, 12, 0, 0)
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        Effect end{};
        end.kind = EffectKind::EndTurn;
        add_bottom(g, end);
        // AbstractRoom.endTurn queues WaitAction(1.2f) before MonsterStartTurnAction.
        add_bottom(g, Wait::make(73));
        Effect start{};
        start.kind = EffectKind::MonsterStart;
        if (!g.skip_monster_turn) add_bottom(g, start);
        g.manager.monster_attacks_queued = false;
    }
};
}
