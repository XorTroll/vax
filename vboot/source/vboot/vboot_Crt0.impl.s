.section ".crt0", "ax"
.global __vboot_start

__vboot_start:
    b __vboot_startup
    .word 0

.org __vboot_start+0x8
__vboot_startup:
    // context ptr and main thread handle
    mov x25, x0
    mov x26, x1

    // clear .bss
    ldr x0, =__bss_start__
    ldr x1, =__bss_end__
    sub x1, x1, x0  // calculate size
    add x1, x1, #7  // round up to 8
    bic x1, x1, #7

__vboot_bss_clear_loop: 
    str xzr, [x0], #8
    subs x1, x1, #8
    bne __vboot_bss_clear_loop

__vboot_impl:
    // store stack pointer
    mov x1, sp
    ldr x0, =__stack_top
    str x1, [x0]

    // initialize system (only with main thread handle, we don't make use of the context ptr)
    mov x0, x26
    bl __vboot_init

    // call entrypoint
    ldr x30, =__vboot_exit
    b __vboot_main

.global __vboot_terminate
.type __vboot_terminate, %function
__vboot_terminate:
    // restore stack pointer
    ldr x8, =__stack_top
    ldr x8, [x8]
    mov sp, x8

    // jump back to loader
    br x2

.pool
