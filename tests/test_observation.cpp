#include "obs/encode.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

using namespace spire;
static int pairs[13]{};
static int positive_screens[13]{};
static int positives = 0;

static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static obs::Observation observe_recorded(const Game& g, obs::History& reveal) {
    const Game before = g;
    obs::Observation o; ActionSet actions;
    const auto result = obs::encode(g, reveal, o.view(), actions);
    require(!result.fault, "observation fault");
    require(std::memcmp(&before, &g, sizeof(Game)) == 0, "encoder mutated Game");
    return o;
}
static obs::Observation observe(const Game& g, obs::History reveal = {}) {
    return observe_recorded(g, reveal);
}
static void equal(const Game& a, const Game& b) {
    const auto x = observe(a), y = observe(b);
    require(std::memcmp(&x, &y, sizeof(x)) == 0, "hidden information changed observation/candidates/masks/padding");
    ++pairs[static_cast<int>(a.screen)];
}
static void different(const Game& a, const Game& b) {
    const auto x = observe(a), y = observe(b);
    if (std::memcmp(&x, &y, sizeof(x)) == 0) std::fprintf(stderr, "positive=%d screen=%d event=%d\n", positives, static_cast<int>(a.screen), static_cast<int>(a.event.id));
    require(std::memcmp(&x, &y, sizeof(x)) != 0, "visible positive control had no effect");
    ++positives;
    ++positive_screens[static_cast<int>(a.screen)];
}
static void hidden_rng(const Game& g) {
    Game h = g;
    h.run.seed += 911;
    h.rng.generate_seeds(h.run.seed);
    h.rng.map.reset(h.run.seed); h.rng.neow.reset(h.run.seed);
    h.unseeded = UnseededRandom(h.run.seed);
    if (h.run.event_list.size > 1) {
        auto temp = h.run.event_list[0]; h.run.event_list[0] = h.run.event_list[1]; h.run.event_list[1] = temp;
    }
    equal(g, h);
}
static Game combat(bool dome = false) {
    Game g; reset(g, 123);
    CombatSetup setup; setup.encounter = EncounterId::JawWorm;
    setup.deck = g.run.master_deck; setup.relics = g.run.relics;
    if (dome) setup.relics.push_back({RelicId::RunicDome});
    start_combat(g, setup);
    return g;
}
static void draw_and_identity() {
    Game a = combat(), b = a;
    int other = 1;
    while (other < b.draw.size && b.cards[b.draw[0]].id == b.cards[b.draw[other]].id) ++other;
    require(other < b.draw.size, "draw permutation needs distinct visible cards");
    auto temp = b.draw[0]; b.draw[0] = b.draw[other]; b.draw[other] = temp;
    equal(a, b);
    equip_relic(a, RelicId::FrozenEye); equip_relic(b, RelicId::FrozenEye);
    different(a, b);
    a = combat(); b = a;
    for (int i = 0; i < b.cards.size; ++i) if (b.cards.contains(i)) b.cards[i].uuid += 500;
    for (int i = 0; i < b.run.master_deck.size; ++i) b.run.master_deck[i].uuid += 500;
    b.next_card_uuid += 500;
    equal(a, b);
    b = a; b.cards[b.hand[0]] = make_card(CardId::BladeDance);
    different(a, b);
}
static void pile_sorting_matches_tokens() {
    Game g; reset(g, 123);
    g.cards.clear(); g.grid_source = PileId::Discard;
    const CardId kinds[]{CardId::GlassKnife, CardId::BladeDance, CardId::Survivor, CardId::Slimed, CardId::Regret};
    for (int i = 0; i < card_capacity; ++i) {
        auto c = make_card(kinds[(i / 2) % 5]);
        c.upgrades = (i / 13) % 2; c.cost_for_turn = (i / 3) % 4 - 1;
        c.damage += i % 7; c.base_damage += i % 3; c.block += i % 5;
        c.magic += i % 2; c.base_magic += i % 3;
        c.free_to_play_once = i % 17 == 0;
        c.bottle_flame = i % 19 == 0; c.bottle_lightning = i % 23 == 0; c.bottle_tornado = i % 29 == 0;
        g.cards.add(c);
    }
    for (auto phase : {RoomPhase::Combat, RoomPhase::Event}) {
        g.room_phase = phase;
        for (int count : {0, 1, 64, card_capacity}) for (int rotation : {0, 1, 73}) {
            Pile cards;
            for (int i = 0; i < count; ++i) cards.push_back((i * 101 + rotation) % card_capacity);
            for (auto zone : {obs::Zone::Master, obs::Zone::Hand, obs::Zone::Draw, obs::Zone::Discard,
                              obs::Zone::Exhaust, obs::Zone::Grid}) for (bool unordered : {false, true}) for (bool copy : {false, true}) {
                const Game before = g;
                std::vector<obs::Token> tokens;
                for (int i = 0; i < count; ++i) {
                    const int index = !unordered && (zone == obs::Zone::Draw || zone == obs::Zone::Exhaust) ? count - i - 1 : i;
                    tokens.push_back(obs::card_token(g, g.cards[cards[index]], zone, unordered ? 0 : i + 1, !copy));
                }
                if (unordered) std::stable_sort(tokens.begin(), tokens.end(), obs::less);
                obs::Observation actual, expected;
                obs::Writer left{actual.view()}, right{expected.view()};
                obs::pile(left, g, cards, zone, unordered, copy);
                for (const auto& token : tokens) right.add(token);
                left.view.finish(left.result.entities, 0); right.view.finish(right.result.entities, 0);
                require(!left.result.fault && !right.result.fault, "pile sorting fixture overflow");
                require(std::memcmp(&actual, &expected, sizeof(actual)) == 0, "pile indices changed token order or bytes");
                require(std::memcmp(&g, &before, sizeof(g)) == 0, "pile sorting changed Game");
            }
        }
    }
}
static void dome() {
    const Game before = combat(true);
    Game a = before, b = before;
    step(a, {ActionKind::EndTurn});
    for (int seed = 1; seed < 100; ++seed) {
        b = before; b.rng.ai.reset(seed);
        step(b, {ActionKind::EndTurn});
        if (a.monsters[0].intent != b.monsters[0].intent) break;
    }
    require(a.monsters[0].intent != b.monsters[0].intent, "Dome pair needs two legal next moves");
    equal(a, b);
    unequip_relic(a, RelicId::RunicDome); unequip_relic(b, RelicId::RunicDome);
    different(a, b);
}
static void discard_damage() {
    Game a = combat();
    const int index = a.hand.pop_back();
    a.cards[index] = make_card(CardId::GlassKnife);
    a.cards[index].base_damage = 4; a.cards[index].damage = 6;
    a.discard.push_back(index);
    Game b = a; b.cards[index].damage = 4;
    equal(a, b);
    b.cards[index].base_damage = 6; different(a, b);
    b.cards[index].base_damage = 4;
    open_grid_selection(a, PileId::Discard, 1, false);
    open_grid_selection(b, PileId::Discard, 1, false);
    equal(a, b);
    b.cards[index].base_damage = 6; different(a, b);
    b.cards[index].base_damage = 4;
    a.discard.clear(); b.discard.clear();
    a.hand.push_back(index); b.hand.push_back(index);
    a.screen = b.screen = Screen::Combat;
    different(a, b);
}
static void combat_card_values() {
    Game a = combat();
    const int index = a.hand[0];
    a.cards[index] = make_card(CardId::IronWave);
    apply_power(a, player_target, player_target, PowerId::Strength, 3);
    apply_power(a, player_target, player_target, PowerId::Dexterity, 2);
    card_applyPowers(a, index);
    const auto& card = a.cards[index];
    require(card.damage == card.base_damage + 3 && card.block == card_baseBlock(card) + 2, "live card fixture");
    const auto live = obs::card_token(a, card, obs::Zone::Hand, 1, true);
    require(live.values[3] == card.damage && live.values[4] == card.block, "combat preview lost live damage/block");
    Game b = a; b.cards[index].damage += 7; b.cards[index].block += 9;
    different(a, b);
    a.cards[index] = make_card(CardId::PoisonedStab);
    b = a; b.cards[index].magic += 4;
    different(a, b);
    a.hand.erase(0); a.draw.push_back(index);
    b = a; b.cards[index].damage += 5; b.cards[index].magic += 3;
    different(a, b);
    open_grid_selection(a, PileId::Draw, 1, false);
    open_grid_selection(b, PileId::Draw, 1, false);
    different(a, b);
    a.screen = b.screen = Screen::Combat;
    a.draw.pop_back(); a.exhaust.push_back(index); b = a;
    b.cards[index].damage += 5; b.cards[index].block += 9; b.cards[index].magic += 3;
    equal(a, b);
    b.cards[index].base_damage += 1;
    different(a, b);
}
static void conditional_card_description() {
    // SingleCardViewPopup.open rebuilds descriptions through makeStatEquivalentCopy.
    // These cards' conditional EXTENDED_DESCRIPTION variables do not survive that copy.
    for (auto id : {CardId::Blizzard, CardId::Stack, CardId::ThunderStrike, CardId::MindBlast, CardId::SpiritShield}) {
        Game a = combat();
        const int index = a.hand.pop_back();
        a.cards[index] = make_card(id);
        a.exhaust.push_back(index);
        Game b = a;
        if (id == CardId::Blizzard || id == CardId::MindBlast) b.cards[index].base_damage += 7;
        if (id == CardId::Stack || id == CardId::SpiritShield) b.cards[index].base_block_adjust += 9;
        if (id == CardId::ThunderStrike) b.cards[index].base_magic += 3;
        equal(a, b);
    }
    // Expunger.makeStatEquivalentCopy explicitly preserves its setX description.
    Game a = combat();
    a.cards[a.hand[0]] = make_card(CardId::Expunger);
    a.cards[a.hand[0]].base_magic = a.cards[a.hand[0]].magic = 3;
    Game b = a;
    b.cards[b.hand[0]].base_magic = b.cards[b.hand[0]].magic = 4;
    different(a, b);
}
static void combat_bottle_identity() {
    Game a = combat(), b = a;
    const int index = a.hand[0];
    b.cards[index].bottle_flame = true;
    equal(a, b);
    a.hand.erase(0); a.draw.push_back(index);
    b = a; b.cards[index].bottle_flame = true;
    equal(a, b);
    a.draw.pop_back(); a.discard.push_back(index);
    b = a; b.cards[index].bottle_lightning = true;
    equal(a, b);
    a.discard.pop_back(); a.exhaust.push_back(index);
    b = a; b.cards[index].bottle_tornado = true;
    equal(a, b);
    open_grid_selection(a, PileId::Exhaust, 1, false);
    open_grid_selection(b, PileId::Exhaust, 1, false);
    equal(a, b);
    a = combat(); b = a;
    b.run.master_deck[0].bottle_flame = true;
    different(a, b);
    a.room_phase = b.room_phase = RoomPhase::Complete;
    open_grid_selection(a, PileId::MasterDeck, 1, false);
    open_grid_selection(b, PileId::MasterDeck, 1, false);
    different(a, b);
}
static void dead_health_label() {
    Game a; reset(a, 123);
    CombatSetup setup; setup.encounter = EncounterId::Darkling;
    setup.deck = a.run.master_deck; setup.relics = a.run.relics;
    start_combat(a, setup);
    DamageInfo hit{}; hit.source = player_target; hit.target = 0; hit.amount = 999;
    monster_damage(a, 0, hit); run_until_decision(a);
    require(a.monsters[0].creature.hp == 0 && a.monsters[0].creature.half_dead, "Darkling death fixture");
    Game b = a; b.monsters[0].creature.max_hp += 5;
    equal(a, b);
    a.monsters[0].creature.hp = b.monsters[0].creature.hp = 1;
    a.monsters[0].creature.half_dead = b.monsters[0].creature.half_dead = false;
    different(a, b);
}
static void identical_selection_text() {
    Game a = combat();
    a.discard.push_back(a.draw.pop_back()); a.discard.push_back(a.draw.pop_back());
    Game b = a;
    Effect retrieve{}; retrieve.kind = EffectKind::BetterDiscardPileToHand;
    action_initialize_state(retrieve);
    retrieve.payload.state.BetterDiscardPileToHand.numberOfCards = 1;
    Effect meditate{}; meditate.kind = EffectKind::Meditate; meditate.extra = 1;
    action_initialize_state(meditate);
    add_bottom(a, retrieve); add_bottom(b, meditate);
    run_until_decision(a); run_until_decision(b);
    require(a.screen == Screen::Grid && b.screen == Screen::Grid, "retrieval prompt fixture");
    equal(a, b);
}
static void selection_intents() {
    for (auto screen : {Screen::Discard, Screen::Grid, Screen::CardReward}) {
        Game a = combat();
        if (screen == Screen::Discard) {
            Effect discard{}; discard.kind = EffectKind::Discard; discard.amount = 1;
            discard.source = discard.target = player_target;
            add_bottom(a, discard); run_until_decision(a);
        } else if (screen == Screen::Grid) {
            a.grid_cards = a.draw; open_grid_selection(a, PileId::Grid, 1, false);
        } else {
            a.reward_cards.push_back(create_card(a, make_card(CardId::Strike_G)));
            a.screen = Screen::CardReward;
        }
        require(a.screen == screen && !a.manager.turn_has_ended, "player selection fixture");
        Game b = a;
        b.monsters[0].intent = Intent::Attack; b.monsters[0].intent_damage = 39;
        b.monsters[0].intent_multi = true; b.monsters[0].intent_hits = 4;
        different(a, b);
        b = a; b.monsters[0].pending_intent = Intent::Attack; b.monsters[0].pending_base_damage = 39;
        equal(a, b);
        b = a; b.monsters[0].intent_damage += 9;
        a.monsters[0].intent = b.monsters[0].intent = Intent::Attack;
        different(a, b);
        equip_relic(a, RelicId::RunicDome); equip_relic(b, RelicId::RunicDome);
        equal(a, b);
        unequip_relic(a, RelicId::RunicDome); unequip_relic(b, RelicId::RunicDome);
        a.manager.turn_has_ended = b.manager.turn_has_ended = true;
        equal(a, b);
    }
}
static void grid_prompt_text() {
    const EffectKind kinds[]{EffectKind::AttackFromDeckToHand, EffectKind::SkillFromDeckToHand,
                             EffectKind::Exhume, EffectKind::DiscardPileToTopOfDeck};
    const int prompts[]{15, 16, 17, 18};
    for (int i = 0; i < 4; ++i) {
        Game g = combat();
        if (i == 2) { g.exhaust.push_back(g.draw.pop_back()); g.exhaust.push_back(g.draw.pop_back()); }
        if (i == 3) { g.discard.push_back(g.draw.pop_back()); g.discard.push_back(g.draw.pop_back()); }
        Effect e{}; e.kind = kinds[i]; e.amount = 1; action_initialize_state(e);
        add_bottom(g, e); run_until_decision(g);
        require(g.screen == Screen::Grid, "grid text fixture");
        const auto o = observe(g);
        require(o.tags[3 * obs::entity_capacity] == prompts[i], "distinct grid text lost its prompt identity");
        Game h = g;
        h.action_objects[h.manager.current_action].source = no_target;
        equal(g, h);
    }
}
static Game event(EventId id) {
    Game g; reset(g, 101);
    g.event = {}; g.event.id = id; event_initialize_state(g.event);
    g.room_kind = RoomKind::Event; g.room_phase = RoomPhase::Event; g.room_in_combat = false;
    g.room_dialog.clear(); g.image_dialog.clear(); g.screen = Screen::Event;
    event_enter(g);
    return g;
}
static void event_secrets() {
    Game a = event(EventId::Designer), b = a;
    b.event.state.Designer.adjustmentUpgradesOne = !a.event.state.Designer.adjustmentUpgradesOne;
    b.event.state.Designer.cleanUpRemovesCards = !a.event.state.Designer.cleanUpRemovesCards;
    equal(a, b);
    step(a, {ActionKind::Choose, 0}); step(b, {ActionKind::Choose, 0}); different(a, b);
    a = event(EventId::Falling); b = a;
    auto& f = b.event.state.Falling;
    for (int i = 0; i < b.run.master_deck.size; ++i) {
        const auto& c = b.run.master_deck[i];
        if (card_data(c.id).type == CardType::Skill && c.uuid != f.skillCard) { f.skillCard = c.uuid; break; }
    }
    equal(a, b);
    a = event(EventId::GremlinWheelGame); step(a, {ActionKind::Choose, 0}); b = a;
    b.event.state.GremlinWheelGame.result = (a.event.state.GremlinWheelGame.result + 1) % 6;
    equal(a, b);
    step(a, {ActionKind::Choose, 0}); step(b, {ActionKind::Choose, 0}); different(a, b);
    a = event(EventId::GremlinMatchGame);
    step(a, {ActionKind::Choose, 0}); step(a, {ActionKind::Choose, 0}); b = a;
    const auto& s = b.event.state.GremlinMatchGame;
    Card temp = b.cards[s.cards[0]]; b.cards[s.cards[0]] = b.cards[s.cards[1]]; b.cards[s.cards[1]] = temp;
    equal(a, b);
    b = a; step(b, {ActionKind::Choose, 0});
    different(a, b);
}
static int adventurer_clue(const obs::Observation& o) {
    for (int i = 0; i < o.entities; ++i)
        if (o.tags[i] == static_cast<int>(obs::Kind::Option) &&
            o.ids[3 * obs::entity_capacity + i] == static_cast<int>(EventId::DeadAdventurer) + 1)
            return o.values[obs::entity_capacity + i];
    return 0;
}
static void adventurer_previous_clue() {
    Game a = event(EventId::DeadAdventurer), b = a;
    b.event.state.DeadAdventurer.enemy = (a.event.state.DeadAdventurer.enemy + 1) % 3;
    different(a, b);
    obs::History history;
    const int clue = adventurer_clue(observe_recorded(a, history));
    require(clue == a.event.state.DeadAdventurer.enemy + 1, "displayed adventurer clue missing");
    for (int seed = 1; ; ++seed) {
        b = a; b.rng.misc.reset(seed);
        step(b, {ActionKind::Choose, 0});
        if (b.event.state.DeadAdventurer.screen == 0) break;
    }
    require(b.event.state.DeadAdventurer.numRewards == 1, "adventurer search fixture");
    a = b;
    b.event.state.DeadAdventurer.enemy = (a.event.state.DeadAdventurer.enemy + 1) % 3;
    equal(a, b);
    require(adventurer_clue(observe(a)) == 0, "unseen adventurer clue was inferred from hidden state");
    const auto remembered = observe_recorded(a, history), changed = observe(b, history);
    require(adventurer_clue(remembered) == clue, "displayed adventurer clue lost after search");
    require(std::memcmp(&remembered, &changed, sizeof(remembered)) == 0, "hidden enemy overwrote remembered clue");
    const auto copied = observe(a, history);
    require(std::memcmp(&remembered, &copied, sizeof(copied)) == 0, "copied adventurer history changed");
    b.event.state.DeadAdventurer.encounterChance += 25;
    different(a, b);
    step(a, {ActionKind::Choose, 1});
    require(adventurer_clue(observe_recorded(a, history)) == 0, "clue outlived v2 INTRO page");
    require(history.adventurer_clue == 0, "finished adventurer memory not cleared");
}
static void match_current_screen() {
    Game a = event(EventId::GremlinMatchGame);
    step(a, {ActionKind::Choose, 0}); step(a, {ActionKind::Choose, 0});
    step(a, {ActionKind::Choose, 0});
    const auto& s = a.event.state.GremlinMatchGame;
    int first = -1, second = -1;
    for (int i = 1; i < s.count; ++i) {
        const auto id = a.cards[s.cards[i]].id;
        if (id == a.cards[s.cards[0]].id) continue;
        if (first < 0) first = i;
        else if (id != a.cards[s.cards[first]].id) { second = i; break; }
    }
    require(second >= 0, "Match fixture needs two distinct mismatches");
    Game b = a;
    const Action x{ActionKind::Choose, first - 1}, y{ActionKind::Choose, second - 1};
    obs::History hx, hy;
    obs::record_action(hx, a, x); obs::record_action(hy, b, y);
    step(a, x); step(b, y);
    equal(a, b);
    const auto one = observe(a, hx), two = observe(b, hy), copied = observe(a, hx);
    require(std::memcmp(&one, &two, sizeof(one)) != 0, "displayed second Match face lost before next boundary");
    require(std::memcmp(&one, &copied, sizeof(one)) == 0, "copied Match memory changed");
    Game hidden = a;
    hidden.cards[s.cards[first]].id = CardId::Apotheosis;
    const auto unseen = observe(hidden, hx);
    require(std::memcmp(&one, &unseen, sizeof(one)) == 0, "covered face overwrote remembered face");
    hx = {}; hy = {};
    const auto cleared = observe(a, hx), other = observe(b, hy);
    require(std::memcmp(&cleared, &other, sizeof(cleared)) == 0, "reset retained Match history");
    obs::record_action(hx, a, {ActionKind::Choose, 0});
    require(hx.shown && hx.card.ids[4] == 1, "next Match action did not replace previous face");
    a.event.state.GremlinMatchGame.screen = 3;
    obs::record_action(hx, a, {ActionKind::Choose, 0});
    require(!hx.shown, "Match memory outlived the next action");
}
static void private_counters() {
    Game a = combat(), b;
    apply_power(a, player_target, player_target, PowerId::Ritual, 2);
    b = a;
    b.player.creature.powers.at(b.power_objects, 0).just_applied = !a.player.creature.powers.at(a.power_objects, 0).just_applied;
    equal(a, b);
}
static void unopened_content_and_prices() {
    Game a; reset(a, 101);
    a.room_kind = RoomKind::Shop;
    enter_shop(a);
    require(a.screen == Screen::Room, "shop entrance fixture");
    Game b = a;
    b.cards[b.shop.colored[0]].price += 1;
    equal(a, b);
    step(a, {ActionKind::Choose, 0}); step(b, {ActionKind::Choose, 0});
    different(a, b);
    reset(a, 101); a.room_kind = RoomKind::Treasure; enter_treasure(a);
    for (int seed = 1; seed < 100; ++seed) {
        b = a; b.rng.treasure.reset(seed); enter_treasure(b);
        if (b.chest.size == a.chest.size && b.chest.relic != a.chest.relic) break;
    }
    require(b.chest.size == a.chest.size && b.chest.relic != a.chest.relic, "chest pair needs legal alternate hidden contents");
    equal(a, b);
    reset(a, 101); open_map(a);
    b = a;
    b.rng.event.reset(456); b.rng.monster.reset(789); b.rng.treasure.reset(999);
    equal(a, b);
}
static void screen_local_state() {
    Game a = combat();
    a.grid_cards = a.hand;
    open_grid_selection(a, PileId::Grid, 1, false);
    Game b = a;
    b.selection_can_pick_zero = true; b.selection_for_upgrade = true;
    equal(a, b);

    reset(a, 101); a.room_kind = RoomKind::Rest; enter_campfire(a);
    obtain_potion(a, PotionId::FruitJuice, construct_potion(a, PotionId::FruitJuice));
    obs::History history; obs::Observation before, after; ActionSet actions;
    require(!obs::encode(a, history, before.view(), actions).fault, "rest observation failed");
    const int max_hp = a.player.creature.max_hp;
    const Action drink{ActionKind::PotionUse, 0};
    step(a, drink);
    require(a.player.creature.max_hp > max_hp, "fruit juice positive control failed");
    require(!obs::encode(a, history, after.view(), actions).fault, "rest after potion failed");
    int rest_options = 0;
    for (int i = 0; i < obs::entity_capacity; ++i) {
        if (before.tags[i] != static_cast<int>(obs::Kind::Option) ||
            before.ids[i] != static_cast<int>(obs::Control::Campfire) ||
            before.ids[3 * obs::entity_capacity + i] != static_cast<int>(CampfireOption::Rest) + 1) continue;
        ++rest_options;
        require(before.values[obs::entity_capacity + i] == after.values[obs::entity_capacity + i], "RestOption's frozen visible label changed after potion");
        require(before.values[obs::entity_capacity + i] == static_cast<int>(max_hp * 0.3f), "rest label disagrees with entry HP");
    }
    require(rest_options == 1, "rest label fixture lacks the displayed option");
    require(std::memcmp(&before, &after, sizeof(before)) != 0, "visible max HP and potion changes were omitted");
    ++positives;
    ++positive_screens[static_cast<int>(a.screen)];
}
static void reward_identity() {
    Game a; reset(a, 101); a.room_kind = RoomKind::Treasure; enter_treasure(a); open_chest(a);
    Game b = a;
    for (int i = 0; i < b.reward_objects.size; ++i) {
        auto& reward = b.reward_objects[i];
        reward.identity += 10;
        if (reward.relic_link >= 0) reward.relic_link += 10;
    }
    b.next_reward_identity += 10;
    equal(a, b);
    const auto observation = observe(a);
    int linked = 0;
    for (int i = 0; i < obs::entity_capacity; ++i) {
        if (observation.tags[i] != static_cast<int>(obs::Kind::Reward)) continue;
        const int target = observation.values[obs::entity_capacity + i];
        if (!target) continue;
        require(target != i + 1 && observation.tags[target - 1] == static_cast<int>(obs::Kind::Reward), "reward link does not point to another visible reward");
        require(observation.values[obs::entity_capacity + target - 1] == i + 1, "key/relic links are not reciprocal");
        ++linked;
    }
    require(linked == 2, "sapphire key/relic tradeoff is missing");
}
static void traces() {
    std::ifstream input(SPIRE_DEEP_ACTIONS);
    require(input.good(), "missing trace action inputs");
    int64_t seed; int count;
    while (input >> seed >> count) {
        Game g; reset(g, seed);
        for (int i = 0; i <= count; ++i) {
            hidden_rng(g);
            if (!positive_screens[static_cast<int>(g.screen)]) {
                Game h = g; h.player.creature.hp = g.player.creature.hp > 1 ? g.player.creature.hp - 1 : g.player.creature.hp + 1;
                different(g, h);
            }
            if (i == count) break;
            int kind, index, target; input >> kind >> index >> target;
            require(input.good(), "incomplete trace action input");
            step(g, {static_cast<ActionKind>(kind), index, target});
        }
    }
    for (int i = 0; i < 13; ++i) {
        if (static_cast<Screen>(i) == Screen::None) continue;
        require(pairs[i] > 0, "screen lacks non-interference coverage");
        require(positive_screens[i] > 0, "screen lacks a visible positive control");
    }
}
static void overflow() {
    obs::Observation o;
    obs::Writer w{o.view()};
    for (int i = 0; i < obs::entity_capacity; ++i) require(w.add(obs::token(obs::Kind::Card, 1)) == i + 1, "capacity reservation failed");
    require(w.add(obs::token(obs::Kind::Card, 1)) == 0, "overflow silently accepted");
    require(w.result.fault == static_cast<uint32_t>(obs::Fault::Entities), "overflow fault missing");
}
int main() {
    static_assert(sizeof(Game) == 37872);
    selection_intents(); grid_prompt_text(); conditional_card_description(); identical_selection_text(); dead_health_label(); match_current_screen(); adventurer_previous_clue(); combat_bottle_identity(); combat_card_values(); draw_and_identity(); pile_sorting_matches_tokens(); dome(); discard_damage(); event_secrets(); private_counters(); unopened_content_and_prices(); screen_local_state(); reward_identity(); traces(); overflow();
    for (int i = 0; i < 13; ++i) std::printf("screen=%d hidden_pairs=%d positive_controls=%d\n", i, pairs[i], positive_screens[i]);
    std::printf("visible_positive_controls=%d\n", positives);
}
