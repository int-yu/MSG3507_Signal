# MSPM0G3507 Detailed API Usage Manual Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a complete Chinese fixed-point API reference and task tutorial whose examples compile against the real MSPM0G3507 library without floating point.

**Architecture:** `docs/API.md` is the per-symbol authority; `docs/API_USAGE.md` presents real acquisition and processing flows; `examples/api_usage/` contains the exact compilable programs referenced by the tutorial. A pytest contract checks header-symbol coverage, Q-format wording, forbidden constructs, links, and GCC/TI compilation.

**Tech Stack:** Markdown, C11 fixed-point C, Python 3.11, pytest, GCC, TI Arm Clang 5.1.1.LTS, MSPM0 SDK 2.10.00.04.

## Global Constraints

- Do not change `include/sigq15.h` or `include/sigq15_backends.h` ABI.
- Realtime examples must not contain `float`, `double`, standard `math.h`, or dynamic allocation.
- Every numeric parameter and result must state Q format, physical conversion, range, and saturation behavior.
- AGC must be explicitly excluded from amplitude/THD/transfer-function fidelity paths.
- Do not claim unverified MATHACL transactions, CMSIS linking, target benchmark results, or package pins.

---

### Task 1: Documentation Contract Test

**Files:**
- Create: `tests/test_api_docs.py`
- Create: `docs/api-symbols.txt`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: declarations matching `sigq15_*(`, `sigq31_*(`, and backend declarations.
- Produces: an explicit symbol manifest plus checks for reference coverage, Q-format keywords, forbidden constructs, local links, and example compilation.

- [ ] Write failing coverage and example-compilation tests before adding the manifest/tutorial.
- [ ] Run `python -m pytest tests/test_api_docs.py -q`; expect failure for missing manifest/tutorial/examples.
- [ ] Add the exact manifest from both public headers and make CI execute the contract test.
- [ ] Run again; expect failure only for undocumented symbols/examples.
- [ ] Commit with `Test detailed MSP API documentation`.

### Task 2: Fixed-Point API Reference

**Files:**
- Rewrite: `docs/API.md`
- Test: `tests/test_api_docs.py`

**Interfaces:**
- Consumes: all public enums, structs, Q formats, and declarations from both headers.
- Produces: one authoritative entry per public function family and field-level structure tables.

- [ ] Document status/diagnostics, saturation helpers, phase conversion, calibration, DC block, mean removal, moving average, FIR, decimator, SOS IIR, statistics, zero crossing, and correlation.
- [ ] Every entry includes exact prototype, Q format, physical conversion formula, ranges, workspace lifetime, diagnostic updates, reset behavior, minimal snippet, and errors.
- [ ] Run coverage; expect frequency-domain/tracking/backend symbols still missing.
- [ ] Document windows, Q2.14 Goertzel, Q8.8 dB metrics, NCO, IQ, AGC, PLL/FLL, LMS/NLMS, transfer point, CMSIS RFFT, and MATHACL adapter limitations.
- [ ] Run documentation tests; expect symbol coverage and Q-format checks pass.
- [ ] Commit with `Document every MSP fixed-point API`.

### Task 3: Compilable Fixed-Point Tutorials

**Files:**
- Create: `docs/API_USAGE.md`
- Create: `examples/api_usage/preprocess_filter.c`
- Create: `examples/api_usage/measure_spectrum.c`
- Create: `examples/api_usage/tracking_adaptive.c`
- Create: `examples/api_usage/platform_backends.c`

**Interfaces:**
- Programs include public headers, own static state, check statuses/diagnostics, and provide integer display conversions.
- No realtime program uses floating point, `math.h`, or allocation.

- [ ] Add expected example files to the contract test and observe missing-file failure.
- [ ] Implement ADC-code-to-Q15, calibration, DC block, FIR/SOS IIR, moving average, decimation, saturation diagnostics, and reset example.
- [ ] Implement RMS/mHz frequency, Goertzel, Q8.8 dB formatting, and Q16.16 delay example.
- [ ] Implement phase-continuous NCO, IQ, FLL/PLL lock/dropout handling, and NLMS/AGC-boundary example.
- [ ] Implement backend example that shows portable availability checks and guarded CMSIS/MATHACL calls without claiming unavailable target behavior.
- [ ] Write task tutorial with exact conversions such as `volts = q15 * Vref / 32768`, performed only in display/configuration prose, not realtime example C.
- [ ] Compile every example with GCC and TI Arm Clang; scan forbidden tokens; expect all pass.
- [ ] Commit with `Add MSP fixed-point API tutorials`.

### Task 4: Index, CI, and Release Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/TI_BUILD.md`
- Modify: `.github/workflows/ci.yml`

**Interfaces:**
- Produces direct manual navigation and CI enforcement of documentation/example consistency.

- [ ] Add manual quick links and recommended reading order.
- [ ] Run GCC regressions, pytest, TI Arm Clang compilation, fixed-point forbidden-token scan, Markdown/UTF-8/link/placeholder checks, and `git diff --check`.
- [ ] Verify `main.syscfg` still generates with MSPM0 SDK 2.10.00.04.
- [ ] Commit with `Complete MSP API manual`.
- [ ] Push `main`, wait for GitHub Actions, and report the run URL.
