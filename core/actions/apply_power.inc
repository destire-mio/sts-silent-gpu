#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ApplyPower : ActionHooks {
    SPIRE_SNAPSHOT("ApplyPowerAction.powerToApply", power_value, effect.payload.power, effect.target, {20, queue, index})
    static SPIRE_FN constexpr ActionData data() { return {false, "POWER"}; }
    SPIRE_FN static Effect make(int target, int source, const Power& power, int amount) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        action_initialize_state(e);
        e.source = source;
        e.target = target;
        e.power = power.id;
        e.payload = EffectPayload(power);
        e.amount = amount;
        e.power_constructed = true;
        return e;
    }
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        if (!e.power_constructed) {
            Power power{};
            power.id = e.power;
            power.amount = e.amount;
            power.just_applied = e.flag;
            power_constructPower(g, power, e.target, e.source, e.extra, e.card);
            e.payload = EffectPayload(power);
            e.power_constructed = true;
        }
        Power& power = e.payload.power;
        if (power.instance == 0) power.instance = next_power_identity(g);
        if (e.source == player_target && e.target != e.source && power.id == PowerId::Poison) {
            for (int i = 0; i < g.run.relics.size; ++i) {
                const int bonus = relic_data(g.run.relics[i].id).poison_bonus;
                power.amount += bonus;
                e.amount += bonus;
            }
        }
        if (power.id == PowerId::Corruption) {
            const Pile* piles[] = {&g.hand, &g.draw, &g.discard, &g.exhaust};
            for (const auto* pile : piles) for (int i = 0; i < pile->size; ++i) {
                Card& card = g.cards[(*pile)[i]];
                if (card_data(card.id).type == CardType::Skill) modify_cost_for_combat(card, -9);
            }
        }
        if (monsters_basically_dead(g)) e.done = true;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.target == no_target) return;
        const Creature& target = get_creature(g, e.target);
        if (!alive(target)) return;
        if (!power_data(e.payload.power.id).can_stack && has_power(g, target, e.payload.power.id)) return;
        if (e.target == player_target)
            for (int i = 0; i < g.run.relics.size; ++i)
                if (relic_data(g.run.relics[i].id).blocked_debuff == e.payload.power.id) {
                    settle_deaths_after_wait(g, 13);
                    return;
                }
        apply_prepared_power(g, e.source, e.target, e.payload.power, e.amount);
        // ApplyPowerAction.update refreshes first, then waits at least ACTION_DUR_FASTER.
        // Both normal-speed constructors guarantee 13 death-animation ticks before the next action.
        settle_deaths_after_wait(g, 13);
    }
};
}
