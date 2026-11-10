"""Run Stage12 no-search PPO, saving rank collection state on yield."""
import argparse
from dataclasses import asdict
from copy import deepcopy
import gc
import gzip
import json
import math
import os
from pathlib import Path
import signal
import resource
import statistics
import subprocess
import time

import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel

from train import OBSERVATION_VERSION
from train.e4 import AllocationEnded, REACH
from train.e4_stage12 import (CLSPolicy, SeparateCritic, NoSLSearch, NoSLCollector, StageTimes,
    new_cursor, precision_probe, choose_microbatch, advance_minibatch, finish_metrics,
    save_rank, restore_rank, health_stop, next_learning_rate, decision_entropy_summary, student_log_baseline,
    restart_rank_observation, restart_rank_engine, pandora_cuda_gates, MAX_LEARNING_RATE,
    DISTILLATION_GLOBAL_MINIBATCH, PPO_GLOBAL_MINIBATCH, GAE_LAMBDA, GAE_GAMMA,
    DEFAULT_ENTROPY_COEFFICIENT, decision_training_summary)
from train.model import ModelConfig
from train.rollout import synchronize
from train.e4_stage6.training_worker import move_tensors
from train.e4_midstart import MidstartCollector, midstart_policy
from train.e4_learner_profile import TwoMicrobatchProfile
from train.e4_recompute import TRIAL_ROUNDS, prepare_round, finish_round
from train.e4_checkpoint import load_matching_rank
from train.e4_value_diagnostics import (explained_variance, nonforced_entropy, value_coefficient,
                                        record_ppo_calibration_round)
from train.e4_evaluation_ledger import EvaluationLedgerCollector, exam_breakdown, value_correlations
from train.e4_lookahead_window import lookahead60_window
from train.e4_expert_window import due_combat_comparison, run_combat_supplements


DISTILLATION_LEARNING_RATE = 1.5e-4
DISTILLATION_PASS_FLOOR = 30.96
DISTILLATION_REPORT_ROUND = 150


def scored_distillation(state):
    return (state.get("model_config", {}).get("width") == 768
            and bool(state.get("teacher_path")) and not state.get("distillation_evaluated"))


def new_round_minibatch(state):
    return DISTILLATION_GLOBAL_MINIBATCH if scored_distillation(state) else PPO_GLOBAL_MINIBATCH


def apply_resume_learning_rate(state):
    state["learning_rate"] = (DISTILLATION_LEARNING_RATE if scored_distillation(state)
                              else min(state["learning_rate"], MAX_LEARNING_RATE))


def passing_distillation_score(row):
    if (row["event"] == "evaluation" and row["selection"] == "argmax"
            and row["complete"] and row["episodes"] == 1024
            and row["phase"] == "distillation" and row["mean_floor"] >= DISTILLATION_PASS_FLOOR):
        return row["mean_floor"]
    return None


def distillation_limit_due(state):
    return (scored_distillation(state) and state["distillation_updates"] >= DISTILLATION_REPORT_ROUND
            and not state.get("claude166_round_limit_reported") and state["stop_reason"] is None)


def write_json(path, value):
    temporary = Path(str(path) + ".tmp")
    temporary.write_text(json.dumps(value, allow_nan=False) + "\n")
    temporary.replace(path)


def metrics(episodes):
    count = len(episodes)
    return dict(episodes=count, mean_floor=statistics.mean(e["floor"] for e in episodes) if count else None,
        boss_pass_rates={name:sum(e["reach"][name] for e in episodes) / count if count else None
                         for name in ("act1_boss", "act2_boss", "act3_first_boss")},
        victories=sum(e["reach"]["act3_second_boss"] for e in episodes),
        key_counts={name:sum(e["keys"][name] for e in episodes) for name in ("ruby","emerald","sapphire")},
        all_three_keys=sum(all(e["keys"].values()) for e in episodes),
        act4_entries=sum(e["reach"]["act4"] for e in episodes),
        shield_spear_wins=sum(e["reach"]["shield_spear"] for e in episodes),
        heart_wins=sum(e["reach"]["heart"] for e in episodes))



TRAINING_MILESTONES = ("act1_boss", "act2_boss", "act3_first_boss", "act3_second_boss",
                       "act4", "shield_spear", "heart")


def record_training_milestones(state, fresh, midstart):
    totals = state.setdefault("training_milestones", {})
    report = {}
    for group, episodes in (("fresh", fresh), ("midstart", midstart)):
        # The terminal reward ledger includes milestones inherited by midstarts.
        completed = [episode for episode in episodes if episode["outcome"] != "stalled"]
        counts = {name: sum(episode["reach"][name] for episode in completed)
                  for name in TRAINING_MILESTONES}
        previous = totals.get(group, {})
        cumulative = {name: previous.get(name, 0) + count for name, count in counts.items()}
        totals[group] = cumulative
        report[group] = dict(round=counts, cumulative=cumulative.copy())
    return report


def paired_summary(episodes, baseline):
    previous = {e["seed"]:e["floor"] for e in baseline}
    deltas = [e["floor"] - previous[e["seed"]] for e in episodes if e["seed"] in previous]
    return dict(mean_delta=statistics.mean(deltas) if deltas else None,
                paired_SE=statistics.stdev(deltas) / math.sqrt(len(deltas)) if len(deltas) > 1 else None,
                paired_cases=len(deltas))


def save_dev_weights(policy, output, training_seconds):
    path = output / f"dev-{int(training_seconds // 7200) * 2}.pt"
    if not path.exists():
        weights = policy.state_dict()
        if hasattr(policy, "auxiliary_heads"):
            weights = {key: value for key, value in weights.items() if not key.startswith("auxiliary_heads.")}
        torch.save(move_tensors(weights, torch.device("cpu")), path)
    return path.name


def rollback_dev78(policy, state, output):
    if "claude150_dev78_rollback" in state:
        return False
    policy.load_state_dict(torch.load(output / "dev-78.pt", map_location="cpu", weights_only=True), strict=True)
    state["claude150_dev78_rollback"] = dict(
        checkpoint="dev-78.pt", update=state["update"], training_seconds=state["training_seconds"])
    return True


def source_commit():
    root = Path(__file__).resolve().parents[1]
    if (root / ".git").exists():
        result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    return os.environ.get("E4_SOURCE_COMMIT")


def startup_value_setting(state, warming):
    if warming:
        return dict(coefficient=1., source="value_warmup")
    calibration = state.get("value_calibration")
    if state.get("standalone_value_coefficient") is not None:
        source = "fixed"
    elif state.get("claude171_value_calibration_pending") or calibration is None:
        source = "pending_measurement"
    elif calibration["completed_updates"] < (19 if "start_coefficient" in calibration else 20):
        source = "ramp"
    else:
        source = "measured"
    return dict(coefficient=value_coefficient(state), source=source)


