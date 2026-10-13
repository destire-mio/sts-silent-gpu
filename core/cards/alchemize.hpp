#pragma once
#include "../hooks.hpp"
#include "../actions/obtain_potion.hpp"

namespace spire::units::cards {
struct Alchemize : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        constexpr const char* pool[] = {
            "Poison Potion",
            "CunningPotion",
            "GhostInAJar",
            "Block Potion",
            "Dexterity Potion",
            "Energy Potion",
            "Explosive Potion",
            "Fire Potion",
            "Strength Potion",
            "Swift Potion",
            "Weak Potion",
            "FearPotion",
            "AttackPotion",
            "SkillPotion",
            "PowerPotion",
            "ColorlessPotion",
            "SteroidPotion",
            "SpeedPotion",
            "BlessingOfTheForge",
            "Regen Potion",
            "Ancient Potion",
            "LiquidBronze",
            "GamblersBrew",
            "EssenceOfSteel",
            "DuplicationPotion",
            "DistilledChaos",
            "LiquidMemories",
            "CultistPotion",
            "Fruit Juice",
            "SneckoOil",
            "FairyPotion",
            "SmokeBomb",
            "EntropicBrew"};
        const int roll = g.rng.potion.random(0, 99);
        const int rarity = roll < 65 ? 0 : roll < 90 ? 1 : 2;
        const char* id = pool[g.rng.potion.random(32)];
        bool spam_check = true;
        // AbstractDungeon.returnRandomPotion always discards the first draw when limited.
        while (potion_rarity(id) != rarity || spam_check) {
            id = pool[g.rng.potion.random(32)];
            spam_check = same_text(id, "Fruit Juice");
        }
        const PotionId potion = potion_id(id);
        add_bottom(g, actions::ObtainPotion::from_potion(potion, {}));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            const int diff = card.cost_for_turn - card.cost;
            card.cost = 0;
            if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + diff;
            if (card.cost_for_turn < 0) card.cost_for_turn = 0;
            card.upgraded_cost = true;
        }
    }
};
}
