.arm
.align 4
.globl co_switch_arm
# r0: cothread to switch to, r1: current cothread
co_switch_arm:
   # Save state
   stmia r1!, {r4, r5, r6, r7, r8, r9, r10, r11, sp, lr}
   # Load state
   ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, sp, pc}

