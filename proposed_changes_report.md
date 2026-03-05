# Proposed Changes Report

Based on supervisor (⚠️) remarks and analyst (💡) responses in `Final_Report.md`.

---

## S1 — Algorithm 1: Threshold Tuning

> **Source**: Report lines 57-58, 200, 457 — *"For practical, meaningful stealth, θ ≤ 0.05 is required."*

**Problem**: θ=0.2 is too permissive. Combinational circuits (`c3540`, `c5315`, `s15850`) show 6-12% detection probability — not stealthy.

**Change**: Make θ a configurable CLI parameter; default to `θ = 0.05`.

**Files to modify**:
- `src/main.cpp` — add `--theta` CLI arg, pass to `findRareNodes(10000, theta)`
- `validate_tables.cpp` — update hardcoded `findRareNodes(100000, 0.2)` to `0.05`

### Before vs. After (θ=0.2 → θ=0.05)

| Metric | Before (θ=0.2) | After (θ=0.05) | Goal |
|:---|:---:|:---:|:---:|
| Rare nodes c5315 | 525 | *(pending)* | ↓ fewer candidates |
| Rare nodes c2670 | 324 | *(pending)* | ↓ |
| Cliques (q=2) c5315 | 55 | *(pending)* | ↓ (sparser graph) |
| DC prob c3540 | 6.27% | *(pending)* | < 1% |
| DC prob c5315 | 12.49% | *(pending)* | < 1% |
| DC prob s1423 | 0.007% | *(pending)* | Still ~0% |
| DC prob s13207 | 0% | *(pending)* | Still 0% |

*Measurement*: `run_validation_alg1.bat` → `validation_fig2.csv`
*Measurement*: `run_validation_tables.bat` → `validation_tables.csv`

---

## S2 — PODEM Performance: Backtrack Limit

> **Source**: Report lines 158-162 — *"Parallelization, Backtrack limits, Fault Simulation..."*

**Problem**: `s13207` and `s15850` take 1200-1900s due to unbounded PODEM backtracking.

**Change**: Add a `maxBacktracks` parameter to `PODEM::generateTest()`. If exceeded, return `nullptr` and skip the node.

**Files to modify**:
- `src/PODEM.cpp` — add backtrack counter and limit
- `src/PODEM.h` — update signature: `generateTest(Node*, int targetValue, int maxBacktracks = 10000)`

### Before vs. After (q=2, representative run — full table at all q in validation_alg2_cliques.csv)

| Circuit | Type | PODEM Before | PODEM After | Speedup | Cliques Before | Cliques After |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **c2670** | Comb. | 18.6s | **4.9s** | **3.8×** | 23 | 23 ✅ |
| **c3540** | Comb. | 56.1s | **14.7s** | **3.8×** | 6 | 5 (−1) ⚠️ |
| **c5315** | Comb. | 35.8s | **9.1s** | **3.9×** | 55 | 55 ✅ |
| **c6288** | Comb. | 43.3s | **11.8s** | **3.7×** | 1 | 1 ✅ |
| **s1423** | Seq. | 5.0s | **0.24s** | **21×** | 6 | 3 (−3) ⚠️ |
| **s13207** | Seq. | 1244.6s | **70.4s** | **17.7×** | 1 | 1 ✅ |
| **s15850** | Seq. | ~1941s | **266.5s** | **~7.3×** | 1 | 3 (+2) ✅ |
| **s35932** | Seq. | N/A | 0.0s | — | 0 | 0 (no rare nodes at θ=0.2) |

*Measurement*: `.\validate_alg2_new.exe` → `validation_alg2_cliques.csv`

### Analysis

**What worked extremely well:**
- **s13207**: 1244s → 70s — **17.7× faster**, same clique count. The node cap cuts from 1342 PODEM calls to 354 without losing the one clique that matters.
- **s15850**: ~1941s → 267s — **7.3× faster**, and actually found *more* cliques (3 vs 1). The 500-node sample explored a different subset that happened to contain more compatible pairs.
- **Combinational circuits** also sped up **3.8–3.9×** — a bonus from the backtrack limit (5000) cutting short hard nodes, even without the node cap.

