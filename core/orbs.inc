#pragma once
#include "core/orbs/lightning.hpp"
#include "core/orbs/frost.hpp"
#include "core/orbs/dark.hpp"
#include "core/orbs/plasma.hpp"
#include "core/actions/damage_all_enemies.hpp"
#include "core/actions/channel.hpp"
#include "core/actions/evoke_orb.hpp"
#include "core/actions/animate_orb.hpp"

namespace spire {
SPIRE_FN inline int orb_base_passive(OrbKind kind) {
    switch (kind) {
    case OrbKind::Lightning: return units::orbs::Lightning::passive();
    case OrbKind::Frost: return units::orbs::Frost::passive();
    case OrbKind::Dark: return units::orbs::Dark::passive();
    case OrbKind::Plasma: return units::orbs::Plasma::passive();
    }
    SPIRE_ASSERT(false); __builtin_unreachable();
}
SPIRE_FN inline int orb_base_evoke(OrbKind kind) {
    switch (kind) {
    case OrbKind::Lightning: return units::orbs::Lightning::evoke();
    case OrbKind::Frost: return units::orbs::Frost::evoke();
    case OrbKind::Dark: return units::orbs::Dark::evoke();
    case OrbKind::Plasma: return units::orbs::Plasma::evoke();
    }
    SPIRE_ASSERT(false); __builtin_unreachable();
}
SPIRE_FN inline void apply_orb_focus(Game& g, int index) {
    Orb& orb = g.orbs.objects[index];
    const OrbKind kind = g.orbs.kind(index);
    const int focus = kind == OrbKind::Plasma ? 0 : power_amount(g, g.player.creature, PowerId::Focus);
    orb.passive = max(0, orb_base_passive(kind) + focus);
    if (kind != OrbKind::Dark) orb.evoke = max(0, orb_base_evoke(kind) + focus);
}
SPIRE_FN inline int make_orb(Game& g, OrbKind kind) {
    const int index = g.orbs.add(kind, orb_base_passive(kind), orb_base_evoke(kind));
    apply_orb_focus(g, index); return index;
}
SPIRE_FN inline void orb_start_turn(Game& g, int index) {
    if (index >= 0 && g.orbs.kind(index) == OrbKind::Plasma) units::orbs::Plasma::onStartOfTurn(g, index);
}
SPIRE_FN inline void orb_end_turn(Game& g, int index) {
    if (index < 0) return;
    switch (g.orbs.kind(index)) {
    case OrbKind::Lightning: units::orbs::Lightning::onEndOfTurn(g, index); break;
    case OrbKind::Frost: units::orbs::Frost::onEndOfTurn(g, index); break;
    case OrbKind::Dark: units::orbs::Dark::onEndOfTurn(g, index); break;
    case OrbKind::Plasma: break;
    }
}
SPIRE_FN inline void orb_evoke(Game& g, int index) {
    switch (g.orbs.kind(index)) {
    case OrbKind::Lightning: units::orbs::Lightning::onEvoke(g, index); break;
    case OrbKind::Frost: units::orbs::Frost::onEvoke(g, index); break;
    case OrbKind::Dark: units::orbs::Dark::onEvoke(g, index); break;
    case OrbKind::Plasma: units::orbs::Plasma::onEvoke(g, index); break;
    }
}
SPIRE_FN inline void lightning_damage(Game& g, const Effect& e) {
    if (e.flag) {
        int amounts[monster_capacity]{};
        for (int i = 0; i < g.monster_order.size; ++i)
            amounts[i] = has_power(g, g.monsters[g.monster_order[i]].creature, PowerId::LockOn) ? static_cast<int>(e.info_base * 1.5f) : e.info_base;
        add_top(g, units::actions::DamageAllEnemies::make(g, player_target, amounts, g.monster_order.size, DamageType::Thorns));
    } else {
        const int target = random_living_monster(g);
        if (target == no_target) return;
        Effect hit{}; hit.kind = EffectKind::Damage; hit.target = target; hit.damage_type = DamageType::Thorns;
        hit.info_base = e.info_base;
        hit.amount = has_power(g, get_creature(g, target), PowerId::LockOn) ? static_cast<int>(e.info_base * 1.5f) : e.info_base;
        hit.flag = true; add_top(g, hit);
    }
}
}
#include "core/orbs/has_orb.hpp"
#include "core/orbs/has_empty_orb.hpp"
#include "core/orbs/filled_orb_count.hpp"
#include "core/orbs/evoke_orb.hpp"
#include "core/orbs/evoke_newest_orb.hpp"
#include "core/orbs/evoke_without_losing_orb.hpp"
#include "core/orbs/remove_next_orb.hpp"
#include "core/orbs/increase_max_orb_slots.hpp"
#include "core/orbs/decrease_max_orb_slots.hpp"
#include "core/orbs/channel_orb.hpp"
#include "core/orbs/start_turn_orbs.hpp"
#include "core/orbs/end_turn_orbs.hpp"
