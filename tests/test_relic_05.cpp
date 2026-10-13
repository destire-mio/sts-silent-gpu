#include "core/engine.hpp"
#include <cstdlib>
#include <iostream>
using namespace spire;

static void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}

static CombatSetup setup() {
    CombatSetup s{};
    s.hp = 59;
    s.max_hp = 66;
    s.deck.push_back(make_card(CardId::Strike_G));
    s.deck.push_back(make_card(CardId::Defend_G));
    return s;
}

int main() {
    Game g{};
    reset(g, 101);
    CombatSetup s = setup();
    start_combat(g, s);
    equip_relic(g, RelicId::RunicDome);
    equip_relic(g, RelicId::Sozu);
    equip_relic(g, RelicId::SneckoEye);
    require(g.player.energy_master == 5 && g.player.master_hand_size == 7, "equip updates persistent energy and hand size");
    unequip_relic(g, RelicId::RunicDome);
    unequip_relic(g, RelicId::Sozu);
    unequip_relic(g, RelicId::SneckoEye);
    require(g.player.energy_master == 3 && g.player.master_hand_size == 5, "unequip reverses persistent modifiers");

    equip_relic(g, RelicId::SacredBark);
    g.run.potions[0] = PotionId::StrengthPotion;
    g.run.potion_state[0] = construct_potion(g, PotionId::StrengthPotion);
    require(potion_potency(g, 0) == 4, "new potion uses bark potency");
    unequip_relic(g, RelicId::SacredBark);
    g.run.potion_state[0] = construct_potion(g, PotionId::StrengthPotion);
    require(potion_potency(g, 0) == 2, "base potency before bark acquisition");
    equip_relic(g, RelicId::SacredBark);
    require(potion_potency(g, 0) == 4, "bark refreshes an existing potion");

    equip_relic(g, RelicId::Strawberry);
    require(g.player.creature.max_hp == 73 && g.player.creature.hp == 66, "strawberry increases health and heals");
    equip_relic(g, RelicId::PrismaticShard);
    require(g.player.master_max_orbs == 1, "shard grants a master slot");
    g.player.master_max_orbs = 2;
    units::relics::PrismaticShard::onEquip(g, 0);
    require(g.player.master_max_orbs == 2, "shard preserves existing slots");

    equip_relic(g, RelicId::SsserpentHead);
    const int head = g.run.relics.size - 1;
    const int gold = g.run.gold;
    relic_onEnterRoom(g, head, static_cast<int>(RoomKind::Shop));
    require(g.run.gold == gold, "shop entry does not grant event gold");
    relic_onEnterRoom(g, head, static_cast<int>(RoomKind::Event));
    require(g.run.gold == gold + 50, "event entry grants serpent gold");
    equip_relic(g, RelicId::SmilingMask);
    const int mask = g.run.relics.size - 1;
    relic_onEnterRoom(g, mask, static_cast<int>(RoomKind::Shop));
    require(g.run.relics[mask].state.SmilingMask.pulse, "mask pulses in a shop");
    relic_onEnterRoom(g, mask, static_cast<int>(RoomKind::Event));
    require(!g.run.relics[mask].state.SmilingMask.pulse, "mask stops pulsing after a shop");
    g.run.floor = 48;
    g.room_kind = RoomKind::Shop;
    require(!relic_canSpawn(g, mask), "mask cannot spawn in a shop");
    g.room_kind = RoomKind::Monster;
    require(relic_canSpawn(g, mask), "mask can spawn at floor 48 outside a shop");
    g.run.floor = 49;
    require(!relic_canSpawn(g, mask), "mask cannot spawn after floor 48");

    equip_relic(g, RelicId::SlaversCollar);
    const int collar = g.run.relics.size - 1;
    relic_beforeEnergyPrep(g, collar);
    require(g.player.energy_master == 3, "ordinary room does not activate collar");
    g.room_kind = RoomKind::Elite;
    g.room_elite_trigger = true;
    relic_beforeEnergyPrep(g, collar);
    require(g.player.energy_master == 4, "elite room activates collar");
    relic_onVictory(g, collar);
    require(g.player.energy_master == 3, "collar releases its battle energy");
    equip_relic(g, RelicId::Sling);
    relic_atBattleStart(g, g.run.relics.size - 1);
    run_until_decision(g);
    require(power_amount(g, g.player.creature, PowerId::Strength) == 2, "elite sling applies strength through the queue");

    Game h{};
    reset(h, 101);
    start_combat(h, s);
    equip_relic(h, RelicId::Sozu);
    const PotionState potion = construct_potion(h, PotionId::StrengthPotion);
    add_bottom(h, units::actions::ObtainPotion::from_potion(PotionId::StrengthPotion, potion));
    run_until_decision(h);
    require(h.run.potions[0] == PotionId::Empty, "sozu prevents the obtain action");
    obtain_potion(h, PotionId::StrengthPotion, potion);
    require(h.run.potions[0] == PotionId::StrengthPotion, "raw player potion acquisition has no sozu check");
}
