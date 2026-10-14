#pragma once
#include "../api.hpp"
SPIRE_RULE("rule:characters.AbstractPlayer:channelOrb")

namespace spire {
SPIRE_FN inline void channel_orb(Game& g, int index) {
    if (g.player.max_orbs <= 0) return;
    if (has_relic(g, "Dark Core") && g.orbs.objects[index].kind != OrbKind::Dark) index = make_orb(g, OrbKind::Dark);
    for (int i = 0; i < g.orbs.slots.size; ++i) {
        if (g.orbs.slots[i] >= 0) continue;
        g.orbs.slots[i] = index;
        Powers& powers = g.player.creature.powers;
        for (int j = 0; j < powers.size; ++j) power_onChannel(g, powers.at(g.power_objects, j), player_target, index);
        g.orbs.channeled.push_back(index); apply_orb_focus(g, index); return;
    }
    add_top(g, units::actions::Channel::make(index));
    add_top(g, units::actions::EvokeOrb::make(1));
    add_top(g, units::actions::AnimateOrb::make(1));
}
}
