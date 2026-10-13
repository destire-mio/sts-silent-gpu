#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct FountainOfCurseRemoval : EventHooks {
    SPIRE_WAITING_RULE("Event choices")
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.FountainOfCurseRemoval", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) {
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const auto id = g.run.master_deck[i].id;
            if (card_def(id).type == CardType::Curse && id != CardId::Necronomicurse && id != CardId::CurseOfTheBell && id != CardId::AscendersBane) return true;
        }
        return false;
    }
};
}
