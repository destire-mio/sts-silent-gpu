# E4 #198 E2 training-root replay audit

Native c157 run-determinize CPU engine. The released e2-roots-v1 archive was checked against its SHA-256. The audit used only the 5,815 training-partition roots.

| Stage | Train inputs | Reached declared boundary | Matched recorded state fields | Excluded |
|---|---:|---:|---:|---:|
| act2_entry | 1443 | 1443 | 1443 | 0 |
| act3_entry | 871 | 871 | 871 | 0 |
| act3_boss | 774 | 774 | 774 | 0 |
| act4_entry | 385 | 385 | 385 | 0 |
| heart_start | 254 | 254 | 254 | 0 |
| heart_turn2 | 251 | 251 | 251 | 0 |
| heart_turn3 | 212 | 212 | 212 | 0 |
| heart_turn5 | 170 | 170 | 170 | 0 |
| act1_boss | 1455 | 1455 | 1455 | 0 |

heart_mid combines 633 matched turn-2, turn-3 and turn-5 roots.

State fields compared were floor, turn, HP, max HP, gold, keys, deck, relics and potions. Each prefix replay was also checked against the declared boundary.

Native replay used 4,599,180 decisions in 12.7878 s wall and 12.7218 s CPU.

- Source archive SHA-256 43eb580745d14a387d2ad955a6c5c6d294c3398e8a8480c8fca1301eff41b4e2
- Native replay binary SHA-256 da512073ad3a6715cebe03c7df5a9ecd7f5cd8bc0d6d247fcce61f54d10acb6d
- Audit JSON SHA-256 1df0a5c5e778092567a167b33ffc5a882c806a7a00fbd245331f8a1e0983a8ec
- Accepted train-only gzip SHA-256 1e0633a0d99eb70e0789173c8f0596b407e6fe4f6360a689cb1087f6211d9162

The standalone pool integration and per-update overhead measurement are tracked separately.
