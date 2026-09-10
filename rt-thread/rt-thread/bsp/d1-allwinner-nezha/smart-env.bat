set RTT_CC=gcc
set RTT_EXEC_PATH=%cd%\tools\gnu_gcc\riscv64-linux-musleabi_for_i686-w64-mingw32\bin
set RTT_CC_PREFIX=riscv64-unknown-linux-musl-
set RTT_TOOLS_PATH=%cd%\tools
@set PATH=%RTT_EXEC_PATH%;%RTT_TOOLS_PATH%;%ENV_ROOT%\tools\gnu_gcc\arm_gcc\mingw\bin;%PATH%

@echo "Arch      : riscv64"
@echo "CC        : %RTT_CC%"
@echo "PREFIX    : %RTT_CC_PREFIX%"
@echo "EXEC_PATH : %RTT_EXEC_PATH%"
