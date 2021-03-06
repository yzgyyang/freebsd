#!/bin/sh

# Fatal trap 12: page fault while in kernel mode
# cpuid = 0; apic id = 00
# fault virtual address   = 0x70
# fault code              = supervisor read data, page not present
# instruction pointer     = 0x20:0xffffffff80dec3cf
# stack pointer           = 0x28:0xfffffe013c9384d0
# frame pointer           = 0x28:0xfffffe013c938510
# code segment            = base 0x0, limit 0xfffff, type 0x1b
#                         = DPL 0, pres 1, long 1, def32 0, gran 1
# processor eflags        = interrupt enabled, resume, IOPL = 0
# current process         = 58092 (syzkaller20)
# trap number             = 12
# panic: page fault
# cpuid = 0
# time = 1596109014
# KDB: stack backtrace:
# db_trace_self_wrapper() at db_trace_self_wrapper+0x2b/frame 0xfffffe013c938180
# vpanic() at vpanic+0x182/frame 0xfffffe013c9381d0
# panic() at panic+0x43/frame 0xfffffe013c938230
# trap_fatal() at trap_fatal+0x387/frame 0xfffffe013c938290
# trap_pfault() at trap_pfault+0x99/frame 0xfffffe013c9382f0
# trap() at trap+0x2a5/frame 0xfffffe013c938400
# calltrap() at calltrap+0x8/frame 0xfffffe013c938400
# --- trap 0xc, rip = 0xffffffff80dec3cf, rsp = 0xfffffe013c9384d0, rbp = 0xfffffe013c938510 ---
# in6_setscope() at in6_setscope+0x5f/frame 0xfffffe013c938510
# ip6_output() at ip6_output+0x103c/frame 0xfffffe013c938750
# udp6_send() at udp6_send+0x79b/frame 0xfffffe013c938900
# sosend_dgram() at sosend_dgram+0x34e/frame 0xfffffe013c938960
# sosend() at sosend+0x66/frame 0xfffffe013c938990
# kern_sendit() at kern_sendit+0x246/frame 0xfffffe013c938a30
# sendit() at sendit+0x1d8/frame 0xfffffe013c938a80
# sys_sendto() at sys_sendto+0x4d/frame 0xfffffe013c938ad0
# amd64_syscall() at amd64_syscall+0x159/frame 0xfffffe013c938bf0
# fast_syscall_common() at fast_syscall_common+0xf8/frame 0xfffffe013c938bf0
# --- syscall (0, FreeBSD ELF64, nosys), rip = 0x80045513a, rsp = 0x7fffffffe568, rbp = 0x7fffffffe580 ---
# KDB: enter: panic
# [ thread pid 58092 tid 100337 ]
# Stopped at      kdb_enter+0x37: movq    $0,0x10b8de6(%rip)
# db> x/s version
# version:        FreeBSD 13.0-CURRENT #0 r363687: Thu Jul 30 08:55:21 CEST 2020
# pho@t2.osted.lan:/usr/src/sys/amd64/compile/PHO
# db>

[ `uname -p` != "amd64" ] && exit 0

. ../default.cfg
cat > /tmp/syzkaller20.c <<EOF
// autogenerated by syzkaller (https://github.com/google/syzkaller)

#define _GNU_SOURCE

#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/syscall.h>
#include <unistd.h>

uint64_t r[2] = {0xffffffffffffffff, 0xffffffffffffffff};

int main(void)
{
  syscall(SYS_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x1012ul, -1, 0ul);
  intptr_t res = 0;
  res = syscall(SYS_socket, 0x1cul, 1ul, 0);
  if (res != -1)
    r[0] = res;
  res = syscall(SYS_socket, 0x1cul, 2ul, 0x88);
  if (res != -1)
    r[1] = res;
  syscall(SYS_dup2, r[1], r[0]);
  *(uint8_t*)0x20000000 = 0x1c;
  *(uint8_t*)0x20000001 = 0x1c;
  *(uint16_t*)0x20000002 = htobe16(0x4e21);
  *(uint32_t*)0x20000004 = 0;
  *(uint64_t*)0x20000008 = htobe64(0);
  *(uint64_t*)0x20000010 = htobe64(1);
  *(uint32_t*)0x20000018 = 0;
  syscall(SYS_bind, r[0], 0x20000000ul, 0x1cul);
  *(uint8_t*)0x200002c0 = 0x1c;
  *(uint8_t*)0x200002c1 = 0x1c;
  *(uint16_t*)0x200002c2 = htobe16(0x4e23);
  *(uint32_t*)0x200002c4 = 0;
  memcpy((void*)0x200002c8,
         "\xff\x71\x4e\x39\x01\x68\x0f\x3c\xd3\x1d\xb7\x61\x2c\x26\x8b\xac",
         16);
  *(uint32_t*)0x200002d8 = 0;
  syscall(SYS_sendto, r[0], 0ul, 0ul, 0ul, 0x200002c0ul, 0x1cul);
  return 0;
}
EOF
mycc -o /tmp/syzkaller20 -Wall -Wextra -O2 /tmp/syzkaller20.c -lpthread ||
    exit 1

(cd ../testcases/swap; ./swap -t 1m -i 20 -h > /dev/null 2>&1) &
(cd /tmp; ./syzkaller20) &
sleep 60
pkill -9 syzkaller20 swap
wait

rm -f /tmp/syzkaller20 /tmp/syzkaller20.c /tmp/syzkaller20.core
exit 0
