#include "core/engine.hpp"
#include <cstdlib>
#include <iostream>
using namespace spire;

static void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}

static void appearance() {
    Game g{};
    reset(g, 101);
    g.run.act = 1; g.run.gold = 200;
    require(!event_canSpawn(g, EventId::Designer), "Designer excludes Act 1");
    g.run.act = 2; g.run.gold = 74;
    require(!event_canSpawn(g, EventId::Designer), "Designer excludes gold below 75");
    g.run.gold = 75;
    require(event_canSpawn(g, EventId::Designer), "Designer admits Act 2 at 75 gold");
    g.run.act = 3;
    require(event_canSpawn(g, EventId::Designer), "Designer admits Act 3");
    g.run.act = 4;
    require(!event_canSpawn(g, EventId::Designer), "Designer excludes the Ending");
    g.run.relics.clear(); equip_relic(g, RelicId::SnakeRing);
    g.run.act = 2;
    require(!event_canSpawn(g, EventId::Nloth), "Nloth needs two relics");
    equip_relic(g, RelicId::Anchor);
    require(event_canSpawn(g, EventId::Nloth), "Nloth admits two relics in Act 2");
    g.run.act = 3;
    require(!event_canSpawn(g, EventId::Nloth), "Nloth's Java filter repeats TheCity and excludes Act 3");
    g.run.master_deck.clear();
    for (CardId id : {CardId::AscendersBane, CardId::CurseOfTheBell, CardId::Necronomicurse}) {
        g.run.master_deck.push_back(make_card(id));
        require(!event_canSpawn(g, EventId::FountainOfCurseRemoval), "protected curses do not qualify for the fountain");
    }
    g.run.master_deck.push_back(make_card(CardId::Regret));
    require(event_canSpawn(g, EventId::FountainOfCurseRemoval), "a removable curse qualifies for the fountain");
    for (EventId id : {EventId::GoldShrine, EventId::Lab, EventId::UpgradeShrine, EventId::WeMeetAgain})
        require(event_canSpawn(g, id), "unconditional shrines and one-time events remain eligible");
}

static void references() {
    Game g{};
    reset(g, 101);
    g.run.relics.clear();
    equip_relic(g, RelicId::RunicDome); equip_relic(g, RelicId::Girya);
    g.run.relics[1].counter = 3;
    g.event.id = EventId::Nloth; event_initialize_state(g.event); event_enter(g);
    const auto& event = g.event.state.Nloth;
    const bool first = g.reward_objects[event.choice1].relic.id == RelicId::RunicDome;
    const int dome = first ? event.choice1 : event.choice2, girya = first ? event.choice2 : event.choice1;
    require(g.player.energy_master == 4, "Dome contributes energy before the trade");
    event_choose(g, first ? 0 : 1);
    require(g.player.energy_master == 3 && g.reward_objects[dome].relic_slot == -1,
            "removing a retained relic applies unequip and keeps its original object");
    require(g.reward_objects[girya].relic_slot == 0 && g.run.relics[0].counter == 3,
            "another retained relic follows the surviving object after the list shifts");
    equip_relic(g, RelicId::Circlet); equip_relic(g, RelicId::Circlet);
    require(g.run.relics.size == 3 && g.run.relics[2].counter == 2,
            "Circlet acquisition increments the existing relic without adding an object");
    RoomReward reward{}; reward.kind = RewardKind::Relic; reward.relic.id = RelicId::Circlet;
    const int duplicate = create_reward(g, reward);
    obtain_relic_object(g, duplicate);
    require(g.run.relics.size == 3 && g.run.relics[2].counter == 3 && g.reward_objects[duplicate].relic_slot == -1,
            "an obtained duplicate Circlet keeps the new object separate from the existing relic");
    Game other{}; reset(other, 101);
    require(other.player.energy_master == 3 && other.reward_objects.empty(), "another game owns independent relic references");
}

int main() { appearance(); references(); }
