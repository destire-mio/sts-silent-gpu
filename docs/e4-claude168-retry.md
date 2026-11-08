# E4 #168 — Why job 5389 stopped

Job 5389 ended as `FAILED` with exit code `1:0` after 2,706 seconds. Rank 2 was evaluating the 120-hour combat supplement on the 384×6 dev-120 snapshot. The lookahead preview forward called `train/model.py:125`. The original log reads:

```text
[rank2]:     pointed = table[rows, refs].flatten(2)
[rank2]:               ~~~~~^^^^^^^^^^^^
[rank2]: IndexError: index is out of bounds for dimension with size 0
```

Torchrun then terminated the peer ranks. The controller recorded `student_worker_ended_without_save` and restored the 384 PPO worker on job 5390. The full 5389 job log contains zero completed updates and no CUDA OOM or OOM-kill record. The exact preview input that produced the empty dimension was not captured in the terminal stack.

The **supplementary evaluation** was the active GPU work at the exception. Its 384×6 policy and lookahead preview tensors were on device. The 768 student and dev-112 teacher had already been loaded on device by worker startup. Learner forward/backward and training rollout had not begun. The log provides no per-component VRAM split, learner allocated/reserved peak, or longest learner input for microbatch 256.

## Next natural continuation

The controller armed #168 for the next clean natural successor, currently job 5391. The retry uses the saved 768 student at update 8, full encoder recomputation, microbatch 256, global minibatch 32,768, and `expandable_segments:True`. The 8-hour supplement is deferred while distilling. Its snapshots and any existing progress remain available and are processed after the transition to PPO. The 2-hour 1,024-seed argmax exam and the 30.96 passing threshold remain in effect.

The candidate passed 21 isolated CPU checks. The four-V100 job 5390 and its dependency were still in place at arming. No learner result from the armed retry is available yet.
