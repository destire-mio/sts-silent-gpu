#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Designer : EventHooks {
    SPIRE_STATE(Designer, uint8_t curScreen; uint8_t option; bool adjustmentUpgradesOne; bool cleanUpRemovesCards;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Designer", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return (g.run.act == 2 || g.run.act == 3) && g.run.gold >= 75; }
    SPIRE_SNAPSHOT("Designer.curScreen", scalar, event.state.Designer.curScreen == 0 ? "INTRO" : event.state.Designer.curScreen == 1 ? "MAIN" : "DONE")
    SPIRE_SNAPSHOT("Designer.option", scalar, event.state.Designer.option == 0 ? "UPGRADE" : event.state.Designer.option == 1 ? "REMOVE" : event.state.Designer.option == 2 ? "REMOVE_AND_UPGRADE" : event.state.Designer.option == 3 ? "TRANSFORM" : "NONE")
    SPIRE_SNAPSHOT("Designer.adjustCost", scalar, 50)
    SPIRE_SNAPSHOT("Designer.cleanUpCost", scalar, 75)
    SPIRE_SNAPSHOT("Designer.fullServiceCost", scalar, 110)
    SPIRE_SNAPSHOT("Designer.hpLoss", scalar, 5)
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.Designer;
        s.option = 4;
        s.adjustmentUpgradesOne = g.rng.misc.randomBoolean();
        s.cleanUpRemovesCards = g.rng.misc.randomBoolean();
        event_options(g, 1);
    }
    static SPIRE_FN void upgrade_random(Game& g, int amount) {
        Pile cards{};
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (card_canUpgrade(g, -i - 2)) cards.push_back(i);
        shuffle(cards.values, cards.size, g.rng.misc);
        for (int i = 0; i < min(amount, cards.size); ++i) card_upgrade(g.run.master_deck[cards[i]]);
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Designer;
        if (s.curScreen == 0) {
            bool upgradable = false;
            int unbottled = 0;
            for (int i = 0; i < g.run.master_deck.size; ++i) {
                const Card& card = g.run.master_deck[i];
                upgradable |= card_canUpgrade(g, -i - 2);
                unbottled += !card.bottle_flame && !card.bottle_lightning && !card.bottle_tornado;
            }
            event_options(g, 4);
            g.image_dialog[0] = g.run.gold < 50 || !upgradable;
            g.image_dialog[1] = g.run.gold < 75 || unbottled < (s.cleanUpRemovesCards ? 1 : 2);
            g.image_dialog[2] = g.run.gold < 110 || unbottled == 0;
            s.curScreen = 1;
        } else if (s.curScreen == 1) {
            if (option == 0) {
                lose_gold(g, 50);
                if (s.adjustmentUpgradesOne) {
                    s.option = 0; g.grid_cards.clear();
                    for (int i = 0; i < g.run.master_deck.size; ++i)
                        if (card_canUpgrade(g, -i - 2)) g.grid_cards.push_back(i);
                    open_master_selection(g, 1, false); g.grid_for_upgrade = true;
                } else upgrade_random(g, 2);
            } else if (option == 1 || option == 2) {
                lose_gold(g, option == 1 ? 75 : 110);
                s.option = option == 2 ? 2 : s.cleanUpRemovesCards ? 1 : 3;
                event_purge_cards(g);
                open_master_selection(g, s.option == 3 ? 2 : 1, s.option != 3);
            } else {
                DamageInfo info{}; info.source = no_target; info.target = player_target;
                info.amount = 5; info.damage_type = DamageType::HpLoss; player_damage(g, info);
            }
            s.curScreen = 2; event_options(g, 1);
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.Designer;
        if (g.selected_cards.empty() || s.option == 4) return;
        if (s.option == 0) card_upgrade(g.run.master_deck[g.selected_cards[0]]);
        else if (s.option == 1 || s.option == 2) {
            remove_master_card(g, g.selected_cards[0]);
            if (s.option == 2) upgrade_random(g, 1);
        } else {
            const int first = g.selected_cards[0];
            const Card card = g.run.master_deck[first];
            remove_master_card(g, first);
            queue_card_obtain(g, transform_card(g, card, false, g.rng.misc));
            if (g.selected_cards.size == 2) {
                const int second = g.selected_cards[1] - (g.selected_cards[1] > first);
                const Card other = g.run.master_deck[second];
                remove_master_card(g, second);
                queue_card_obtain(g, transform_card(g, other, false, g.rng.misc));
            }
        }
        g.selected_cards.clear(); s.option = 4;
    }
};
}
