# E4 #174 weight transfer

Windows authenticated to GitHub and confirmed repository write access. The requested `colab-1` tag had no existing release. The cluster CPU audit loaded each snapshot with `weights_only=True` and found only named tensors.

| Snapshot | Bytes | SHA-256 |
| --- | ---: | --- |
| `dev-130.pt` | 361658543 | `8519c0940ff81fbd58743f781f7184628ff75f1df2ab4ca0c9070a35222a896c` |
| `dev-112.pt` | 49768265 | `5d6fc0ded5a10c522fc90a1844206109a205497f4c6b7c3f09ddb569828a9e45` |

The authenticated temporary transfer endpoints were unreachable between the cluster and Windows in both directions. The snapshots are still on the cluster. The active SSH session and training continue. The release URL is pending the Windows file transfer. The later `colab-2` export is tied to the natural transition from 768-wide distillation to PPO.
