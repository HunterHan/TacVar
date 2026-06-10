# TacVar Stencil Kernel Debugging Memory (Deepseek Session)

## Timeline

### 1. Initial State
- **Branch**: `dev/0526-manual` → `codex-deepseekv4`
- **Kernels under test**: `gs2d5p.c`, `tl_f90_cg_calc_w.c` (in `stencil/`)
- **Reference kernel**: `jacobi2d5p.c` (known good)
- **Pipeline script**: `run_entry_filt.0526.sh` (compile → non-TF run → TF run → filter)

### 2. Code Sync Issue (Critical)
- **Problem**: camd9554n2 had an OLD version of `gs2d5p.c` with ALL `#ifdef STAGE_TF` blocks removed/commented out
- **Cause**: Each compute node has its own filesystem; `git pull` only reaches `origin`, not af309 local commits
- **Fix**: `rsync -az --delete` entire TacVar from af309 to compute node BEFORE each test run
- **Lesson**: Always `rsync` code before running on compute nodes

### 3. PAPI Negative tr on tl_f90 (Original)
- **Symptom**: `tl_f90_cg_calc_w` with PAPI timer: tr negative (-301 at size128)
- **PAPIx6 was OK**: tr positive (249 at size128)
- **cgt was OK**: tr positive (40 at size128)
- **Root cause**: Not properly diagnosed at this stage; later reversed after fixes

### 4. ERRCODE 2 from filter
- **Symptom**: `filt.x` returning ERRCODE 2 ("Error in parsing timing fluctuations csv file")
- **Root cause**: Code sync issue — filter binary or Python scripts were stale
- **Fix**: Resolved after proper rsync of full TacVar

### 5. TSC_NATIVE crash
- **Symptom**: `gs2d5p` with `-DUSE_TSC_NATIVE` segfaulted with glibc malloc assertion
- **Root cause**: Heap corruption from old code; resolved after sync
- **Note**: TSC_NATIVE uses `__rdtsc()` without serialization; avoid for now

### 6. Sub_loop Performance Difference (gs2d5p vs tl_f90)
- **Symptom**: `sub_loop` in tl_f90 was 1.8x slower than in gs2d5p (at same nsamp=500)
  - gs2d5p: 158.7 ns for 500 iterations (0.317 ns/iter)
  - tl_f90 (old): 285.1 ns for 500 iterations (0.570 ns/iter)
- **Root cause**: Compiler generated `imul rdi, [rsp+0x20]` (memory-operand multiply) for tl_f90 vs `mov rax, [rsp]; imul rdi, rax` (register-register multiply) for gs2d5p
  - The memory-operand version caused pipeline stalls
  - Triggered by: `ra_res += sub_loop(ra, rb, lower)` where `ra = nsamp * rb` was in the same declaration
- **Fix in `tl_f90_cg_calc_w.c`**: Changed STAGE_TF block from:
  ```c
  register uint64_t ra = nsamp * rb;
  register uint64_t lower = ra_lower_boundary;
  ra_res += sub_loop(ra, rb, lower);
  ```
  to:
  ```c
  register uint64_t ra;
  register uint64_t lower = ra_lower_boundary;
  ra = nsamp * rb;
  sub_loop(ra, rb, lower);
  ra_res += ra;
  ```
- **Effect**: sub_loop went from 0.570 ns/iter to ~0.30 ns/iter (same as gs2d5p)

### 7. TSC Timer Broken for tl_f90 TF Mode
- **Symptom**: TF CSV showed 690 TRILLION as per-row timing (full TSC timestamp since boot)
  - Non-TF timing was normal (~3400 ns)
  - TF timing showed `ns1 - 0` = `ns1` (ns0 was never set)
- **Root cause**: Missing `tsc_start()` call in the TF `#elif defined(USE_TSC)` block
  - The preprocessor produced `_read_ns(ns0)` (from `#else` fallback) instead of `tsc_start(&ns0)`
  - This was because `USE_TSC` define was being filtered by `mpicc`
  - Also, the TSC function guard was too strict: `#if defined(__x86_64__) && (defined(USE_TSC) || ...)`
- **Fix in `tl_f90_cg_calc_w.c`**:
  1. Changed TSC guard from `#if defined(__x86_64__) && (defined(USE_TSC) || ...)` to `#if defined(__x86_64__)` (matching jacobi2d5p.c)
  2. Changed `double tsc_ns = 1.0;` to `double tsc_ns;` (uninitialized, matching jacobi2d5p)
