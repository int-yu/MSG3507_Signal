# TI 工具链验证

计划基线：MSPM0 SDK 2.10.00.04、TI Arm Clang 5.1.1.LTS。本机有 SDK 2.10.00.04 与编译器 4.0.2.LTS，可执行：

~~~powershell
$cc='C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe'
& $cc -std=c11 -Wall -Wextra -Werror -mcpu=cortex-m0plus -mthumb -Iinclude -c src\sigq15.c -o $env:TEMP\sigq15.o
& $cc -std=c11 -Wall -Wextra -Werror -mcpu=cortex-m0plus -mthumb -Iinclude -c backends\sigq15_backends.c -o $env:TEMP\sigq15_backends.o
~~~

CMSIS 构建追加 SDK CMSIS-DSP include/library 并定义 SIGQ15_USE_CMSIS_DSP。MATHACL 构建定义 SIGQ15_USE_MATHACL，但先按 SDK 示例补齐并实测硬件事务。CI 不下载 TI 闭源工具链。