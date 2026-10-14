#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct FountainOfCurseRemoval : EventHooks {
    SPIRE_STATE(FountainOfCurseRemoval, int screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.FountainOfCurseRemoval", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) {
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const auto id = g.run.master_deck[i].id;
            if (card_def(id).type == CardType::Curse && id != CardId::Necronomicurse && id != CardId::CurseOfTheBell && id != CardId::AscendersBane) return true;
        }
        return false;
    }
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.FountainOfCurseRemoval;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0) for (int i = g.run.master_deck.size - 1; i >= 0; --i) {
            const Card& card = g.run.master_deck[i];
            if (card_def(card.id).type == CardType::Curse && !card.bottle_flame && !card.bottle_lightning &&
                card.id != CardId::AscendersBane && card.id != CardId::CurseOfTheBell && card.id != CardId::Necronomicurse)
                remove_master_card(g, i);
        }
        s.screenNum = 1; event_options(g, 1);
    }
};
}