def main(argv=None, *, device_type="cuda", stop_after_update=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--deadline", type=float)
    parser.add_argument("--init", type=Path)
    parser.add_argument("--teacher", type=Path)
    parser.add_argument("--value-coefficient", type=float, help="Persist a fixed standalone value loss coefficient")
    parser.add_argument("--late-pools", action="store_true", help="Use E2 and natural late midstart pools in standalone PPO")
    parser.add_argument("--late-pool-fraction", type=float, help="Share of regular starts allocated to late pools (default .15)")
    parser.add_argument("--late-pool-roots", type=Path, help="E2 train-root gzip (default artifacts/e2-roots/collection-train.jsonl.gz)")
    parser.add_argument("--key-last-chance", action="store_true", help="Preserve last Act 3 key opportunities in standalone PPO and exams")
    parser.add_argument("--separate-critic", action="store_true", help="Train a separate 384x6 critic for standalone PPO")
    parser.add_argument("--critic-learning-rate", type=float, help="Fixed critic Adam LR (default follows policy LR)")
    parser.add_argument("--combat-aux-weight", type=float, help="Combat prediction loss weight (default 0)")
    parser.add_argument("--global-aux-weight", type=float, help="Global prediction loss weight (default 0)")
    parser.add_argument("--lanes", type=int, default=2048)
    parser.add_argument("--microbatch", type=int, default=256)
    parser.add_argument("--recompute", choices=("on", "off"), default="on")
    parser.add_argument("--dtype", choices=("fp16", "bf16"), default="fp16")
    parser.add_argument("--rollout-length", type=int, default=64)
    parser.add_argument("--compile", action="store_true", help="Compile standalone learner encoder layers")
    parser.add_argument("--fused-sdpa", action="store_true", help="Prefer flash or memory-efficient SDPA in the standalone learner")
    parser.add_argument("--batch-stop-file", type=Path)
    parser.add_argument("--reward-scheme", choices=("current", "milestone"),
                        help="Standalone reward ledger (default current)")
    parser.add_argument("--value-warmup-updates", type=int,
                        help="Standalone value-only updates before PPO (default 0)")
    parser.add_argument("--ema-decay", type=float, help="Standalone policy EMA decay (default 0)")
    parser.add_argument("--arena-fraction", type=float, help="Standalone practice lane fraction (default 0)")
    args = parser.parse_args(argv)
    slurm_job = os.environ.get("SLURM_JOB_ID")
    standalone = slurm_job is None
    if not standalone and (args.late_pools or args.late_pool_fraction is not None or args.late_pool_roots is not None):
        parser.error("late pools are standalone only")
    if args.late_pool_fraction is not None and not (0. <= args.late_pool_fraction <= 1.):
        parser.error("--late-pool-fraction must be in [0, 1]")
    if args.key_last_chance and not standalone:
        parser.error("--key-last-chance is standalone only")
    if not standalone and (args.separate_critic or args.critic_learning_rate is not None):
        parser.error("separate critic settings are standalone only")
    if args.critic_learning_rate is not None and (
            not math.isfinite(args.critic_learning_rate) or args.critic_learning_rate <= 0):
        parser.error("--critic-learning-rate must be finite and positive")
    if args.value_coefficient is not None:
        if not standalone:
            parser.error("--value-coefficient is standalone only")
        if not math.isfinite(args.value_coefficient) or args.value_coefficient < 0:
            parser.error("--value-coefficient must be finite and nonnegative")
    requested_auxiliary = {"combat": args.combat_aux_weight, "global": args.global_aux_weight}
    for name, weight in requested_auxiliary.items():
        if weight is not None and not standalone:
            parser.error(f"--{name}-aux-weight is standalone only")
        if weight is not None and (not math.isfinite(weight) or weight < 0):
            parser.error(f"--{name}-aux-weight must be finite and nonnegative")
    for flag, value, maximum_inclusive in (("--ema-decay", args.ema_decay, False),
                                            ("--arena-fraction", args.arena_fraction, True)):
        if value is not None:
            if not standalone:
                parser.error(f"{flag} is standalone only")
            if not math.isfinite(value) or value < 0 or value > 1 or (value == 1 and not maximum_inclusive):
                parser.error(f"{flag} must be finite and in [0, {'1]' if maximum_inclusive else '1)'}")
    if not standalone and (args.reward_scheme is not None or args.value_warmup_updates is not None):
        parser.error("reward scheme and value warmup are standalone only")
    if args.value_warmup_updates is not None and args.value_warmup_updates < 0:
        parser.error("--value-warmup-updates must be nonnegative")
    if not standalone and (args.compile or args.fused_sdpa):
        parser.error("learner speed options are standalone only")
    rank, world, local = (int(os.environ[k]) for k in ("RANK", "WORLD_SIZE", "LOCAL_RANK"))
    if standalone and world != 1:
        parser.error("the standalone entry requires torchrun --standalone --nproc_per_node=1")
    if not standalone and args.deadline is None:
        parser.error("--deadline is required under Slurm")
    if min(args.lanes, args.microbatch, args.rollout_length) < 1:
        parser.error("lanes, microbatch and rollout length must be positive")
    torch.set_num_threads(1)
    cuda = device_type == "cuda"
    device = torch.device("cuda", local) if cuda else torch.device("cpu")
    if cuda:
        torch.cuda.set_device(device)
        dist.init_process_group("nccl", device_id=device)
    else:
        dist.init_process_group("gloo")
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    stop_file = output / "stop-requested"
    deadline = (time.monotonic() + args.deadline - time.time() - 180
                if args.deadline is not None else float("inf"))
    torch.manual_seed(20261031)
    common = output / "model.pt"
    saved = torch.load(common, map_location="cpu", weights_only=False) if common.exists() else None
    saved_state = saved["state"] if standalone and saved else {}
    saved_late_pools = saved_state.get("standalone_late_pools")
    late_pools = args.late_pools or saved_late_pools is not None
    if not late_pools and (args.late_pool_fraction is not None or args.late_pool_roots is not None):
        parser.error("late pool settings require --late-pools")
    if late_pools and (args.teacher is not None or saved_state.get("teacher_path")):
        parser.error("--late-pools requires teacher-free standalone PPO")
    late_settings = None
    if late_pools:
        previous_late = saved_late_pools or {}
        fraction = args.late_pool_fraction if args.late_pool_fraction is not None else previous_late.get("fraction", .15)
        roots = args.late_pool_roots or Path(previous_late.get("roots", "artifacts/e2-roots/collection-train.jsonl.gz"))
        late_settings = dict(fraction=fraction, roots=str(roots.resolve()))
    saved_key_mask = saved_state.get("standalone_key_last_chance", False)
    enable_key_mask_on_resume = standalone and saved is not None and args.key_last_chance and not saved_key_mask
    key_last_chance = args.key_last_chance or saved_key_mask
    saved_critic = saved.get("critic") if saved else None
    create_critic_on_resume = saved is not None and args.separate_critic and saved_critic is None
    separate_critic = args.separate_critic or saved_critic is not None
    critic_rate_override = args.critic_learning_rate
    if saved_critic is not None:
        if critic_rate_override is not None and critic_rate_override != saved_critic["learning_rate_override"]:
            parser.error("resume requires the checkpoint's critic learning rate")
        critic_rate_override = saved_critic["learning_rate_override"]
    if critic_rate_override is not None and not separate_critic:
        parser.error("--critic-learning-rate requires --separate-critic")
    control = saved["state"].get("claude186_control") if saved and not standalone else None
    if control is not None and (args.lanes, args.rollout_length) != (control["lanes"], control["rollout_length"]):
        parser.error("the Slurm control requires its original lane count and rollout length")
    saved_reward = saved_state.get("standalone_reward_scheme", "current")
    reward_scheme = args.reward_scheme or saved_reward
    saved_warmup = saved_state.get("standalone_value_warmup", {}).get("updates", 0)
    warmup_updates = args.value_warmup_updates if args.value_warmup_updates is not None else saved_warmup
    if standalone and saved and (reward_scheme != saved_reward or warmup_updates != saved_warmup):
        parser.error("resume requires the checkpoint reward scheme and warmup length")
    if warmup_updates and args.value_coefficient is not None:
        parser.error("value warmup remeasures the PPO coefficient; omit --value-coefficient")
    if (reward_scheme == "milestone" or warmup_updates) and (args.teacher is not None or scored_distillation(saved_state)):
        parser.error("milestone rewards and value warmup require standalone PPO")
    collector_options = {}
    if reward_scheme == "milestone":
        from train.e4 import MilestoneRewardLedger
        collector_options["reward_ledger_type"] = MilestoneRewardLedger
    ema_decay = args.ema_decay if args.ema_decay is not None else saved_state.get("standalone_ema_decay", 0.)
    if control is not None:
        ema_decay = control["ema_decay"]
    arena_fraction = (args.arena_fraction if args.arena_fraction is not None
                      else saved_state.get("standalone_arena_fraction", 0.))
    if not arena_fraction and saved_state.get("standalone_arena_fraction"):
        parser.error("an arena checkpoint requires a positive arena fraction to resume its live segments")
    if arena_fraction and (args.teacher is not None or scored_distillation(saved_state)):
        parser.error("arena practice requires standalone PPO")
    engine_transition = (not standalone and control is None and saved is not None
                         and "claude153_pandora_grid" not in saved["state"])
    if engine_transition:
        if rank == 0:
            import spire_gpu
            pandora_cuda_gates(Path(spire_gpu.__file__).parent,
                output / f"engine153-gates-{(slurm_job or 'standalone')}.xml")
        dist.barrier()
    initial_weights = None
    settings = dict(lanes=args.lanes, rollout_length=args.rollout_length, microbatch=args.microbatch,
                    recompute=args.recompute, dtype=args.dtype)
    if saved and "model_config" in saved["state"]:
        config = ModelConfig(**saved["state"]["model_config"])
        if standalone and saved["state"].get("standalone_settings") != settings:
            parser.error("standalone resume requires the same lane, rollout, precision and learner settings")
    elif standalone:
        if args.init is None:
            parser.error("--init is required for a fresh standalone run")
        initial_weights = torch.load(args.init, map_location="cpu", weights_only=True)
        width = initial_weights["cls"].shape[-1]
        if width not in (384, 768):
            parser.error("--init must contain 384- or 768-wide Stage12 weights")
        config = ModelConfig(width, width // 64, width // 64, width * 4)
        if args.teacher is not None and width != 768:
            parser.error("--teacher is for 768-wide distillation")
    else:
        config = ModelConfig(256, 4, 4, 1024)
    if separate_critic and (not standalone or config.width != 384 or config.layers != 6
                            or args.teacher is not None or saved_state.get("teacher_path")):
        parser.error("--separate-critic requires standalone teacher-free 384x6 PPO")
    saved_auxiliary = saved["state"].get("standalone_auxiliary_weights", {}) if standalone and saved else {}
    auxiliary_weights = {}
    for name, weight in requested_auxiliary.items():
        weight = saved_auxiliary.get(name, 0.) if weight is None else weight
        if weight:
            auxiliary_weights[name] = weight
    if saved_auxiliary and auxiliary_weights.keys() != saved_auxiliary.keys():
        parser.error("auxiliary resume requires the same enabled heads")
    if auxiliary_weights:
        if args.teacher is not None or (saved and scored_distillation(saved["state"])):
            parser.error("auxiliary heads require standalone PPO")
        from train.e4_stage12_auxiliary import AuxiliaryPolicy
        policy = AuxiliaryPolicy(config)
    else:
        policy = CLSPolicy(config)
    if initial_weights is not None:
        policy.load_state_dict(initial_weights, strict=True)
        del initial_weights
    if standalone:
        policy.encoder_dtype_override = torch.float16 if args.dtype == "fp16" else torch.bfloat16
        policy.encoder_checkpoint_enabled = args.recompute == "on"
    if auxiliary_weights:
        if saved and not saved_auxiliary:
            policy.load_state_dict(saved["model"], strict=True)
        policy.install_auxiliary_heads(auxiliary_weights)
    if create_critic_on_resume and (not auxiliary_weights or saved_auxiliary):
        policy.load_state_dict(saved["model"], strict=True)
    policy.to(device)
    critic = SeparateCritic(policy) if separate_critic else None
    if critic is not None:
        policy.value.requires_grad_(False)
    if standalone and (args.compile or args.fused_sdpa):
        from train.colab_learner import configure_learner
        configure_learner(policy, compile_encoder=args.compile, fused_sdpa=args.fused_sdpa)
        if critic is not None:
            configure_learner(critic, compile_encoder=args.compile, fused_sdpa=args.fused_sdpa)
    if auxiliary_weights:
        # A legacy partial learner may still finish without auxiliary targets.
        model = DistributedDataParallel(policy, device_ids=[local] if cuda else None, broadcast_buffers=False,
                                        find_unused_parameters=True)
        trunk_parameters = [parameter for name, parameter in policy.named_parameters()
                            if not name.startswith("auxiliary_heads.")]
        optimizer = torch.optim.Adam(trunk_parameters, lr=1e-4, eps=1e-5)
        if saved and not saved_auxiliary:
            optimizer.load_state_dict(saved["optimizer"])
        optimizer.add_param_group({"params": policy.auxiliary_heads.parameters()})
    else:
        model = DistributedDataParallel(policy, device_ids=[local] if cuda else None, broadcast_buffers=False)
        optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, eps=1e-5)
    ppo_rng = torch.Generator(device=device).manual_seed(20261031)
    seed_rng = torch.Generator().manual_seed(20261131 + rank)
    storage_slots = 1024
    collector_type = MidstartCollector
    training_collector_options = dict(collector_options)
    if arena_fraction:
        from train.e4_stage12_arena import ArenaCollector, POOL_SLOTS
        storage_slots += POOL_SLOTS
        collector_type = ArenaCollector
        training_collector_options["fraction"] = arena_fraction
    if late_pools:
        from train.e4_late_pools import LateArenaCollector, LateMidstartCollector, LATE_SLOTS
        collector_type = LateArenaCollector if arena_fraction else LateMidstartCollector
        training_collector_options.update(late_fraction=late_settings["fraction"], late_offset=storage_slots)
        storage_slots += LATE_SLOTS
    search = NoSLSearch(args.lanes, device, 20261031 + rank, storage_slots=storage_slots,
                        key_last_chance=key_last_chance)
    collector = collector_type(search, seed_rng, rank=rank, **training_collector_options)
    search.deadline = deadline
    search.evidence_directory = output
    search.training_critic = critic
    state = dict(update=0, training_seconds=0., initial_entropy=None, next_evaluation=7200.,
                 precision=None, microbatch=None, checkpoint_serial=0, world=world,
                 stop_reason=None, high_kl_streak=0, learning_rate=1e-4,
                 observation_version=OBSERVATION_VERSION)
    if standalone and saved is None:
        state.update(model_config=asdict(config), standalone_settings=settings,
                     precision=f"encoder_{args.dtype}_heads_fp32", microbatch=args.microbatch)
        if key_last_chance:
            state["standalone_key_last_chance"] = True
        if args.teacher is not None:
            state.update(teacher_path=str(args.teacher.resolve()), teacher_model_config=asdict(ModelConfig(384, 6, 6, 1536)),
                         teacher_evaluated_model="dev-112.pt", teacher_label="teacher-dev112-argmax",
                         distillation_updates=0, learning_rate=DISTILLATION_LEARNING_RATE,
                         claude168_defer_supplement=True)
        elif critic is not None:
            state.update(learning_rate=MAX_LEARNING_RATE, standalone_value_coefficient=.5)
        else:
            state.update(learning_rate=MAX_LEARNING_RATE, claude171_value_calibration_pending=True)
    cursor = None
    expert_saved = None
    rank_path = output / f"rank-{rank}.pt"
    committed_rank_path = None
    round_state = dict(ready=None, seconds=0., decisions_start=0)
    if common.exists():
        if not auxiliary_weights or saved_auxiliary:
            if not create_critic_on_resume:
                policy.load_state_dict(saved["model"], strict=True)
            optimizer.load_state_dict(saved["optimizer"])
        state.update(saved["state"])
        apply_resume_learning_rate(state)
        previous_observation = saved['state'].get('observation_version', 13)
        assert previous_observation in (13, OBSERVATION_VERSION)
        state['observation_version'] = OBSERVATION_VERSION
        policy.encoder_fp16 = state["precision"] == "encoder_fp16_heads_fp32"
        if control is None or state["checkpoint_serial"] != 0:
            ppo_rng.set_state(saved["ppo_rng"])
        for owner in range(world):
            if owner == rank:
                candidate, committed_rank_path = load_matching_rank(
                    rank_path, (state["update"], state["checkpoint_serial"]),
                    lambda path: torch.load(path, map_location="cpu", weights_only=False))
                if candidate is not None:
                    if engine_transition:
                        partial_steps = candidate['cursor']['optimizer_steps'] if candidate['cursor'] else 0
                        candidate = restart_rank_engine(candidate, collector)
                        print(json.dumps(dict(event='engine153_collector_restart', rank=rank,
                            update=state['update'], retained_partial_optimizer_steps=partial_steps,
                            live_collector_cursor_and_four_pools_discarded=True,
                            seed_rng_preserved=True)), flush=True)
                    elif previous_observation != OBSERVATION_VERSION:
                        partial_steps = candidate['cursor']['optimizer_steps'] if candidate['cursor'] else 0
                        candidate = restart_rank_observation(candidate, collector)
                        print(json.dumps(dict(event='observation_transition', rank=rank,
                            previous_version=previous_observation, observation_version=OBSERVATION_VERSION,
                            retained_partial_optimizer_steps=partial_steps,
                            live_collector_and_cursor_discarded=True,
                            pool_sizes=[len(p) for p in candidate['collector']['midstart']['pools']])), flush=True)
                    expert_saved = candidate.get("expert")
                    cursor, _, round_state = restore_rank(candidate, collector, device, ppo_rng)
                    if committed_rank_path != rank_path:
                        print(json.dumps(dict(event="resumed_previous_rank_checkpoint", rank=rank,
                            update=state["update"], serial=state["checkpoint_serial"])), flush=True)
                else:
                    print(json.dumps(dict(event="resume_fresh_collector", rank=rank, reason="rank checkpoint differs from shared checkpoint")), flush=True)
                del candidate
            dist.barrier()
        if state["world"] != world:
            retained_partial_steps = cursor['optimizer_steps'] if cursor is not None else 0
            cursor = None
            round_state["ready"] = None
            if expert_saved is not None:
                expert_saved['round'] = None
            print(json.dumps(dict(event="resume_world_changed", previous=state["world"], current=world,
                                  retained_partial_optimizer_steps=retained_partial_steps,
                                  unfinished_learner="restart collection, retain model and optimizer")), flush=True)
        print(json.dumps(dict(event="resumed", rank=rank, update=state["update"])), flush=True)
    if engine_transition:
        state['claude153_pandora_grid'] = dict(update=state['update'],
            training_seconds=state['training_seconds'], engine_fix='2e8a69569e4a2144b3c94a3d480bcdc5d8867d17',
            fresh_collector=True, four_pools_discarded=True, cuda_five_gates_passed=True)
    if not standalone and control is None and rollback_dev78(policy, state, output):
        retained_partial_steps = cursor['optimizer_steps'] if cursor is not None else 0
        cursor = None
        collector.partial = None
        round_state["ready"] = None
        print(json.dumps(dict(event="claude150_dev78_rollback", rank=rank, world=world,
            update=state["update"], training_seconds=state["training_seconds"],
            retained_partial_optimizer_steps=retained_partial_steps,
            model_checkpoint="dev-78.pt", expert_beta=0., expert_collection=False,
            optimizer_clock_pools_rng_preserved=True)), flush=True)
    if expert_saved is not None:
        expert_saved["round"] = None
    if standalone and args.value_coefficient is not None:
        state["standalone_value_coefficient"] = args.value_coefficient
    if auxiliary_weights:
        state["standalone_auxiliary_weights"] = auxiliary_weights
        if "global" in auxiliary_weights and not hasattr(collector, "auxiliary_fresh"):
            collector.auxiliary_fresh = dict(pending={}, started=[], finished=[])
    ema = None
    if ema_decay:
        from train.e4_stage12_ema import ExponentialWeights
        ema = ExponentialWeights(policy, ema_decay, saved.get("ema_weights") if saved else None)
        if standalone:
            state["standalone_ema_decay"] = ema_decay
    else:
        state.pop("standalone_ema_decay", None)
    if arena_fraction:
        state["standalone_arena_fraction"] = arena_fraction
    if reward_scheme != "current":
        state["standalone_reward_scheme"] = reward_scheme
    warmup = None
    if warmup_updates:
        from train.e4_stage12_value_warmup import advance_value_minibatch, warmup_act_metrics
        warmup = state.get("standalone_value_warmup")
        if warmup is None:
            warmup = state["standalone_value_warmup"] = dict(updates=warmup_updates, completed=0)
            for parameter in policy.value.parameters():
                optimizer.state.pop(parameter, None)
    previous_calibration = state.get("claude171_value_calibration")
    if previous_calibration and not math.isfinite(previous_calibration["ratio"]):
        state["claude171_value_calibration_pending"] = True
    fp16 = args.dtype == "fp16" if standalone else policy.encoder_fp16
    scaler = torch.amp.GradScaler(device.type, enabled=cuda and fp16)
    bootstrap = control is not None and state["checkpoint_serial"] == 0
    if common.exists() and scaler.is_enabled() and not bootstrap:
        scaler.load_state_dict(saved["scaler"])
    critic_optimizer = critic_scaler = critic_ema = None
    if critic is not None:
        critic.encoder_fp16 = policy.encoder_fp16
        critic_optimizer = torch.optim.Adam(critic.parameters(),
                                            lr=critic_rate_override or state["learning_rate"], eps=1e-5)
        critic_scaler = torch.amp.GradScaler(device.type, enabled=cuda and fp16)
        if saved_critic is not None:
            critic.load_state_dict(saved_critic["model"])
            critic_optimizer.load_state_dict(saved_critic["optimizer"])
            if critic_scaler.is_enabled():
                critic_scaler.load_state_dict(saved_critic["scaler"])
        if ema_decay:
            critic_ema = ExponentialWeights(critic, ema_decay, saved_critic.get("ema_weights") if saved_critic else None)
    state["world"] = world
    if state["stop_reason"] in ('allocation_end_or_yield', 'entropy_below_half_initial', 'KL_above_0.1'):
        state["stop_reason"] = None
    teacher = None
    training_branch = "claude165_768" if config.width == 768 else "dev78_rollback_pure_PPO"
    rollback_checkpoint = "dev-112.pt" if config.width == 768 else "dev-78.pt"
    if control is not None:
        training_branch = "claude186_384_current_ppo"
        rollback_checkpoint = None
    if "claude165_dev112_rollback" in state:
        training_branch = "claude165_dev112_rollback"
        rollback_checkpoint = "dev-112.pt"
    score_guided = scored_distillation(state)
    distillation_target = None if score_guided else state.get('distillation_target', 80)
    if score_guided:
        state.pop("distillation_target", None)
        state["claude166_distillation"] = dict(learning_rate=DISTILLATION_LEARNING_RATE,
            pass_mean_floor=DISTILLATION_PASS_FLOOR, report_round=DISTILLATION_REPORT_ROUND)
    if state.get('teacher_path'):
        teacher_config = state.get('teacher_model_config')
        teacher = CLSPolicy(ModelConfig(**teacher_config) if teacher_config else None).to(device)
        teacher.load_state_dict(torch.load(state['teacher_path'], map_location='cpu', weights_only=True))
        teacher.encoder_fp16 = not standalone
        if standalone:
            teacher.encoder_dtype_override = policy.encoder_dtype_override
        teacher.eval().requires_grad_(False)
    stopped_by_signal = False
    def on_signal(*_):
        nonlocal stopped_by_signal
        stopped_by_signal = True
    signal.signal(signal.SIGUSR1, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    def log(row):
        if rank == 0:
            print(json.dumps(row, allow_nan=True), flush=True)
            with (output / "training.jsonl").open("a") as stream:
                stream.write(json.dumps(row, allow_nan=True) + "\n")

    if enable_key_mask_on_resume:
        live_slots = [slot for slot, seed in enumerate(collector.seeds) if seed is not None]
        search.defer_key_last_chance_on_resume(live_slots)
        state["standalone_key_last_chance"] = True
        log(dict(event="key_last_chance_enabled_on_resume", update=state["update"],
                 message=f"key mask enabled on resume at update {state['update']}",
                 deferred_existing_act3_until_next_start=True))

    if late_pools:
        state["standalone_late_pools"] = late_settings
        needs_initialization = collector.late_initialization is None
        replay = collector.seed_external(Path(late_settings["roots"]))
        log(dict(event="late_pools_ready", update=state["update"], fraction=late_settings["fraction"],
                 initialized=needs_initialization, **replay))

    def stopping():
        batch_stopping = args.batch_stop_file is not None and args.batch_stop_file.exists()
        update_limit = stop_after_update is not None and state["update"] >= stop_after_update
        flag = torch.tensor(int(stopped_by_signal or stop_file.exists() or batch_stopping or update_limit or time.monotonic() >= deadline),
                            dtype=torch.int32, device=device)
        dist.all_reduce(flag, op=dist.ReduceOp.MAX)
        return bool(flag)

    def checkpoint():
        nonlocal committed_rank_path
        state["checkpoint_serial"] += 1
        identity = (state["update"], state["checkpoint_serial"])
        save_rank(rank_path, collector, cursor, identity, ppo_rng, round_state,
                  committed_path=committed_rank_path,
                  expert_state=expert_saved, retain_by_copy=standalone)
        dist.barrier()
        if rank == 0:
            temporary = output / "model.pt.tmp"
            contents = dict(model=move_tensors(policy.state_dict(), torch.device("cpu")),
                            optimizer=move_tensors(optimizer.state_dict(), torch.device("cpu")),
                            scaler=scaler.state_dict(), state=deepcopy(state), ppo_rng=ppo_rng.get_state())
            if ema is not None:
                contents["ema_weights"] = move_tensors(ema.weights, torch.device("cpu"))
            if critic is not None:
                contents["critic"] = dict(model=move_tensors(critic.state_dict(), torch.device("cpu")),
                    optimizer=move_tensors(critic_optimizer.state_dict(), torch.device("cpu")),
                    scaler=critic_scaler.state_dict(), learning_rate_override=critic_rate_override)
                if critic_ema is not None:
                    contents["critic"]["ema_weights"] = move_tensors(critic_ema.weights, torch.device("cpu"))
            torch.save(contents, temporary)
            temporary.replace(common)
            log(dict(event="checkpoint", update=state["update"], reason=state["stop_reason"]))
        dist.barrier()
        committed_rank_path = rank_path

    def evaluate(evaluated=None, label=None, *, ema_run=False):
        evaluated = evaluated or policy
        primary = not label and not ema_run
        gc.collect()
        torch.cuda.empty_cache()
        evaluator_search = NoSLSearch((1024 + world - 1) // world, device, 101 + rank, greedy=True,
                                      key_last_chance=key_last_chance)
        evaluator_search.deadline = deadline
        evaluator_search.evidence_directory = output
        evaluator = EvaluationLedgerCollector(evaluator_search, torch.Generator().manual_seed(101 + rank),
                                              rank=rank, extra_details=standalone, **collector_options)
        evaluated.eval()
        complete = True
        try:
            evaluator.evaluate(evaluated, list(range(2000000000, 2000001024))[rank::world])
        except AllocationEnded:
            complete = False
        episodes = [{k:e[k] for k in ("seed","floor","reach","keys","outcome","reward")} for e in evaluator.completed]
        prefix = label or ("evaluation-ema" if ema_run else "evaluation")
        filename = output / f"{prefix}-{state['update']}-rank-{rank}.json.gz"
        with gzip.open(filename, "wt") as stream:
            json.dump(episodes, stream)
        ledger_filename = output / f"{prefix}-{state['update']}-ledger-rank-{rank}.json.gz"
        with gzip.open(ledger_filename,"wt") as stream:json.dump([e["act2_ledger"] for e in evaluator.completed],stream)
        flag = torch.tensor(int(complete), device=device)
        dist.all_reduce(flag, op=dist.ReduceOp.MIN)
        dist.barrier()
        track_score = score_guided and not state.get("distillation_evaluated") and primary
        if track_score:
            score = torch.full((), float("-inf"), dtype=torch.float64, device=device)
        if rank == 0:
            all_episodes = []
            for owner in range(world):
                with gzip.open(output / f"{prefix}-{state['update']}-rank-{owner}.json.gz", "rt") as stream:
                    all_episodes.extend(json.load(stream))
            row = dict(event="teacher_evaluation" if label else "evaluation", selection="argmax", phase="distillation" if state.get("teacher_path") and not state.get("distillation_evaluated") else "PPO", update=state["update"], complete=bool(flag),
                training_branch=training_branch, world=world, rollback_checkpoint=rollback_checkpoint,
                       training_seconds=state["training_seconds"], no_SL=True,
                       learning_rate_cap=MAX_LEARNING_RATE, learning_rate_note="学习率封顶 4.44e-5",
                       observation_version=state['observation_version'], **metrics(all_episodes))
            if reward_scheme != "current":
                row["reward_scheme"] = reward_scheme
            if control is not None:
                row.update(reward_scheme=reward_scheme, init_checkpoint=control["init_checkpoint"],
                           cumulative_training_decisions=control["cumulative_decisions"])
            if ema_run and bool(flag) and (control is not None or critic is not None):
                path = output / f"dev-{int(state['training_seconds'] // 7200) * 2}-ema.pt"
                if not path.exists():
                    temporary = path.with_suffix(".pt.tmp")
                    torch.save(move_tensors(ema.weights, torch.device("cpu")), temporary)
                    temporary.replace(path)
                row["model_snapshot"] = path.name
            if ema is not None and not label:
                row["weight_source"] = "ema" if ema_run else "raw"
                row["ema_decay"] = ema_decay
            if track_score:
                row.update(distillation_learning_rate=DISTILLATION_LEARNING_RATE,
                           distillation_pass_mean_floor=DISTILLATION_PASS_FLOOR)
                mean_floor = passing_distillation_score(row)
                if mean_floor is not None:
                    score.fill_(mean_floor)
            if state.get("teacher_path"):
                row["paired_baseline_model"] = state.get("teacher_evaluated_model", "dev-16.pt") + " argmax"
            if label:
                row["evaluated_model"] = state.get("teacher_evaluated_model", "dev-16.pt")
                row["evaluated_model_training_hours"] = int(Path(row["evaluated_model"]).stem.split("-")[-1])
            baseline = output / "baseline.json.gz"
            if state["update"] == 0 and bool(flag) and not ema_run:
                with gzip.open(baseline, "wt") as stream:
                    json.dump(all_episodes, stream)
            if baseline.exists():
                with gzip.open(baseline, "rt") as stream:
                    row["paired_baseline"] = paired_summary(all_episodes, json.load(stream))
            if bool(flag) and primary:
                if (not score_guided and state.get('distillation_updates') == distillation_target
                        and not state.get('distillation_evaluated')):
                    name = f'distillation-{distillation_target}.pt'
                    if not (output / name).exists():
                        torch.save(move_tensors(policy.state_dict(), torch.device('cpu')), output / name)
                    row['model_snapshot'] = name
                else:
                    row["model_snapshot"] = save_dev_weights(policy, output, state["training_seconds"])
            if bool(flag) and not label and critic is not None:
                hour = int(state["training_seconds"] // 7200) * 2
                suffix = "-critic-ema" if ema_run else "-critic"
                path = output / f"dev-{hour}{suffix}.pt"
                if not path.exists():
                    temporary = path.with_suffix(".pt.tmp")
                    weights = critic_ema.weights if ema_run else critic.state_dict()
                    torch.save(move_tensors(weights, torch.device("cpu")), temporary)
                    temporary.replace(path)
                row["critic_snapshot"] = path.name
            ledger = []
            for owner in range(world):
                with gzip.open(output/f"{prefix}-{state['update']}-ledger-rank-{owner}.json.gz","rt") as stream:ledger.extend(json.load(stream))
            ledger_name=f"{prefix}-{state['update']}-act2-ledger.json"
            write_json(output/ledger_name,dict(episodes=ledger,stalled_games=sum(e["stalled"] for e in ledger)))
            row.update(value_spearman=value_correlations(ledger),act2_ledger_file=ledger_name)
            if standalone:
                row["exam_breakdown"] = exam_breakdown(ledger)
                if key_last_chance:
                    row["key_last_chance"] = True
            row.update(state.get("decision_entropy_metrics", {}))
            write_json(output / f"{prefix}-{state['update']}.json", row)
            log(row)
        if track_score:
            dist.broadcast(score, src=0)
            if bool(score >= DISTILLATION_PASS_FLOOR):
                state["claude166_passing_mean_floor"] = float(score)
        dist.barrier()
        del evaluator, evaluator_search
        gc.collect()
        torch.cuda.empty_cache()
        if bool(flag) and primary and control is None:
            if not lookahead60_window(output,config,device,rank,world,deadline):
                state['stop_reason'] = 'allocation_end_or_yield'
        if bool(flag) and primary and control is None and due_combat_comparison(state['training_seconds']):
            hour = int(state['training_seconds'] // 7200) * 2
            if state.get("claude168_defer_supplement"):
                if rank == 0:
                    write_json(output/f'expert-deferred-{hour}.json',
                               dict(hour=hour, model_config=asdict(config)))
                dist.barrier()
            else:
                write_json(output/f'expert-pending-{rank}.json', dict(hour=hour))
            if not combat_supplements():
                state['stop_reason'] = 'allocation_end_or_yield'
        return bool(flag)

    def evaluate_training():
        if control is None and critic is None:
            return evaluate() and (ema is None or evaluate(ema, ema_run=True))
        for weight_source, evaluated in (("raw", policy), ("ema", ema)):
            if evaluated is None:
                continue
            is_ema = weight_source == "ema"
            prefix = "evaluation-ema" if is_ema else "evaluation"
            result = output / f"{prefix}-{state['update']}.json"
            if result.is_file() and json.loads(result.read_text())["complete"]:
                continue
            if not evaluate(evaluated, ema_run=is_ema):
                return False
        return True

    def combat_supplements():
        paused = state.get("claude168_defer_supplement") and scored_distillation(state)
        return run_combat_supplements(output, config, device, rank, world, deadline,
                                      paused=paused)

    def regular_evaluation():
        nonlocal teacher
        if not evaluate_training():
            state["stop_reason"] = "allocation_end_or_yield"
            return
        state["next_evaluation"] += 7200.
        if control is not None or critic is not None:
            checkpoint()
        if control is not None:
            if due_combat_comparison(state["training_seconds"]):
                hour = int(state["training_seconds"] // 7200) * 2
                write_json(output / f"expert-pending-{rank}.json", dict(hour=hour))
                if not combat_supplements():
                    state["stop_reason"] = "allocation_end_or_yield"
        if score_guided and "claude166_passing_mean_floor" in state and teacher is not None:
            state["distillation_evaluated"] = True
            state["claude171_value_calibration_pending"] = True
            state["learning_rate"] = MAX_LEARNING_RATE
            state["high_kl_streak"] = 0
            teacher = None
            checkpoint()
            log(dict(event="distillation_complete", update=state["update"],
                     rounds=state["distillation_updates"], training_seconds=state["training_seconds"],
                     world=world, mean_floor=state["claude166_passing_mean_floor"],
                     pass_mean_floor=DISTILLATION_PASS_FLOOR, PPO_learning_rate=MAX_LEARNING_RATE))
            if state.get("claude168_defer_supplement") and not combat_supplements():
                state["stop_reason"] = "allocation_end_or_yield"

    def report_distillation_limit():
        if distillation_limit_due(state):
            state["claude166_round_limit_reported"] = True
            log(dict(event="distillation_round_limit", update=state["update"],
                     rounds=state["distillation_updates"], training_seconds=state["training_seconds"],
                     pass_mean_floor=DISTILLATION_PASS_FLOOR, continues_distillation=True))

    startup_minibatch = (cursor.get("global_minibatch", PPO_GLOBAL_MINIBATCH)
                         if cursor is not None else new_round_minibatch(state))
    log(dict(event="start", world=world, lanes_per_gpu=args.lanes, length=args.rollout_length, global_minibatch=startup_minibatch,
             epochs=2, model_parameters=sum(p.numel() for p in policy.parameters()),
             model=dict(**asdict(config),pooling="CLS"), teacher=state.get("teacher_path"), distillation_updates=state.get("distillation_updates"),
             no_SL=True, from_scratch=not common.exists(), observation_version=state['observation_version']))
    if control is not None:
        log(dict(event="claude186_control_active", update=state["update"], world=world,
                 reward_scheme=reward_scheme, warmup_updates=warmup_updates, auxiliary_heads=bool(auxiliary_weights),
                 arena_fraction=arena_fraction, training_seconds=state["training_seconds"],
                 learning_rate=state["learning_rate"], next_evaluation=state["next_evaluation"], **control))
    if state.get("claude167_retry"):
        log(dict(event="claude167_retry_active", update=state["update"], world=world,
                 training_seconds=state["training_seconds"], microbatch=state["microbatch"],
                 allocator=os.environ.get("PYTORCH_CUDA_ALLOC_CONF"), encoder_recompute="full"))
    if state.get("claude168_defer_supplement") and score_guided:
        log(dict(event="claude168_supplement_paused", update=state["update"],
                 training_seconds=state["training_seconds"], world=world))
    if score_guided:
        log(dict(event="claude166_distillation_active", update=state["update"],
                 training_seconds=state["training_seconds"], world=world,
                 retained_partial_optimizer_steps=cursor["optimizer_steps"] if cursor is not None else 0,
                 next_new_round_global_minibatch=DISTILLATION_GLOBAL_MINIBATCH,
                 **state["claude166_distillation"]))
    profile_path = output / "claude127-learner-profile.json"
    learner_profile = (TwoMicrobatchProfile(profile_path, state["update"] + 1, log)
                       if rank == 0 and cuda and common.exists() and not profile_path.exists() else None)
    midstart = midstart_policy([len(pool) for pool in collector.pools])
    log(dict(event="midstart_policy", **midstart))
    if standalone:
        warming_at_start = warmup is not None and warmup["completed"] < warmup["updates"]
        arena = None
        if arena_fraction:
            pools = {kind: len(pool) for kind, pool in collector.arena_pools.items()}
            arena = dict(fraction=arena_fraction, lanes=collector.arena_lanes,
                         active_lanes=collector.arena_lanes if any(pools.values()) else 0,
                         pool_sizes=pools)
        critic_config = {"key_last_chance": True} if key_last_chance else {}
        if critic is not None:
            critic_config["critic"] = dict(learning_rate=critic_rate_override or state["learning_rate"],
                                           adam="restored" if saved_critic else "fresh")
        if create_critic_on_resume:
            log(dict(event="critic_created_on_resume", update=state["update"]))
        log(dict(event="effective_config", update=state["update"], source_commit=source_commit(),
                 lanes=args.lanes, microbatch=state["microbatch"], rollout_length=args.rollout_length,
                 gae_lambda=GAE_LAMBDA, gamma=GAE_GAMMA,
                 learning_rate=state["learning_rate"],
                 learning_rate_cap=DISTILLATION_LEARNING_RATE if score_guided else MAX_LEARNING_RATE,
                 entropy_coefficient=state.get("entropy_coefficient", DEFAULT_ENTROPY_COEFFICIENT),
                 entropy_active=not warming_at_start and not score_guided,
                 value=startup_value_setting(state, warming_at_start), ema_decay=ema_decay,
                 arena=arena, reward_scheme=reward_scheme, compile_encoder=args.compile,
                 recompute=policy.encoder_checkpoint_enabled, precision=state["precision"],
                 midstart=midstart, adam="restored" if saved else "fresh", **critic_config))
    last_checkpoint = time.monotonic()
    if bootstrap or not common.exists():
        checkpoint()
    teacher_label = state.get('teacher_label', 'teacher-dev16-argmax')
    teacher_baseline = output / f'{teacher_label}-0.json'
    if teacher is not None and (not teacher_baseline.exists() or not json.loads(teacher_baseline.read_text())['complete']):
        if not evaluate(teacher, teacher_label):
            state['stop_reason'] = 'allocation_end_or_yield'
    if (not score_guided and teacher is not None and state['distillation_updates'] == distillation_target
            and not state.get('distillation_evaluated') and state['stop_reason'] is None):
        if evaluate_training():
            state['distillation_evaluated'] = True
            state['learning_rate'] = MAX_LEARNING_RATE if config.width == 768 else 1e-4
            state['high_kl_streak'] = 0
            if config.width == 768 and state['training_seconds'] >= state['next_evaluation']:
                state['next_evaluation'] += 7200.
            checkpoint()
            if config.width == 768:
                log(dict(event='distillation_complete', update=state['update'], rounds=distillation_target,
                         training_seconds=state["training_seconds"], world=world,
                         PPO_learning_rate=state['learning_rate']))
        else:
            state['stop_reason'] = 'allocation_end_or_yield'
    if teacher is not None and state.get('distillation_evaluated'):
        del teacher
        teacher = None
    del saved, saved_critic
    initial_evaluation_due = not (output / "baseline.json.gz").exists()
    if control is not None or (critic is not None and ema is not None):
        initial_evaluation_due |= not (output / "evaluation-ema-0.json").is_file()
        if not initial_evaluation_due:
            initial_evaluation_due = not json.loads((output / "evaluation-ema-0.json").read_text())["complete"]
    if teacher is None and state["update"] == 0 and initial_evaluation_due:
        if not evaluate_training():
            state["stop_reason"] = "allocation_end_or_yield"
    if state['stop_reason'] is None and not lookahead60_window(output,config,device,rank,world,deadline):
        state['stop_reason'] = 'allocation_end_or_yield'
    if state["stop_reason"] is None and not combat_supplements():
        state["stop_reason"] = "allocation_end_or_yield"
    if ((score_guided or control is not None or critic is not None) and cursor is None and state["stop_reason"] is None
            and state["training_seconds"] >= state["next_evaluation"]):
        regular_evaluation()
    report_distillation_limit()
    resumed_learner = cursor is not None
    while state["stop_reason"] is None:
        if stopping():
            state["stop_reason"] = "allocation_end_or_yield"
            break
        started = time.monotonic()

        def checkpoint_if_due():
            nonlocal last_checkpoint, started
            now = time.monotonic()
            if now - last_checkpoint >= 1800:
                round_state["seconds"] += now - started
                checkpoint()
                last_checkpoint = started = time.monotonic()

        distilling = teacher is not None and (score_guided or state["distillation_updates"] < distillation_target)
        warming = warmup is not None and warmup["completed"] < warmup["updates"]
        if cursor is None:
            if collector.partial is None and round_state["ready"] is None:
                round_state = dict(ready=None, seconds=0., decisions_start=collector.decisions)
                if cuda:
                    torch.cuda.reset_peak_memory_stats(device)
                search.entropy_sum.zero_()
                search.entropy_count = 0
                search.decision_entropy.zero_()
                search.decision_nonforced.zero_()
                search.times = StageTimes()
                search.fault_counts.clear()
                collector.round_games = sum(seed is not None for seed in collector.seeds)
            interrupted = False
            if round_state["ready"] is None:
                policy.eval()
                if critic is not None:
                    critic.eval()
                try:
                    round_state["ready"] = collector.collect(teacher if distilling else policy, length=args.rollout_length,
                        after_step=checkpoint_if_due if standalone else None,
                        auxiliary_weights=None if warming else auxiliary_weights, track_value_acts=warming)
                except AllocationEnded:
                    interrupted = True
            interrupted = torch.tensor(int(interrupted), device=device)
            dist.all_reduce(interrupted, op=dist.ReduceOp.MAX)
            if bool(interrupted):
                round_state["seconds"] += time.monotonic() - started
                state["stop_reason"] = "allocation_end_or_yield"
                break
            if distilling:
                policy.eval()
                student_log_baseline(policy, round_state['ready'])
            if not state.get("value_rollout_diagnostic_complete"):
                types = round_state["ready"].get("decision_types")
                valid_types = torch.tensor(int(types is not None and bool((types >= 0).all())),device=device)
                dist.all_reduce(valid_types,op=dist.ReduceOp.MIN)
                if bool(valid_types):
                    ev = explained_variance(round_state["ready"])
                    entropy = search.decision_nonforced.clone()
                    dist.all_reduce(entropy)
                    report = dict(event="value_rollout_diagnostic",update=state["update"]+1,explained_variance=ev,nonforced_entropy=nonforced_entropy(entropy,["combat","card_reward","map","shop","campfire","other"]))
                    log(report)
                    if rank == 0:write_json(output/"claude132-rollout-diagnostic.json",report)
                    state["value_rollout_diagnostic_complete"] = True
            cursor = new_cursor(round_state["ready"], ppo_rng, world,
                                global_minibatch=new_round_minibatch(state))
            round_state["ready"] = None
            if not sum(cursor["counts"]):
                failed = torch.tensor([sum(search.fault_counts.values()), collector.round_games], device=device, dtype=torch.int64)
                dist.all_reduce(failed)
                faults, games = failed.tolist()
                state['stop_reason'] = health_stop(state, dict(kl=0., normalized_entropy=1., finite=True,
                    amp_scale=scaler.get_scale(), amp_skipped_steps=0, optimizer_attempts=0), faults, games)
                log(dict(event="no_valid_rows", update=state["update"], episode_faults=faults,
                         episode_cases=games, stop_reason=state['stop_reason']))
                cursor = None
                continue
            if state["initial_entropy"] is None:
                ent = torch.stack((search.entropy_sum, search.entropy_sum.new_tensor(search.entropy_count)))
                dist.all_reduce(ent)
                state["initial_entropy"] = float(ent[0] / ent[1].clamp_min(1))
            if state["precision"] is None:
                probe = precision_probe(policy, cursor, rank)
                state["precision"] = probe["precision"]
                scaler = torch.amp.GradScaler("cuda", enabled=policy.encoder_fp16)
                log(dict(event="precision_probe", update=state["update"], **probe))
            if state["microbatch"] is None:
                model.train()
                state["microbatch"] = choose_microbatch(model, cursor)
                log(dict(event="microbatch", value=state["microbatch"]))
        model.train(not warming)
        if critic is not None:
            critic.train(not warming)
            for group in critic_optimizer.param_groups:
                group["lr"] = critic_rate_override or state["learning_rate"]
        recompute = None
        if config.width == 768 and not standalone:
            recompute = prepare_round(policy, cursor, state, world, resumed_learner)
        resumed_learner = False
        for group in optimizer.param_groups:
            group["lr"] = state["learning_rate"]
        while cursor["epoch"] < 2:
            if stopping():
                state["stop_reason"] = "allocation_end_or_yield"
                break
            with search.times.measure("learner"):
                previous_steps = cursor["optimizer_steps"]
                critic_stepped = False
                if warming:
                    if critic is None:
                        advance_value_minibatch(policy, optimizer, scaler, cursor, ppo_rng, state["microbatch"])
                    else:
                        advance_value_minibatch(critic, critic_optimizer, critic_scaler, cursor, ppo_rng,
                                                state["microbatch"], split=True)
                        critic_stepped = cursor["optimizer_steps"] > previous_steps
                else:
                    critic_stepped = advance_minibatch(model, optimizer, scaler, cursor, ppo_rng, rank, world,
                        state["microbatch"], teacher=teacher if distilling else None,
                        entropy_coefficient=state.get('entropy_coefficient', DEFAULT_ENTROPY_COEFFICIENT),
                        learner_profile=learner_profile,value_state=state,diagnostic_log=log,
                        auxiliary_weights=auxiliary_weights,decision_diagnostics=standalone,
                        critic=critic,critic_optimizer=critic_optimizer,critic_scaler=critic_scaler)
                if ema is not None and (not warming or critic is None) and cursor["optimizer_steps"] > previous_steps:
                    ema.update()
                if critic is not None:
                    cursor["critic_optimizer_steps"] = cursor.get("critic_optimizer_steps", 0) + int(critic_stepped)
                    if critic_ema is not None and critic_stepped:
                        critic_ema.update()
                if rank == 0 and "value_calibration" in state and not (output/"claude133-gradient-diagnostic.json").exists():
                    write_json(output/"claude133-gradient-diagnostic.json",dict(update=state["update"]+1,**state["value_calibration"]))
            if standalone:
                checkpoint_if_due()
        sync_started = time.perf_counter()
        synchronize(device)
        search.times.seconds["learner"] += time.perf_counter() - sync_started
        round_state["seconds"] += time.monotonic() - started
        if state["stop_reason"] is not None:
            break
        row = finish_metrics(cursor, critic if warming and critic is not None else model,
                             critic_scaler if warming and critic is not None else scaler)
        if critic is not None:
            row["critic_optimizer_steps"] = cursor.get("critic_optimizer_steps", 0)
            row["critic_learning_rate"] = critic_optimizer.param_groups[0]["lr"]
            row["finite"] &= bool(torch.stack([torch.isfinite(p).all() for p in critic.parameters()]).all())
        if "auxiliary_metrics" in cursor["fragment"]:
            row["auxiliary"] = cursor["fragment"]["auxiliary_metrics"]
        if arena_fraction:
            row["arena"] = collector.take_arena_metrics()
        elapsed = torch.tensor(round_state["seconds"], dtype=torch.float64, device=device)
        dist.all_reduce(elapsed, op=dist.ReduceOp.MAX)
        row.update(expert_beta=0., expert_samples=0, expert_collection=False,
                   training_branch=training_branch, world=world,
                   rollback_checkpoint=rollback_checkpoint)
        decisions = torch.tensor(collector.decisions - round_state["decisions_start"], device=device, dtype=torch.float64)
        by_rank = [torch.empty_like(decisions) for _ in range(world)]
        dist.all_gather(by_rank, decisions)
        fresh_done = [e for e in collector.completed if e.get("start_kind", "fresh") == "fresh"]
        middle_done = [e for e in collector.completed if e.get("start_kind", "fresh") != "fresh"]
        done = torch.tensor([len(fresh_done),sum(e["floor"] for e in fresh_done),
                            sum(e["reach"]["act1_boss"] for e in fresh_done),
                            len(middle_done),sum(e["floor"] for e in middle_done)],device=device,dtype=torch.float64)
        dist.all_reduce(done)
        parts = torch.tensor(list(search.times.seconds.values()),device=device,dtype=torch.float64)
        dist.all_reduce(parts, op=dist.ReduceOp.MAX)
        per_gpu = [float(n) / max(.001,float(elapsed)) for n in by_rank]
        global_decisions = int(sum(float(n) for n in by_rank))
        peaks = [torch.cuda.max_memory_allocated(device), torch.cuda.max_memory_reserved(device)] if cuda else [0, 0]
        if control is not None:
            control["cumulative_decisions"] += global_decisions
            row["cumulative_training_decisions"] = control["cumulative_decisions"]
            global_peaks = torch.tensor(peaks, dtype=torch.int64, device=device)
            dist.all_reduce(global_peaks, op=dist.ReduceOp.MAX)
            peaks = global_peaks.tolist()
        row["value_loss_coefficient"] = 1. if warming else value_coefficient(state)
        if reward_scheme != "current":
            row["reward_scheme"] = reward_scheme
        if warming:
            row["value_warmup"] = dict(round=warmup["completed"] + 1, target=warmup["updates"],
                                       by_act=warmup_act_metrics(cursor))
            entropy_sum, entropy_count = search.decision_entropy.sum(dim=1).tolist()
            row["normalized_entropy"] = entropy_sum / max(1., entropy_count)
        if recompute is not None:
            recompute = finish_round(recompute, device, float(parts[-1]), float(elapsed))
            if state["update"] < TRIAL_ROUNDS:
                state.setdefault("claude165_recompute", []).append(recompute)
            row["encoder_recompute"] = recompute
        if not warming and "value_calibration" in state:
            state["value_calibration"]["completed_updates"] += 1
        state["update"] += 1
        state["training_seconds"] += float(elapsed)
        if not distilling and not warming:
            calibration_event = record_ppo_calibration_round(state, row)
            if calibration_event is not None:
                log(calibration_event)
        health = torch.tensor([sum(search.fault_counts.values()), collector.round_games], device=device, dtype=torch.int64)
        dist.all_reduce(health)
        faults, games = health.tolist()
        rss = torch.tensor(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024, device=device, dtype=torch.int64)
        rss_by_rank = [torch.empty_like(rss) for _ in range(world)]
        dist.all_gather(rss_by_rank, rss)
        state['stop_reason'] = health_stop(state, row, faults, games)
        if distilling:
            state['distillation_updates'] += 1
            if score_guided:
                # finish_metrics already averages these losses over valid examples on all ranks.
                row.update(distillation_kl=row["policy_loss"], distillation_value_mse=row["value_loss"])
        elif not warming:
            state["learning_rate"] = next_learning_rate(state["learning_rate"], row["kl"])
        entropies = search.decision_entropy.clone()
        dist.all_reduce(entropies)
        state["decision_entropy_metrics"] = decision_entropy_summary(entropies)
        row["combat_entropy"] = state["decision_entropy_metrics"]["entropy_by_decision_type"]["combat"]
        if row['normalized_entropy'] < .1:
            log(dict(event='low_normalized_entropy_warning', update=state['update'], normalized_entropy=row['normalized_entropy']))
        if row['kl'] > .1:
            log(dict(event='high_KL_warning', update=state['update'], kl=row['kl'], streak=state['high_kl_streak']))
        if standalone:
            row["training_milestones"] = record_training_milestones(state, fresh_done, middle_done)
            row["decision_training"] = decision_training_summary(cursor)
            if late_pools:
                by_start = collector.late_round_metrics()
                if world > 1:
                    totals = torch.tensor([list(counts.values()) for counts in by_start.values()],
                                          dtype=torch.int64, device=device)
                    dist.all_reduce(totals)
                    by_start = {name: dict(zip(counts, values))
                                for (name, counts), values in zip(by_start.items(), totals.tolist())}
                row["training_by_start"] = by_start
        phase = "value_warmup" if warming else "distillation" if distilling else "PPO"
        log(dict(event="update",phase=phase,distillation_updates=state.get("distillation_updates"),update=state["update"],seconds=float(elapsed),training_seconds=state["training_seconds"],
            mainline_decisions_global=global_decisions,decisions_per_second_per_gpu=per_gpu,
            decisions_per_second_global=sum(per_gpu),training_mean_floor=float(done[1]/done[0]) if float(done[0]) else None,
            training_act1_boss_pass_rate=float(done[2]/done[0]) if float(done[0]) else None,
            training_fresh_completed=int(done[0]),training_midstart_completed=int(done[3]),
            training_midstart_mean_floor=float(done[4]/done[3]) if float(done[3]) else None,
            midstart_pool_sizes=[len(p) for p in collector.pools],
            midstart_randomization='whole_run_159',
            cuda_peak_allocated_bytes=peaks[0],cuda_peak_reserved_bytes=peaks[1],
            timing_seconds=dict(zip(search.times.seconds,parts.tolist())),initial_entropy=state["initial_entropy"],
            precision=state["precision"],microbatch=state["microbatch"],
            global_minibatch=cursor.get("global_minibatch", PPO_GLOBAL_MINIBATCH),
            learning_rate=optimizer.param_groups[0]["lr"],
            next_learning_rate=state["learning_rate"],observation_version=state['observation_version'],**state["decision_entropy_metrics"],
            stop_reason=state["stop_reason"],episode_faults=faults,episode_cases=games,
            episode_fault_rate=faults/max(1,games),rss_peak_bytes_per_rank=[int(v) for v in rss_by_rank],**row))
        if state.get('model_config', {}).get('width') == 384 and state.get('distillation_updates') == 1 and float(elapsed) > 4 * state['small_round_seconds'] and state['stop_reason'] is None:
            state['architecture_retry_width'] = 320
            state['stop_reason'] = 'allocation_end_or_yield'
            log(dict(event='architecture_retry_required', measured_seconds=float(elapsed), small_seconds=state['small_round_seconds'], next_width=320, reason='more_than_four_times_small_model'))
        cursor = None
        collector.completed.clear()
        round_state = dict(ready=None,seconds=0.,decisions_start=collector.decisions)
        if warming:
            warmup["completed"] += 1
            if warmup["completed"] == warmup["updates"]:
                if critic is None:
                    state["claude171_value_calibration_pending"] = True
                checkpoint()
                log(dict(event="value_warmup_complete", update=state["update"],
                         rounds=warmup["completed"], remeasure_value_coefficient=critic is None))
        distillation_finished = distilling and not score_guided and state['distillation_updates'] == distillation_target
        if distillation_finished and state['stop_reason'] is None:
            checkpoint()
            if evaluate_training():
                state['distillation_evaluated'] = True
                state['learning_rate'] = MAX_LEARNING_RATE if config.width == 768 else 1e-4
                state['high_kl_streak'] = 0
                del teacher
                teacher = None
                if state['training_seconds'] >= state['next_evaluation']:
                    state['next_evaluation'] += 7200.
                checkpoint()
                log(dict(event='distillation_complete', update=state['update'], rounds=distillation_target,
                         training_seconds=state["training_seconds"], world=world,
                         PPO_learning_rate=state['learning_rate']))
            else:
                state['stop_reason'] = 'allocation_end_or_yield'
        if state["training_seconds"] >= state["next_evaluation"] and state["stop_reason"] is None:
            if standalone or control is not None:
                checkpoint()
                last_checkpoint = time.monotonic()
            regular_evaluation()
        report_distillation_limit()
        if time.monotonic() - last_checkpoint >= 1800 and state["stop_reason"] is None:
            checkpoint()
            last_checkpoint = time.monotonic()
    checkpoint()
    log(dict(event="stopped",update=state["update"],reason=state["stop_reason"],training_seconds=state["training_seconds"]))
    dist.destroy_process_group()


if __name__ == "__main__":
    main()
