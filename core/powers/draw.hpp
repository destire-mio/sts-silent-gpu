#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Draw : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    SPIRE_STATE(Draw, bool debuff;)
    static SPIRE_FN void initialPower(Game& g, Power& p, int) {
        p.state.Draw.debuff = p.amount < 0;
        g.player.hand_size += p.amount;
    }
    static SPIRE_FN bool isDebuff(const Power& p, int) { return p.state.Draw.debuff; }
    static SPIRE_FN void onRemove(Game& g, Power& p, int) { g.player.hand_size -= p.amount; }
    static SPIRE_FN void reducePower(Game& g, Power& p, int owner, int amount) {
        p.amount -= amount;
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::Draw;
            add_top(g, e);
        }
    }
};
}
