# MTLCompilerService Sample Issues (Picorv32 Async Pipeline)

These notes summarize the sampled compiler stacks from:
- `Sample of MTLCompilerService.txt` (pre-async pipeline)
- `Sample of MTLCompilerServiceB.txt` .. `Sample of MTLCompilerServiceF.txt`
  (async pipeline, ~1 min intervals)

Footprint growth across samples:
- A: 350M (peak 500M)
- B: 438M (peak 500M)
- C: 1.2G (peak 1.2G)
- D: 1.9G (peak 1.9G)
- E: 2.7G (peak 2.7G)
- F: 3.3G (peak 3.3G)

## Issues to Hone In On

1) CFG simplification churn (pre-async phase)
- Evidence: `Sample of MTLCompilerService.txt` shows
  `SimplifyCFGPass`, `FoldBranchToCommonDest`, and heavy
  `BasicBlock::instructionsWithoutDebug`.
- Likely cause: large branch diamonds and dense control flow in the
  sched-vm kernels before the async pipeline.
- Focus: reduce large branch ladders and repeated inline conditionals,
  keep helpers `noinline`, and avoid large inlined switch/if chains.

2) PHI node explosion and removal (async pipeline)
- Evidence: `PHINode::removeIncomingValue` dominates in B..F
  (thousands of samples in C), with `_platform_memmove` spikes.
- Likely cause: large SSA merge points created by deep CFG duplication
  (many branches, many join points).
- Focus: reduce the number of merge points by lowering control flow
  to table lookups and by minimizing ternary-style expansions.

3) Basic block cloning / CFG duplication (async pipeline)
- Evidence: `CloneBasicBlock` + `Instruction::clone` in B, C, E, F.
- Likely cause: tail duplication / inlining in AGX backend passes.
- Focus: keep large helpers `noinline`, break huge kernels into smaller
  helpers, avoid emitting repeated inline blocks with slight variants.

4) Value symbol table / metadata churn (async pipeline)
- Evidence: `ValueSymbolTable::reinsertValue`, `MetadataTracking::track`,
  and `SmallVectorBase::grow_pod` appear in B/F/E.
- Likely cause: extensive cloning and symbol/metadata rewriting.
- Focus: reduce cloning (see #3) and avoid extra debug-like metadata
  paths where possible (keep IR simpler).

5) Memory footprint ballooning over time
- Evidence: footprint grows steadily from 350M to 3.3G across samples.
- Likely cause: IR size explosion during aggressive CFG transforms.
- Focus: overall IR simplification (less branching, fewer PHI merges,
  reduced inlining) and keep MSL size trending down.

## Notes
- The async pipeline is clearly in AGX/LLVM backend passes
  (`AGXCompilePlan::execute`, `legacy::PassManagerImpl::run`).
- The pre-async phase is a different pipeline (front-end + inliner),
  so improvements need to target both CFG surface area and SSA merge
  complexity.
