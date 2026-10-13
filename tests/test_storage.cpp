#include "host/batch.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace spire;

static void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static void compact_storage() {
    Game game{};
    reset(game, 3);
    CombatSetup setup{};
    for (int i = 0; i < master_deck_capacity; ++i) setup.deck.push_back(make_card(CardId::Defend_G));
    start_combat(game, setup);
    require(game.cards.size == 128 && game.cards[127].uuid == 128, "last byte-sized card index retains its UUID");
    for (int round = 0; round < 8; ++round) {
        for (int i = 0; i < action_capacity; ++i)
            add_bottom(game, units::actions::GainEnergy::make(1));
        run_until_decision(game);
    }
    require(game.player.energy == 3 + 8 * action_capacity, "completed action slots are reusable");
    for (int round = 0; round < 2; ++round) {
        for (int i = 0; i < action_capacity; ++i)
            add_bottom(game, units::actions::GainEnergy::make(1));
        clear_actions(game, game.manager.actions);
    }
    require(game.player.energy == 3 + 8 * action_capacity, "discarded queued actions release slots without executing");
    for (int i = 0; i < card_capacity; ++i) game.multi_damage.base[card_capacity - 1 - i] = i + 1;
    const int amounts[]{1, 200000, 1, 300000};
    DamageArray row{};
    row.assign(game.damage_exceptions, amounts, 4);
    game.multi_damage.set(0, row);
    apply_power(game, player_target, player_target, PowerId::Strength, 4);
    const int text = game.text.intern("CLOSED");
    Game copy = game;
    copy.multi_damage.base[card_capacity - 1] = 99;
    DamageArray changed = copy.multi_damage.get(0);
    const int replacement[]{9, 8, 7, 6};
    changed.assign(copy.damage_exceptions, replacement, 4);
    copy.multi_damage.set(0, changed);
    require(game.multi_damage.get(0).at(game.damage_exceptions, 1) == 200000 &&
            copy.multi_damage.get(0).at(copy.damage_exceptions, 1) == 8,
            "relocating and replacing sparse damage preserves 32-bit values and isolates storage");
    apply_power(copy, player_target, player_target, PowerId::Strength, 2);
    require(game.multi_damage.get(card_capacity - 1).at(game.damage_exceptions, 0) == 1 && power_amount(game, game.player.creature, PowerId::Strength) == 4 &&
            power_amount(copy, copy.player.creature, PowerId::Strength) == 6 &&
            std::strcmp(copy.text[text], "CLOSED") == 0, "a relocated Game owns independent arena and text storage");
}

static void interleaved_histories() {
    MoveHistories storage{};
    MoveHistory histories[7]{};
    for (int i = 0; i < 1024; ++i) storage.append(histories[i % 7], i % 121 - 60);
    MoveHistories copy = storage;
    for (int owner = 0; owner < 7; ++owner) {
        const int last = 1023 - (1023 - owner) % 7;
        for (int back = 0; back < histories[owner].size; ++back)
            require(copy.last(histories[owner], back) == (last - back * 7) % 121 - 60,
                    "packed links preserve interleaved histories through the final arena slot");
    }
}

static void fault_isolation() {
    host::Batch batch(2);
    batch.reset({7, 8});
    for (Game& game : batch.games) {
        CombatSetup setup{};
        setup.deck = game.run.master_deck;
        start_combat(game, setup);
    }
    record_fault(batch.games[0].fault, 123);
    record_fault(batch.games[0].fault, 456);
    unsigned char before[sizeof(Game)];
    std::memcpy(before, &batch.games[0], sizeof(Game));
    const int turn = batch.games[1].turn;
    batch.step({{ActionKind::EndTurn}, {ActionKind::EndTurn}});
    require(batch.faults() == std::vector<uint32_t>({123, 0}) &&
            std::memcmp(before, &batch.games[0], sizeof(Game)) == 0 && batch.games[1].turn == turn + 1,
            "the first fault persists, the faulted environment does not step, and its neighbor steps");
    std::memcpy(before, &batch.games[1], sizeof(Game));
    batch.reset_at(0, 9);
    require(batch.faults() == std::vector<uint32_t>({0, 0}) && batch.games[0].run.seed == 9 &&
            std::memcmp(before, &batch.games[1], sizeof(Game)) == 0, "indexed reset clears the fault without resetting its neighbor");
}

static void host_overflow_aborts() {
    const pid_t child = fork();
    require(child >= 0, "fork");
    if (child == 0) {
        const rlimit limit{0, 0};
        setrlimit(RLIMIT_CORE, &limit);
        Game game{};
        for (int i = 0; i <= card_capacity; ++i) game.cards.push_back(Card{});
        _exit(0);
    }
    int status = 0;
    require(waitpid(child, &status, 0) == child && WIFSIGNALED(status) &&
            (WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP), "host capacity overflow must abort in release builds");
}

int main() {
    compact_storage();
    interleaved_histories();
    fault_isolation();
    host_overflow_aborts();
}
