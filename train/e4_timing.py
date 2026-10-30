from functools import wraps
import time

import torch


def forward_autocast(device):
    # Volta/V100 has no BF16 tensor cores; retain FP32 on that hardware.
    enabled = device.type == 'cuda' and torch.cuda.get_device_capability(device)[0] >= 8
    return torch.autocast(device_type=device.type, dtype=torch.bfloat16, enabled=enabled)


class LabelTimer:
    def __init__(self):
        self.enabled = False
        self.seconds = dict.fromkeys(('learning', 'repeat_check', 'combat_sl', 'event_sl', 'mainline'), 0.)
        self.counts = dict.fromkeys(self.seconds, 0)

    def measure(self, label):
        def decorate(function):
            @wraps(function)
            def measured(*args, **kwargs):
                if not self.enabled:
                    return function(*args, **kwargs)
                torch.cuda.synchronize()
                started = time.perf_counter()
                try:
                    return function(*args, **kwargs)
                finally:
                    torch.cuda.synchronize()
                    self.seconds[label] += time.perf_counter() - started
                    self.counts[label] += 1
            return measured
        return decorate

    def report(self, wall_seconds, complete):
        total = sum(self.seconds.values())
        return dict(complete_update=complete, measured_wall_seconds=wall_seconds,
                    labels={key: dict(seconds=value, count=self.counts[key],
                                      fraction_of_label_sum=value / total if total else 0.,
                                      fraction_of_update_wall=value / wall_seconds)
                            for key, value in self.seconds.items()},
                    label_seconds_sum=total,
                    scope='Synchronized inclusive wall timings; nested labels overlap. Label sum is not elapsed time or GPU utilization. Entry synchronization is outside each label; exit synchronization is included. Update wall includes instrumentation overhead.')


timing = LabelTimer()
