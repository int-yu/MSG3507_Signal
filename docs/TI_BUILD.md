# TI 工具链验证

验证基线：MSPM0 SDK 2.10.00.04、TI Arm Clang 5.1.1.LTS。本机已用 CCS 内置 5.1.1.LTS 编译核心库和四个 API 示例。可执行：

~~~powershell
$cc='D:\Program Files (x86)\Ti\CCS\ccs\tools\compiler\ti-cgt-armllvm_5.1.1.LTS\bin\tiarmclang.exe'
& $cc -std=c11 -Wall -Wextra -Werror -mcpu=cortex-m0plus -mthumb -Iinclude -c src\sigq15.c -o $env:TEMP\sigq15.o
& $cc -std=c11 -Wall -Wextra -Werror -mcpu=cortex-m0plus -mthumb -Iinclude -c backends\sigq15_backends.c -o $env:TEMP\sigq15_backends.o
~~~

CMSIS 构建追加 SDK CMSIS-DSP include/library 并定义 SIGQ15_USE_CMSIS_DSP。MATHACL 构建定义 SIGQ15_USE_MATHACL，但先按 SDK 示例补齐并实测硬件事务。CI 不下载 TI 闭源工具链。
## API 教程示例编译

四个无浮点实时示例对应 [API_USAGE.md](API_USAGE.md)。逐个以 TI Arm Clang 编译核心和示例（只检查语法/告警）：

~~~powershell
$cc='D:\Program Files (x86)\Ti\CCS\ccs\tools\compiler\ti-cgt-armllvm_5.1.1.LTS\bin\tiarmclang.exe'
Get-ChildItem examples\api_usage\*.c | ForEach-Object {
  & $cc -std=c11 -Wall -Wextra -Werror -mcpu=cortex-m0plus -mthumb -Iinclude -c $_.FullName -o "$env:TEMP\$($_.BaseName).o"
}
~~~

链接目标程序时还需要 `src/sigq15.c` 与 `backends/sigq15_backends.c`。CMSIS/MATHACL 的 SDK 头、库和板级验证仍由工程集成方提供；不应把本机编译通过解释为已验证硬件事务或时序。