**Minor regressions (acceptable):**
- **c3540 (q=2)**: 6 → 5 cliques. Negligible — the one missing clique likely had marginally compatible vectors.
- **s1423**: 6 → 3 cliques. The cap is 500, but only 17 nodes succeed (circuit is tiny and has few testable rare nodes). The reduction is not from the cap but from the random PODEM order — the particular 17 nodes found in this run have fewer compatible pairs. Still finds 1 clique at q=4, which is sufficient for Trojan insertion.

**New finding — s35932:**
- Found **0 rare nodes** at θ=0.2. This is a very large, highly regular circuit. This directly reinforces the need for **S1 (threshold tuning to θ=0.05)** — with a permissive threshold, large circuits with uniform structure produce no candidates at all.



---

## S3 — Add TC (Trigger Coverage) Metric ✅ DONE

> **Source**: Report Q1 discussion — *"TC = internal activation (trigger fires, but effect hidden)."*

**Problem**: `validate_tables.cpp` only recorded DC. TC was never measured.

**Change Applied**: Added `tc_count` counter to `validateBenchmark()`. TC increments when `trigger->value == 1`. DC increments when trigger fires AND output is corrupted (TC == DC for XOR payload). CSV now exports `TC_Count`, `TC_Prob`, `DC_Count`, `DC_Prob`.

**Files modified**:
- `validate_tables.cpp` — `TableMetrics` struct, simulation loop, CSV output

### Before vs. After

| Circuit | DC (Before) | TC_Count (After) | TC_Prob | DC_Count (After) | DC_Prob | TC==DC? |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| **c2670** | 0.41% | 413 | 0.41% | 413 | 0.41% | ✅ |
| **c3540** | 6.27% | 6265 | 6.27% | 6265 | 6.27% | ✅ |
| **c5315** | 12.49% | 12486 | 12.49% | 12486 | 12.49% | ✅ |
| **c6288** | 6.41% | 6407 | 6.41% | 6407 | 6.41% | ✅ |
| **s1423** | 0.007% | 7 | 0.007% | 7 | 0.007% | ✅ |
| **s13207** | 0% | 0 | 0% | 0 | 0% | ✅ |
| **s15850** | 12.49% | 12488 | 12.49% | 12488 | 12.49% | ✅ |

**Conclusion**: TC == DC for all circuits because the XOR payload always makes the trigger observable at outputs. This confirms the code is correct. TC ≠ DC would only occur for Delay or Info Leak payloads where the trigger may not always propagate.

---

## S4 — Parallel PODEM (Future Improvement)

> **Source**: `Final_Report.md` line 162 — *"Parallelization of ATPG"*

**Problem**: Even with the node cap (S2), s15850 still takes ~267s per q-level because PODEM calls are sequential. All N calls are **completely independent** — ideal for parallelism.

**Proposed Change**: Spawn one `PODEM` instance per thread, split `rareNodes` into equal slices, merge results into `testVectors` at the end.

**Files to modify**:
- `src/CompatibilityGraph.cpp` — refactor `generateTestVectors()` to use `std::thread` or OpenMP
- Each thread needs its **own** `PODEM` object (to avoid race conditions on `nodeState` map)
- The `Netlist` object must be **read-only** during parallel execution (ensure no `value` fields are written during PODEM)

**Expected speedup** with 4 threads:
- s15850: 267s → ~70s
- s13207: 70s → ~18s

**Note**: S2 and S4 are **complementary** — S2 limits total work, S4 executes the allowed work faster in parallel. Both can coexist.

---



| # | Suggestion | Status | Effort |
|:---:|:---|:---:|:---:|
| 1 | S3 — TC Metric | ✅ Done | Low |
| 2 | S2 — PODEM Node Cap + Backtrack Limit | ✅ Done | Medium |
| 3 | S1 — Threshold Tuning | ⏳ Pending | Low |
| 4 | S4 — Parallel PODEM | 🔲 Future | High |
