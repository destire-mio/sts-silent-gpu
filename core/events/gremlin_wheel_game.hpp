#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct GremlinWheelGame : EventHooks {
    SPIRE_STATE(GremlinWheelGame, int result; int goldAmount; uint8_t screen; bool startSpin; bool buttonPressed; bool purgeResult;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.GremlinWheelGame", true, false, "spin"}; }
    SPIRE_SNAPSHOT("GremlinWheelGame.screen", scalar, event.state.GremlinWheelGame.screen == 0 ? "INTRO" : event.state.GremlinWheelGame.screen == 1 ? "COMPLETE" : "LEAVE")
    SPIRE_SNAPSHOT("GremlinWheelGame.doneSpinning", scalar, false)
    SPIRE_SNAPSHOT("GremlinWheelGame.finishSpin", scalar, false)
    SPIRE_SNAPSHOT("GremlinWheelGame.hpLossPercent", scalar, 0.15f)
    static SPIRE_FN void enter(Game& g) {
        g.event.no_cards = true;
        g.event.state.GremlinWheelGame.goldAmount = g.run.act < 4 ? g.run.act * 100 : 0;
        event_options(g, 1);
    }
    static SPIRE_FN void choices(const Game& g, EventChoices& choices) {
        if (g.event.state.GremlinWheelGame.startSpin) choices.push_back({EventChoiceKind::Control, 0});
        else event_dialog_choices(g, choices);
    }
    static SPIRE_FN void choose(Game& g, int) {
        auto& s = g.event.state.GremlinWheelGame;
        if (s.startSpin) {
            s.buttonPressed = true; s.startSpin = false; s.screen = 1;
            if (s.result == 0) gain_gold(g, s.goldAmount);
            event_options(g, 1);
        } else if (s.screen == 0) {
            s.result = g.rng.misc.random(0, 5); s.startSpin = true;
        } else if (s.screen == 1) {
            s.screen = 2; event_options(g, 1);
            // EventRoom.update completes the room after applyResult clears hasFocus.
            if (s.result < 4) { g.event.has_focus = false; g.room_phase = RoomPhase::Complete; }
            if (s.result == 1) {
                g.room_rewards.clear();
                RoomReward reward{}; reward.kind = RewardKind::Relic;
                reward.relic.id = random_screenless_relic(g, random_relic_tier(g));
                g.room_rewards.push_back(create_reward(g, reward));
                g.room_phase = RoomPhase::Complete;
                open_room_card_rewards(g);
                g.reward_proceed_visible = true; g.combat_reward_skippable = false;
            } else if (s.result == 2) heal_creature(g, player_target, g.player.creature.max_hp);
            else if (s.result == 3) queue_card_obtain(g, make_card(CardId::Decay));
            else if (s.result == 4) {
                event_purge_cards(g);
                if (!g.grid_cards.empty()) { open_master_selection(g, 1, true); s.purgeResult = true; }
            } else if (s.result == 5) {
                DamageInfo damage{}; damage.source = no_target; damage.target = player_target;
                damage.amount = static_cast<int>(g.player.creature.max_hp * 0.15f); damage.damage_type = DamageType::HpLoss;
                player_damage(g, damage);
            }
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.GremlinWheelGame;
        if (!s.purgeResult || g.selected_cards.empty()) return;
        remove_master_card(g, g.selected_cards[0]); g.selected_cards.clear(); s.purgeResult = false;
        g.event.has_focus = false;
        g.room_phase = RoomPhase::Complete;
    }
};
}
