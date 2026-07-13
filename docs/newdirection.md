1. Fixed-size batch kernels
   Directly reduces per-record comparison/update cost.

2. Padding-safe compact state guidance
   Prevents false changes and can remove memset overhead.

3. Dirty bitset output
   Helps moderate/high-change workloads and downstream set operations.

4. Changed-range output
   Helps clustered changes and row/chunk-style consumers.

5. SoA stream diff
   Big potential for ECS/particles/simulation, but more API complexity.

6. Two-phase diff/consume
   Useful once multiple output formats exist.

7. Preview/commit
   Useful for transactional systems, less relevant to renderer speed now.