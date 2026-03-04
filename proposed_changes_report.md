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

### Before vs. After (No limit → maxBacktracks=10000)

| Metric | Before | After | Goal |
|:---|:---:|:---:|:---:|
| s13207 total time | ~1252s | *(pending)* | < 200s |
| s15850 total time | ~1941s | *(pending)* | < 200s |
| c5315 total time | ~35s | *(pending)* | ~Same |
| c2670 total time | ~17s | *(pending)* | ~Same |
| Cliques s13207 (q=2) | 1 | *(pending)* | Same or more |

*Measurement*: `run_validation_alg2.bat` → `validation_alg2_cliques.csv` TotalTime column

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

## Priority & Status

| # | Suggestion | Status | Effort |
|:---:|:---|:---:|:---:|
| 1 | S3 — TC Metric | ✅ Done | Low |
| 2 | S1 — Threshold Tuning | ⏳ Pending | Low |
| 3 | S2 — PODEM Backtrack | ⏳ Pending | Medium |
