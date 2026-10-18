import hashlib
import json

import torch


SCHEME = "SHA256(root seed,prefix) key; Philox4x32-10(seed,attempt,decision); uint32 inverse CDF"
MASK = 0xffffffff


def root_key(root):
    payload = json.dumps([root["seed"], root["candidates"]], separators=(",", ":")).encode()
    return hashlib.sha256(payload).hexdigest()


def root_words(keys, device):
    return torch.tensor([[int(key[i:i + 8], 16) for i in (0, 8)] for key in keys],
                        dtype=torch.int64, device=device).reshape(-1, 2)


def multiply(a, b):
    low = (a & 65535) * b
    high = (a >> 16) * b + (low >> 16)
    return high >> 16, ((high << 16) | (low & 65535)) & MASK


def random_words(seed, keys, attempts, decisions):
    c0, c1 = decisions & MASK, (decisions >> 32) & MASK
    c2, c3 = attempts & MASK, (attempts >> 32) & MASK
    k0, k1 = keys[:, 0] ^ (seed & MASK), keys[:, 1] ^ ((seed >> 32) & MASK)
    for _ in range(10):
        hi0, lo0 = multiply(c0, 0xD2511F53)
        hi1, lo1 = multiply(c2, 0xCD9E8D57)
        c0, c1, c2, c3 = hi1 ^ c1 ^ k0, lo1, hi0 ^ c3 ^ k1, lo0
        k0, k1 = (k0 + 0x9E3779B9) & MASK, (k1 + 0xBB67AE85) & MASK
    return c0


def uniforms(seed, keys, attempts, decisions):
    return (random_words(seed, keys, attempts, decisions).double() + 0.5) / 4294967296


def sample(policy, uniform):
    cumulative = policy.probs.double().cumsum(-1)
    cumulative = cumulative / cumulative[:, -1:]
    return (uniform[:, None] >= cumulative).sum(-1).to(torch.int32)
