#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Falling : EventHooks {
    SPIRE_STATE(Falling, uint16_t attackCard; uint16_t skillCard; uint16_t powerCard; uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.beyond.Falling", true}; }
    SPIRE_SNAPSHOT("Falling.screen", scalar, event.state.Falling.screen == 0 ? "INTRO" : event.state.Falling.screen == 1 ? "CHOICE" : "RESULT")
    SPIRE_SNAPSHOT("Falling.attack", scalar, event.state.Falling.attackCard != 0)
    SPIRE_SNAPSHOT("Falling.skill", scalar, event.state.Falling.skillCard != 0)
    SPIRE_SNAPSHOT("Falling.power", scalar, event.state.Falling.powerCard != 0)
    SPIRE_SNAPSHOT("Falling.attackCard", master_card_uuid, event.state.Falling.attackCard)
    SPIRE_SNAPSHOT("Falling.skillCard", master_card_uuid, event.state.Falling.skillCard)
    SPIRE_SNAPSHOT("Falling.powerCard", master_card_uuid, event.state.Falling.powerCard)
    static SPIRE_FN uint16_t pick(Game& g, CardType type) {
        List<uint8_t, master_deck_capacity> cards{};
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const Card& c = g.run.master_deck[i];
            if (card_def(c.id).type == type && !c.bottle_flame && !c.bottle_lightning && !c.bottle_tornado) cards.push_back(i);
        }
        return cards.empty() ? 0 : g.run.master_deck[cards[g.rng.misc.random(cards.size - 1)]].uuid;
    }
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.Falling;
        s.attackCard = pick(g, CardType::Attack); s.skillCard = pick(g, CardType::Skill); s.powerCard = pick(g, CardType::Power);
        event_options(g, 1);
    }
    static SPIRE_FN bool referencesMasterCard(const Game& g, uint32_t uuid) {
        const auto& s = g.event.state.Falling;
        return s.attackCard == uuid || s.skillCard == uuid || s.powerCard == uuid;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Falling;
        if (s.screen == 0) {
            s.screen = 1;
            if (s.attackCard || s.skillCard || s.powerCard) {
                event_options(g, 3);
                g.image_dialog[0] = !s.skillCard; g.image_dialog[1] = !s.powerCard; g.image_dialog[2] = !s.attackCard;
            } else event_options(g, 1);
        } else if (s.screen == 1) {
            if (s.attackCard || s.skillCard || s.powerCard) {
                const uint16_t uuid = option == 0 ? s.skillCard : option == 1 ? s.powerCard : s.attackCard;
                for (int i = 0; i < g.run.master_deck.size; ++i)
                    if (g.run.master_deck[i].uuid == uuid) { remove_master_card(g, i); break; }
            }
            s.screen = 2; event_options(g, 1);
        } else open_map(g);
    }
};
}