- **Follow-up by user**: Also fixed a missing `tsc_start` in the STAGE_TF timer chain

### 8. PAPI/PAPIx6 Reversal (Current Issue)
- **Symptom**: After fixes, PAPI tr became POSITIVE (was negative), PAPIx6 tr became NEGATIVE (was positive)
  - Ratio=0.5, size128: PAPI tr=+170, PAPIx6 tr=-642
- **Direct cause**: PAPIx6 TF per-row timing (2273 ns) is 831 ns larger than PAPI (1442 ns)
  - PAPIx6 sub_loop: 781 ns for 500 iterations (1.56 ns/iter)
  - PAPI sub_loop: 631 ns for 500 iterations (1.26 ns/iter)
  - Delta: 150 ns for 500 iter = 0.30 ns/iter overhead from active hardware counters
- **Mechanism**: When 6 hardware performance counters are active (PAPIx6 mode via `PAPI_add_named_event()`), every instruction incurs ~24% microarchitectural overhead
  - This overhead is INCLUDED in the timing measurement (between ns0 and ns1)
  - The injection model (nsamp * NSPV) does NOT account for this counter overhead
  - Result: tf_residual = (sub_loop + counter_overhead) - nsamp*NSPV is too large → tr negative
- **Why reversal?** The user reverted a PAPI init change that previously mitigated counter overhead
  - Before revert: PAPIx6 had some init (e.g., domain setting) that reduced counter overhead
  - After revert: counter overhead fully active → sub_loop slower → tr negative
  - Sub_loop fix helped PAPI (removed memory-operand imul overhead) → PAPI tr went positive

### 9. PAPIx6 TF CSV Format Issue
- **PAPIx6 TF CSV has 9 columns**: `rank, nsamp, ns, counter0, counter1, ..., counter5`
- **PAPI TF CSV has 3 columns**: `rank, nsamp, ns`
- This is expected behavior (PAPIx6 code appends counter values to CSV)
- `get_tf.py` correctly reads column indices 1 (nsamp) and 2 (ns) regardless of extra columns

## Key Code Differences Between Kernels

### tl_f90_cg_calc_w.c vs jacobi2d5p.c
| Aspect | jacobi2d5p | tl_f90 (original) | tl_f90 (fixed) |
|--------|-----------|-------------------|----------------|
| TSC function guard | `#if __x86_64__` | `#if __x86_64__ && (USE_TSC \|\| ...)` | `#if __x86_64__` ✓ |
| tsc_ns init | uninitialized | `= 1.0` | uninitialized ✓ |
| STAGE_TF sub_loop call | `ra = nsamp * rb; sub_loop(ra, rb, lower);` | `ra_res += sub_loop(ra, rb, lower)` with `ra = nsamp * rb` in decl | Separate decl + assignment ✓ |
| MPI_Allreduce | none | `MPI_Allreduce(&pw, ..., MPI_MIN)` after timing stop | same |
| Input matrices | 2 (x, y) | 5 (w, Di, p, Kx, Ky) | same |
| USE_PREWARM | yes | no | no |

### gs2d5p.c vs jacobi2d5p.c
- Nearly identical twins (same framework)
- gs2d5p: Gauss-Seidel in-place update `x[i][j] = a * (x[i-1][j] + ...)`
- jacobi2d5p: Jacobi read-write separated `y[i][j] = a*x[i][j] + b*(x[i-1][j] + ...)`
- gs2d5p has unused variable `b = 0.20` (copy-paste residue)
- jacobi2d5p has `USE_PREWARM` feature

## Testing Parameters
- **Node**: camd9554n2 (primary), camd9554n1 (latest test)
- **NP**: 4 (testing), 64 (full run)
- **Sizes**: 128, 256, 512 (quick test); 64-2048 (full run)
- **Timers**: cgt, papi, papix6, wtime, tsc, tsc_native (x86_64)
- **NSAMP_RATIO**: 0.5 (quick), 0.5, 0.8 (full)
- **INSITU**: INSITU_SUB_ASM

## Recommended Next Steps
1. Fix PAPIx6 by disabling counters during sub_loop (PAPI_stop/PAPI_start around STAGE_TF)
2. Or calibrate NSPV per-timer (PAPIx6 needs ~1.24x NSPV to compensate)
3. Re-run full pipeline with gs2d5p ratio=0.7 + tl_f90 ratio=0.85 + PAPI fixes
4. Verify tr quality: all non-negative, tr < tm, timer consistency
