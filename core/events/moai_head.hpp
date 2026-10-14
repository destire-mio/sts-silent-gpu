#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct MoaiHead : EventHooks {
    SPIRE_STATE(MoaiHead, int hpAmt; uint8_t screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.MoaiHead", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return has_relic(g, "Golden Idol") || static_cast<float>(g.player.creature.hp) / g.player.creature.max_hp <= 0.5f; }
    static SPIRE_FN void enter(Game& g) {
        g.event.state.MoaiHead.hpAmt = floor_int(g.player.creature.max_hp * 0.18f + 0.5f);
        event_options(g, 3); g.image_dialog[1] = !has_relic(g, "Golden Idol");
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.MoaiHead;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0) {
            // MoaiHead.buttonEffect clamps current HP before the minimum max HP and bypasses decreaseMaxHealth hooks.
            g.player.creature.max_hp -= s.hpAmt;
            g.player.creature.hp = min(g.player.creature.hp, g.player.creature.max_hp);
            g.player.creature.max_hp = max(1, g.player.creature.max_hp);
            heal_creature(g, player_target, g.player.creature.max_hp);
        } else if (option == 1) { unequip_relic(g, RelicId::GoldenIdol); gain_gold(g, 333); }
        s.screenNum = 1; event_options(g, 1);
    }
};
}
