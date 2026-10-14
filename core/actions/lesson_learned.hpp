#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct LessonLearned : ActionHooks {
    SPIRE_WAITING_RULE("LessonLearnedAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_SNAPSHOT("LessonLearnedAction.theCard", card, effect.card)
    SPIRE_SNAPSHOT("LessonLearnedAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void prepare(Game &, Effect& e) {
        action_initialize_state(e);
        e.card = -1;
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target == no_target)
            return;
        creature_damage(g, e);
        const Creature &c = get_creature(g, e.target);
        if ((c.dying || c.hp <= 0) && !c.half_dead && !has_power(g, c, "Minion")) {
            List<uint8_t, master_deck_capacity> candidates{};
            for (int i = 0; i < g.run.master_deck.size; ++i)
                if (card_canUpgrade(g, -i - 2))
                    candidates.push_back(i);
            if (!candidates.empty()) {
                e.card = -candidates[g.rng.misc.random(candidates.size - 1)] - 2;
                card_upgrade(get_card(g, e.card));
            }
        }
        if (monsters_basically_dead(g))
            clear_post_combat_actions(g);
    }
};
}
