"""One diagnostic on rank zero's first two resumed learner microbatches."""
import json
import os
import torch


class TwoMicrobatchProfile:
    def __init__(self, path, update, log):
        self.path, self.update, self.log = path, update, log
        self.profiler = None
        self.masks = []
        self.count = 0
        self.done = False

    def failure(self, error):
        self.done = True
        # Profiling is observational and must not add a training health stop.
        if self.profiler is not None:
            try:
                self.profiler.stop()
            except Exception:
                pass
        self.profiler = None
        self.masks.clear()
        self.log(dict(event="learner_profile_error", update=self.update,
                      error=repr(error)))

    def before_microbatch(self):
        if self.done or self.profiler is not None:
            return
        try:
            self.profiler = torch.profiler.profile(
                activities=[torch.profiler.ProfilerActivity.CPU,
                            torch.profiler.ProfilerActivity.CUDA],
                record_shapes=False, profile_memory=False, with_stack=False)
            self.profiler.start()
        except Exception as error:
            self.failure(error)

    def observe(self, mask):
        if not self.done and self.profiler is not None:
            self.masks.append(mask.detach())

    def after_microbatch(self):
        if self.done or self.profiler is None:
            return
        try:
            self.count += 1
            self.profiler.step()
            if self.count < 2:
                return
            self.profiler.stop()
            events = self.profiler.key_averages()
            operators = [dict(name=e.key, calls=e.count,
                self_cpu_us=e.self_cpu_time_total,
                self_cuda_us=getattr(e, "self_device_time_total", 0.)) for e in events]
            operators.sort(key=lambda e: e["self_cuda_us"], reverse=True)
            batches = []
            for index, mask in enumerate(self.masks):
                valid = int(mask.sum().item())
                samples, padded = mask.shape
                batches.append(dict(microbatch=index + 1, samples=samples,
                    mean_entities=valid / samples, padded_entities=padded,
                    padding_ratio=1 - valid / mask.numel(),
                    mean_encoder_tokens=valid / samples + 1,
                    padded_encoder_tokens=padded + 1))
            report = dict(event="learner_profile", instruction=127, rank=0,
                job_id=os.environ.get("SLURM_JOB_ID"), update=self.update,
                microbatches=self.count, scope="packing, forward and backward; optimizer step excluded",
                units="microseconds", operator_sort="self CUDA time",
                top_operators=operators[:10], batches=batches)
            temporary = self.path.with_suffix(".json.tmp")
            temporary.write_text(json.dumps(report, allow_nan=False) + "\n")
            temporary.replace(self.path)
            self.done = True
            self.profiler = None
            self.masks.clear()
            self.log(report)
        except Exception as error:
            self.failure(error)
