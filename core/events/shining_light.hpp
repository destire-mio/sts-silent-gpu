#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct ShiningLight : EventHooks {
    SPIRE_STATE(ShiningLight, int damage; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.ShiningLight", true}; }
    SPIRE_SNAPSHOT("ShiningLight.screen", scalar, event.state.ShiningLight.screen ? "COMPLETE" : "INTRO")
    static SPIRE_FN void enter(Game& g) {
        g.event.state.ShiningLight.damage = floor_int(g.player.creature.max_hp * 0.3f + 0.5f);
        bool upgradable = false;
        for (int i = 0; i < g.run.master_deck.size; ++i) upgradable |= card_canUpgrade(g, -i - 2);
        event_options(g, 2); g.image_dialog[0] = !upgradable;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.ShiningLight;
        if (s.screen) { open_map(g); return; }
        s.screen = 1; event_options(g, 1);
        if (option == 0) {
            event_damage(g, s.damage, player_target);
            List<uint8_t, master_deck_capacity> cards{};
            for (int i = 0; i < g.run.master_deck.size; ++i)
                if (card_canUpgrade(g, -i - 2)) cards.push_back(i);
            shuffle(cards.values, cards.size, g.rng.misc);
            for (int i = 0; i < min(2, cards.size); ++i) card_upgrade(g.run.master_deck[cards[i]]);
        }
    }
};
}
