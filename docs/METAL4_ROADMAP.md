# MetalFPGA Road Ahead

Checkpoint status:
- Parser is Verilog 2005 complete.
- Elaborator is in principle Verilog 2005 complete.

Roadmap (ordered):
1) MSL emission test suite: get emission correct end-to-end, then debloat.
2) Peephole and CSE passes after MSL emission correctness.
3) Retroactive retest from the ground up to verify no regressions.
4) Milestone: MSL emission is neat and correct (declare milestone with virtual cake together with human).
5) Runtime upgrade work begins after the milestone and retest.
