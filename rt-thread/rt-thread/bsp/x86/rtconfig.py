import os

# toolchains options
ARCH='x86'
CPU='i386'
CROSS_TOOL='gcc'

RTT_ROOT = os.getenv('RTT_ROOT') or os.path.join(os.getcwd(), '..', '..')

if os.getenv('RTT_CC'):
	CROSS_TOOL = os.getenv('RTT_CC')

# cross_tool provides the cross compiler
# EXEC_PATH is the compiler execute path, for example, CodeSourcery,

if  CROSS_TOOL == 'gcc':
	PLATFORM 	= 'gcc'
	EXEC_PATH 	= '/home/jasonhu/Desktop/rtthread-smart/tools/gnu_gcc/i386-linux-musleabi_for_x86_64-pc-linux-gnu/bin'
elif CROSS_TOOL == 'keil':
    print('================ERROR============================')
    print('Not support keil yet!')
    print('=================================================')
    exit(0)
elif CROSS_TOOL == 'iar':
    print('================ERROR============================')
    print('Not support iar yet!')
    print('=================================================')
    exit(0)

if os.getenv('RTT_EXEC_PATH'):
	EXEC_PATH = os.getenv('RTT_EXEC_PATH')

BUILD = 'debug'
LIBC_MODE = 'release' # 'debug' or 'release', if debug, use libc in components, or not use libc with toolchain

if PLATFORM == 'gcc':
    # toolchains
    PREFIX = 'i386-unknown-linux-musl-'
 
    CC = PREFIX + 'gcc'
    AS = PREFIX + 'gcc'
    AR = PREFIX + 'ar'
    LINK = PREFIX + 'gcc'
    TARGET_EXT = 'elf'
    SIZE = PREFIX + 'size'
    OBJDUMP = PREFIX + 'objdump'
    OBJCPY = PREFIX + 'objcopy'

    DEVICE = ''

    if LIBC_MODE == 'debug':
        EXT_CFLAGS = ' -nostdinc -nostdlib -fno-builtin -fno-stack-protector'
    else:
        EXT_CFLAGS = ''

    CFLAGS = DEVICE + ' -Wall' + EXT_CFLAGS
    AFLAGS = ' -c' + DEVICE + ' -x assembler-with-cpp -I.'

    if LIBC_MODE == 'debug':
        EXT_LFLAGS = ' -nostdlib'
    else:
        EXT_LFLAGS = ''
        
    LFLAGS = DEVICE + ' -static -nostartfiles -Wl,--gc-sections,-Map=rtthread.map,-cref -n -T link.lds' + EXT_LFLAGS

    CPATH = ''
    LPATH = ''

    if BUILD == 'debug':
        CFLAGS += ' -O0 -gdwarf-2'
        AFLAGS += ' -gdwarf-2'
    else:
        CFLAGS += ' -O2'
DUMP_ACTION = OBJDUMP + ' -D -S $TARGET > rtthread.asm\n'
POST_ACTION = OBJCPY + ' -O binary $TARGET rtthread.bin\n' + SIZE + ' $TARGET \n'