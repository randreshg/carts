# opt/epoch/ -- Epoch Optimization Passes

Passes that optimize epoch synchronization barriers and CPS chains.

| Pass | File | Purpose |
|------|------|---------|
| EpochOpt | EpochOpt.cpp | Main epoch optimization driver |
| EpochOptStructural | EpochOptStructural.cpp | Structural epoch transformations |
| EpochOptScheduling | EpochOptScheduling.cpp | Epoch scheduling optimizations |
| EpochOptCpsChain | EpochOptCpsChain.cpp | CPS continuation chain formation |

`EpochOptSupport.cpp` contains shared helpers for the split.
