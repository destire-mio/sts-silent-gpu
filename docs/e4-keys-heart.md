### Keys and the Heart objective at the next natural handoff

Recall (Ruby key) and the Sapphire-key reward are no longer removed by Step0. A free reward relic is forced only when no legal Sapphire-key action is available; the first shop-entry rule and Emerald-key acquisition are unchanged. The existing RewardLedger is unchanged: Act 4 entry +10, Shield and Spear +10, Heart defeat +50, with no reward for keys themselves.

New standard evaluation episodes and per-game ledgers retain the terminal Ruby/Emerald/Sapphire bits. Summary tables add each key count, all three keys, Act 4 entry, Shield-and-Spear wins, and Heart wins, using the corresponding RewardLedger reach markers. The victories column still means second Act 3 boss wins; heart_wins is the final Heart objective. A Heart victory keeps outcome victory; an Act 3 win without a Heart victory is act3_win.

| New evaluation field | Definition |
| --- | --- |
| key_counts.ruby / emerald / sapphire | Number of games retaining each key bit |
| all_three_keys | Number of games retaining all three bits |
| act4_entries | RewardLedger.reach.act4 |
| shield_spear_wins | RewardLedger.reach.shield_spear |
| victories | RewardLedger.reach.act3_second_boss |
| heart_wins | RewardLedger.reach.heart |

Historical engine status: the existing [deep-run acceptance manifest](../inventory/deep-neow.json), validated at engine commit e304c2c2cd5d280a5a152c052d17cf0a568174c1, reports original-game comparisons through Act 4 and the Heart, including one winning fresh run with both scopes and terminals matching; this is historical coverage, not a new replay of the current training build. No additional original-game verification is required before continuing training.

This source is prepared for the next natural allocation handoff, preserving the current model, optimizer, scaler, collector, pools and RNG. Existing workers keep their imported mask until that handoff. Old evaluation files remain original records; missing historical per-key fields must not be fabricated. No evaluation rerun, additional GPU job or learner optimization is introduced.
