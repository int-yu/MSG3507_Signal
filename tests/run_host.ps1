$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$out = Join-Path $env:TEMP "mspm0_sigq15_tests.exe"
gcc -std=c11 -Wall -Wextra -Werror -I"$repo/include" "$repo/src/sigq15.c" "$repo/backends/sigq15_backends.c" "$repo/tests/test_signal.c" -o $out
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& $out
if ($LASTEXITCODE) { exit $LASTEXITCODE }
gcc -std=c11 -Wall -Wextra -Werror -I"$repo/include" "$repo/src/sigq15.c" "$repo/examples/synthetic_demo.c" -o (Join-Path $env:TEMP "mspm0_sigq15_demo.exe")
exit $LASTEXITCODE