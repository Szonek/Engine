Particle effects the engine's scenario tests play (`src/tests/scenario_tests/particle_tests.cpp`) and the big scene benchmark
measures (`src/tests/benchmarks`); the game doesn't use them.

* `burst.json`: one burst of 10 particles living 0.5 s, then nothing (a one-shot effect).
* `looping.json`: 60 particles a second, each living 1 s, forever.
* `benchmark.json`: 100 particles a second, each living 1 s (~100 live), forever: the benchmark's particle scene.
* `lit_smoke.json`: a burst of 24 lit, soft, eroding smoke puffs of the flipbook `particles/smoke_puff.png` (under
  assets/models), living about 3 s.
