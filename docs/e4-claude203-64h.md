# E4 evaluation at 64 training hours

Job 5635 completed the raw and EMA evaluations at natural checkpoint 1948, 230456.7152344616 training seconds and 510,656,420 cumulative training decisions. It resumed from update 1923, serial 201, with Adam step 30779 and EMA. Updates 1889–1923 used #186 with entropy 0.01 on job 5634. Updates 1924–1948 used #203 with entropy 0.005 on job 5635. Both evaluations ran with key-last-chance enabled on source `2e85755f80a80b5d132ae8f7f59b5eb1d8a8d33a`.

Raw, EMA and the server's `dev-112.pt` baseline used the same 1,024 seeds, from 2,000,000,000 through 2,000,001,023. The baseline averaged 31.9570 floors. Paired standard errors use per-seed floor differences. Every denominator includes all 1,024 episodes.

| Weights | Mean floor | Paired difference vs dev-112 | Paired SE | Higher / tied / lower |
| --- | ---: | ---: | ---: | ---: |
| Raw | 32.9463 | +0.9893 | 0.4803 | 428 / 223 / 373 |
| EMA | 33.9932 | +2.0361 | 0.4458 | 457 / 241 / 326 |

| Milestone | Raw | EMA |
| --- | ---: | ---: |
| Act 1 boss cleared | 843/1024 | 883/1024 |
| Act 2 boss cleared | 359/1024 | 366/1024 |
| Act 3 first boss cleared | 103/1024 | 111/1024 |
| Act 3 second boss cleared | 36/1024 | 38/1024 |
| All three keys obtained | 240/1024 | 247/1024 |
| Act 4 entered | 35/1024 | 38/1024 |
| Shield and Spear defeated | 26/1024 | 35/1024 |
| Heart defeated | 3/1024 | 5/1024 |
| Ruby key | 252/1024 | 255/1024 |
| Emerald key | 378/1024 | 379/1024 |
| Sapphire key | 286/1024 | 307/1024 |
| Stalled | 1/1024 | 2/1024 |

EMA minus raw was +1.0469 floors with a paired SE of 0.4621 over 1,024 seeds. The independent CPU audit matched both sets of seed shards and ledgers, recorded the `dev-64.pt` and `dev-64-ema.pt` hashes, and reconciled the cumulative decision count from the 62-hour audit through both jobs.

The archived audit SHA256 is `3ab84d2982be1a659d758d267cbd86bf5dfc28aaad0399dd7c3f2ff396155353`.
