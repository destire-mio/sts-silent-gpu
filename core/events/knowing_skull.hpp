#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct KnowingSkull : EventHooks {
    SPIRE_STATE(KnowingSkull, int potionCost; int cardCost; int goldCost; int leaveCost; int damageTaken; int goldEarned; uint8_t screen; uint8_t count; uint32_t choices; uint16_t grants[16];)
    static SPIRE_FN constexpr EventData data() { return {"events.city.KnowingSkull", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 && g.player.creature.hp > 12; }
    SPIRE_SNAPSHOT("KnowingSkull.screen", scalar, event.state.KnowingSkull.screen == 0 ? "INTRO_1" : event.state.KnowingSkull.screen == 1 ? "ASK" : "COMPLETE")
    SPIRE_SNAPSHOT("KnowingSkull.options", scalar, [&]() -> pybind11::object { pybind11::list result; for (const char* value : {"CARD", "GOLD", "POTION", "LEAVE"}) result.append(value); return result; }())
    SPIRE_SNAPSHOT("KnowingSkull.optionsChosen", scalar, [&]() { std::string result; const auto& s = event.state.KnowingSkull; for (int i = 0; i < s.count; ++i) result += ((s.choices >> (2 * i)) & 3) == 0 ? "POTION " : ((s.choices >> (2 * i)) & 3) == 1 ? "GOLD " : "CARD "; return result; }())
    SPIRE_SNAPSHOT("KnowingSkull.cards", scalar, [&]() -> pybind11::object { pybind11::list result; const auto& s = event.state.KnowingSkull; for (int i = 0; i < s.count; ++i) if (((s.choices >> (2 * i)) & 3) == 2) result.append(card_names[s.grants[i]]); return result; }())
    SPIRE_SNAPSHOT("KnowingSkull.potions", scalar, [&]() -> pybind11::object { pybind11::list result; const auto& s = event.state.KnowingSkull; for (int i = 0; i < s.count; ++i) if (((s.choices >> (2 * i)) & 3) == 0 && s.grants[i]) result.append(potion_names[s.grants[i]]); return result; }())
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.KnowingSkull;
        s.potionCost = s.cardCost = s.goldCost = s.leaveCost = 6;
        event_options(g, 1);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.KnowingSkull;
        if (s.screen == 0) { s.screen = 1; event_options(g, 4); return; }
        if (s.screen == 2) { open_map(g); return; }
        int& cost = option == 0 ? s.potionCost : option == 1 ? s.goldCost : option == 2 ? s.cardCost : s.leaveCost;
        DamageInfo damage{}; damage.source = no_target; damage.target = player_target;
        damage.amount = cost; damage.damage_type = DamageType::HpLoss;
        player_damage(g, damage); s.damageTaken += cost;
        if (option == 3) { s.screen = 2; event_options(g, 1); return; }
        ++cost;
        SPIRE_ASSERT(s.count < 16);
        const int index = s.count++;
        s.choices |= static_cast<uint32_t>(option) << (2 * index);
        if (option == 0 && !has_relic(g, RelicId::Sozu)) {
            const PotionId potion = random_potion_from_pool(g, g.rng.potion);
            s.grants[index] = static_cast<uint16_t>(potion);
            obtain_potion(g, potion, construct_potion(g, potion));
        } else if (option == 1) { gain_gold(g, 90); s.goldEarned += 90; }
        else if (option == 2) {
            const CardId card = shuffled_colorless_card(g, CardRarity::Uncommon);
            s.grants[index] = static_cast<uint16_t>(card);
            queue_card_obtain(g, make_card(card));
        }
        event_options(g, 4);
    }
};
}
