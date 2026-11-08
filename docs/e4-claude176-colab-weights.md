# E4 #176 Colab weights

The cluster published `colab-weights` at orphan commit `f33622244119802eb7b5beee35c3b8c2618b9b79`. GitHub returned the expected tree `2a2217194d7ccc75cf3370fa0e4dc30951022471`, with zero parents, ten weight chunks and one checksum manifest.

| Snapshot | Chunks | Bytes | SHA-256 |
| --- | ---: | ---: | --- |
| `dev-130.pt` | 8 | 361658543 | `8519c0940ff81fbd58743f781f7184628ff75f1df2ab4ca0c9070a35222a896c` |
| `dev-112.pt` | 2 | 49768265 | `5d6fc0ded5a10c522fc90a1844206109a205497f4c6b7c3f09ddb569828a9e45` |

On Linux or Colab, fetch the weights and reconstruct both original files:

```bash
git clone --depth 1 --single-branch --branch colab-weights \
  https://github.com/destire-mio/sts-silent-gpu.git e4-colab-weights
cd e4-colab-weights
cat dev-130.pt.part-* > dev-130.pt
cat dev-112.pt.part-* > dev-112.pt
sha256sum -c SHA256SUMS
```

The cluster checked both reconstructed byte streams against the audit hashes before pushing. GitHub readback matched all eleven file names, sizes and blob IDs. The temporary repository deploy key was revoked after the push. The next approved snapshot is the natural 768-wide transition to PPO.
