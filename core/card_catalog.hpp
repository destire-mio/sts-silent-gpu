#pragma once

#define SPIRE_CARD_CATALOG(X) \
    X(Strike_G, "Strike_G", Attack, Enemy, 1, 6, -1, -1, 3, 0, 0, false, false) \
    X(Defend_G, "Defend_G", Skill, Self, 1, -1, 5, -1, 0, 3, 0, false, false) \
    X(Neutralize, "Neutralize", Attack, Enemy, 0, 3, -1, 1, 1, 0, 1, false, false) \
    X(Survivor, "Survivor", Skill, Self, 1, -1, 8, -1, 0, 3, 0, false, false) \
    X(AscendersBane, "AscendersBane", Curse, None, -2, -1, -1, -1, 0, 0, 0, false, true) \
    X(Slimed, "Slimed", Status, Self, 1, -1, -1, -1, 0, 0, 0, true, false) \
    X(Dazed, "Dazed", Status, None, -2, -1, -1, -1, 0, 0, 0, false, true)
