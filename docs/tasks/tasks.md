TASKS
During testing we need to update the 'analysis.md' of each example and verify the diagnosis information.
2. Work on jacobi-for.c example and make sure it is working.
   - We should make it work on differnet nodes with:
     - Element wise partitioning
     - Chunk wise partitioning
     - Stencil partitioning

Run 'carts examples --all' on single rank, then verify on multiple ranks.