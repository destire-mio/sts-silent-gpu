#pragma once
#include "api.hpp"

namespace spire {
struct CardReferences {
    uint64_t words[(card_capacity + 63) / 64]{};
    SPIRE_FN void add(int index) {
        if (index < 0) return;
        SPIRE_ASSERT(index < card_capacity);
        words[index / 64] |= uint64_t{1} << (index % 64);
    }
    template<class Indices> SPIRE_FN void add(const Indices& indices) {
        for (int i = 0; i < indices.size; ++i) add(static_cast<int>(indices[i]));
    }
    SPIRE_FN void power(const Power& value) {
        if (value.id == PowerId::Nightmare) add(value.state.Nightmare.card);
        if (value.id == PowerId::Stasis) add(value.state.Stasis.card);
    }
};

SPIRE_FN inline CardReferences card_references(const Game& g) {
    CardReferences refs{};
    refs.add(g.hand); refs.add(g.draw); refs.add(g.discard); refs.add(g.exhaust); refs.add(g.limbo);
    refs.add(g.drawn_cards); refs.add(g.pending_obtains);
    refs.add(g.detached_master_cards); refs.add(g.reward_cards);
    refs.add(g.player.card_in_use); refs.add(g.discovery_card);
    for (int i = 0; i < g.manager.played_cards.size; ++i) refs.add(static_cast<int>(g.manager.played_cards.values[i]));
    for (int i = 0; i < g.manager.card_queue.size; ++i) refs.add(static_cast<int>(g.manager.card_queue[i].card));
    for (int i = 0; i < (card_capacity + 63) / 64; ++i) refs.words[i] |= g.pending_card_moves[i];
    if (g.screen == Screen::Grid && g.grid_source == PileId::Grid) refs.add(g.grid_cards);
    if (g.screen == Screen::Discard || (g.screen == Screen::Grid && g.grid_source != PileId::MasterDeck))
        refs.add(g.selected_cards);
    for (int i = 0; i < g.reward_objects.size; ++i) refs.add(g.reward_objects[i].cards);
    if (g.room_kind == RoomKind::Shop) { refs.add(g.shop.colored); refs.add(g.shop.colorless); }
    const Powers& player_powers = g.player.creature.powers;
    for (int i = 0; i < player_powers.size; ++i) refs.power(player_powers.at(g.power_objects, i));
    for (int m = 0; m < g.monster_order.size; ++m) {
        const Powers& powers = g.monsters[g.monster_order[m]].creature.powers;
        for (int i = 0; i < powers.size; ++i) refs.power(powers.at(g.power_objects, i));
    }
    for (int word = 0; word < (action_capacity + 63) / 64; ++word) {
        uint64_t used = g.action_objects.used[word];
        while (used) {
            const int i = word * 64 + __builtin_ctzll(used);
            used &= used - 1;
            const Effect& e = g.action_objects[i];
            // SetMoveAction.update reads damage here; the other users store card references.
            if (e.kind != EffectKind::SetMove) refs.add(static_cast<int>(e.card));
            if (e.kind == EffectKind::ApplyPower && e.power_constructed) refs.power(e.payload.power);
            if ((e.kind == EffectKind::RemovePower || e.kind == EffectKind::ReducePower || e.kind == EffectKind::MayhemPlay) && e.extra != 0)
                refs.power(power_instance(g, e.extra));
            switch (e.kind) {
            case EffectKind::Armaments:
                for (int j = 0; j < e.payload.state.Armaments.cannot_upgrade_count; ++j)
                    refs.add(static_cast<int>(e.payload.state.Armaments.cannot_upgrade[j]));
                break;
            case EffectKind::DualWield:
                for (int j = 0; j < e.payload.state.DualWield.count; ++j)
                    refs.add(static_cast<int>(e.payload.state.DualWield.cannotDuplicate[j]));
                break;
            case EffectKind::ChooseOne:
                for (int j = 0; j < e.payload.state.ChooseOne.count; ++j)
                    refs.add(static_cast<int>(e.payload.state.ChooseOne.choices[j]));
                break;
            case EffectKind::ApplyStasisAction: refs.add(e.payload.state.ApplyStasisAction.card); break;
            case EffectKind::Exhume:
                if (e.payload.state.Exhume.selecting) refs.add(g.selection_excluded);
                break;
            case EffectKind::SpawnMonster: {
                const Powers& powers = g.monsters[e.payload.state.SpawnMonster.m].creature.powers;
                for (int p = 0; p < powers.size; ++p) refs.power(powers.at(g.power_objects, p));
                break;
            }
            default: break;
            }
        }
    }
    switch (g.event.id) {
    case EventId::Bonfire: refs.add(static_cast<int>(g.event.state.Bonfire.offeredCard) - 1); break;
    case EventId::Sssserpent: refs.add(static_cast<int>(g.event.state.Sssserpent.curse)); break;
    case EventId::GremlinMatchGame: {
        const auto& s = g.event.state.GremlinMatchGame;
        refs.add(static_cast<int>(s.chosen));
        for (int i = 0; i < s.count; ++i) refs.add(static_cast<int>(s.cards[i]));
        for (int i = 0; i < s.cardsMatched; ++i) refs.add(static_cast<int>(s.matched[i]));
        break;
    }
    default: break;
    }
    return refs;
}

SPIRE_FN inline void collect_cards(Game& g) {
    for (int i = g.detached_master_cards.size - 1; i >= 0; --i) {
        const uint32_t uuid = g.cards[g.detached_master_cards[i]].uuid;
        bool referenced = g.event.id != EventId::Count && event_referencesMasterCard(g, uuid);
        for (int r = 0; r < g.run.relics.size; ++r) referenced |= relic_referencesMasterCard(g, r, uuid);
        if (!referenced) g.detached_master_cards.erase(i);
    }
    const CardReferences refs = card_references(g);
    for (int word = 0; word < (card_capacity + 63) / 64; ++word) {
        SPIRE_ASSERT((refs.words[word] & ~g.cards.used[word]) == 0);
        uint64_t garbage = g.cards.used[word] & ~refs.words[word];
        while (garbage) {
            const int index = word * 64 + __builtin_ctzll(garbage);
            g.damage_exceptions.release(g.multi_damage.head[index]);
            g.multi_damage.head[index] = 0;
            g.cards.release(index);
            garbage &= garbage - 1;
        }
    }
}
}
