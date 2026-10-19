#pragma once
#include "obs/schema.hpp"
#include "obs/card_variables.hpp"

namespace spire::obs {
struct History {
    Token card{};
    bool shown = false;
    int adventurer_floor = -1, adventurer_clue = 0;
    int rest_floor = -1, rest_heal = 0, rest_bonus = 0;
};
SPIRE_FN inline Token card_token(const Game& g, const Card& c, Zone zone, int position, bool live) {
    Token t = token(Kind::Card, static_cast<int>(c.id) + 1, zone, position);
    const auto& d = card_data(c.id);
    const int variables = card_variables[static_cast<int>(c.id)][c.upgrades != 0];
    t.values[0] = c.upgrades;
    const bool cost_shown = c.cost > -2 && (d.type != CardType::Status || c.id == CardId::Slimed) &&
        d.type != CardType::Curse;
    t.values[1] = !cost_shown ? -2 : c.cost == -1 ? -1 :
        (c.free_to_play_once || (g.room_phase == RoomPhase::Combat && d.type == CardType::Attack &&
         has_power(g, g.player.creature, PowerId::FreeAttack))) ? 0 : c.cost_for_turn;
    t.values[2] = variables;
    // Soul.update may clear GlassKnife damage between duplicate ModifyDamageAction.update calls.
    const bool discard = zone == Zone::Discard || (zone == Zone::Grid && g.grid_source == PileId::Discard);
    if (variables & 1) t.values[3] = live && !discard ? c.damage : c.base_damage;
    if (variables & 2) t.values[4] = live ? c.block : card_baseBlock(c);
    if (variables & 4) t.values[5] = live ? c.magic : c.base_magic;
    // MasterDeckViewScreen.render and noncombat GridCardSelectScreen.render show bottle badges.
    if (zone == Zone::Master || (zone == Zone::Grid && g.room_phase != RoomPhase::Combat)) {
        t.values[6] = c.bottle_flame; t.values[7] = c.bottle_lightning; t.values[8] = c.bottle_tornado;
    }
    return t;
}
SPIRE_FN inline void record_event(History& history, const Game& g) {
    if (g.screen != Screen::Event || g.event.id != EventId::DeadAdventurer ||
        g.event.state.DeadAdventurer.screen != 0 || history.adventurer_floor != g.run.floor) {
        history.adventurer_floor = -1;
        history.adventurer_clue = 0;
    }
    if (g.screen == Screen::Event && g.event.id == EventId::DeadAdventurer &&
        g.event.state.DeadAdventurer.screen == 0 && g.event.state.DeadAdventurer.numRewards == 0) {
        // DeadAdventurer's constructor displays the enemy clue before randomReward replaces it.
        history.adventurer_floor = g.run.floor;
        history.adventurer_clue = g.event.state.DeadAdventurer.enemy + 1;
    }
}
SPIRE_FN inline void record_action(History& history, const Game& g, Action action) {
    record_event(history, g);
    history.shown = false;
    if (g.screen != Screen::Event || g.event.id != EventId::GremlinMatchGame ||
        g.event.state.GremlinMatchGame.screen != 2 || action.kind != ActionKind::Choose) return;
    const auto& s = g.event.state.GremlinMatchGame;
    int choice = 0;
    for (int i = 0; i < s.count; ++i) {
        const Card& card = g.cards[s.cards[i]];
        if (!card.is_flipped) continue;
        if (choice++ != action.index) continue;
        // GremlinMatchGame.updateMatchGameLogic reveals the clicked face before hiding a mismatch.
        history.card = card_token(g, card, Zone::History, i + 1, false);
        history.shown = true;
        break;
    }
}
SPIRE_FN inline void pile(Writer& w, const Game& g, const Pile& cards, Zone zone, bool unordered, bool copy) {
    uint8_t sorted[card_capacity];
    for (int i = 0; i < cards.size; ++i) {
        const int index = !unordered && (zone == Zone::Draw || zone == Zone::Exhaust) ? cards.size - i - 1 : i;
        const Card& c = g.cards[cards[index]];
        Token t = card_token(g, c, zone, unordered ? 0 : i + 1, !copy);
        if (unordered) {
            int at = i;
            while (at > 0) {
                const auto& previous = g.cards[sorted[at - 1]];
                if (c.id > previous.id || (c.id == previous.id &&
                    !less(t, card_token(g, previous, zone, 0, !copy)))) break;
                sorted[at] = sorted[at - 1]; --at;
            }
            sorted[at] = cards[index];
        } else w.add(t);
    }
    if (unordered) for (int i = 0; i < cards.size; ++i)
        w.add(card_token(g, g.cards[sorted[i]], zone, 0, !copy));
}
SPIRE_FN inline void powers(Writer& w, const Game& g, const Creature& c, int owner) {
    int position = 0;
    for (int p = c.powers.head; p != 255; p = g.power_objects[p].next) {
        const Power& power = g.power_objects[p];
        Token t = token(Kind::Power, static_cast<int>(power.id) + 1, Zone::None, ++position, owner);
        const bool shown = power.amount > 0 || (power.amount < 0 && power_data(power.id).can_negative);
        t.values[0] = shown; t.values[1] = shown ? power.amount : 0;
        switch (power.id) {
        case PowerId::TheBomb: t.values[2] = power.state.TheBomb.damage; break;
        case PowerId::Panache: t.values[2] = power.state.Panache.damage; break;
        case PowerId::Combust: t.values[2] = power.state.Combust.hpLoss; break;
        case PowerId::Deva: t.values[2] = power.state.Deva.energyGainAmount; break;
        case PowerId::Malleable: t.values[2] = power.state.Malleable.basePower; break;
        case PowerId::Stasis: case PowerId::Nightmare: {
            const auto& card = get_card(g, power.id == PowerId::Stasis ? power.state.Stasis.card : power.state.Nightmare.card);
            t.ids[5] = static_cast<int>(card.id) + 1; t.values[3] = card.upgrades;
            break;
        }
        default: break;
        }
        w.add(t);
    }
}
SPIRE_FN inline Token relic_token(const Relic& relic, Zone zone, int position, bool owned) {
    Token t = token(Kind::Relic, static_cast<int>(relic.id) + 1, zone, position);
    if (owned) {
        t.values[0] = relic.counter >= 0;
        t.values[1] = relic.counter >= 0 ? relic.counter : 0;
        t.values[2] = relic.grayscale;
        t.values[3] = relic.used_up;
        switch (relic.id) {
        case RelicId::FossilizedHelix: t.values[2] = relic.state.FossilizedHelix.grayscale; break;
        case RelicId::StoneCalendar: t.values[2] = relic.state.StoneCalendar.grayscale; break;
        case RelicId::HornCleat: t.values[2] = relic.state.HornCleat.grayscale; break;
        case RelicId::LizardTail: t.values[2] = t.values[3] = relic.state.LizardTail.usedUp; break;
        case RelicId::Matryoshka: t.values[2] = t.values[3] = relic.state.Matryoshka.usedUp; break;
        case RelicId::MawBank: t.values[2] = t.values[3] = relic.state.MawBank.usedUp; break;
        default: break;
        }
    }
    return t;
}
SPIRE_FN inline Token option(Control control, int position, int variant = 0) {
    Token t = token(Kind::Option, static_cast<int>(control), Zone::Offer, position);
    t.ids[5] = variant;
    return t;
}
SPIRE_FN inline int master_offer(Writer& w, const Game& g, uint16_t uuid, int owner) {
    if (!uuid) return 0;
    const Card& card = master_card_by_uuid(g, uuid);
    Token t = card_token(g, card, Zone::Event, 0, false);
    t.ids[3] = owner;
    return w.add(t);
}
}
