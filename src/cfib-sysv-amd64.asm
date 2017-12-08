default rel
bits 64
align 16
global _cfib_init_stack:function
global _cfib_swap:function
extern _cfib_exit_thread
section .text

; Call initializer, reverse-called by _cfib_swap.
; The stack was initialized by _cfib_init_stack(), aligned at 8-byte boundary.
; Now, _cfib_swap() has already popped all callee saved regs (48 bytes),
; and returned to this function (popping addr), and stack is aligned at 
; 16-byte boundary.
;
; _cfib_init_stack() saved the callable function and its argument pointer
; to r15 and r14 positions of the stack, and ince they were popped by
; _cfib_swap(), we can use them as arguments.
;
; Arguments:
; r14 = void* args, pointer to fiber arguments
; r15 = void (*func)(void*), pointer to fiber executed function
_cfib_call:
    mov rdi, r14 ; 1st argument rdi = void* args
    call r15 ; call the function ptr from r15
%ifndef _ELF_SHARED
    call _cfib_exit_thread
%else
    call _cfib_exit_thread wrt ..plt ; call the thread exit vector via ELF plt
%endif

; This function synthesizes an initial stack for
; a context, so that _cfib_swap can return into it, and continue
; execution from _cfib_call.
;
; Arguments:
; rdi = void** sp, pointer to rsp of the allocated stack
; rsi = void (*func)(void*), fiber executed function pointer
; rdx = void *args, arguments for the fiber
_cfib_init_stack:
    ; Set rcx as base pointer to the new stack
    ; NOTE: empty stack's pointer should be aligned to page,
    ; and thus it ishould also must be aligned to 16-bytes.
    ; The pointer points to stack ceiling, which is an invalid
    ; address, the first element starts at -8
    mov rcx, [rdi]
    ; Make space in the new stack for following elements
    ; [rcx + 0]:[rcx + 40] regs r15:r12, rbx and rbp (48 bytes)
    ; [rcx + 48] return vector to _cfib_call (8-bytes)
    ; [rcx + 56] 8-bytes of zero for rbp termination
    ; [rcx + 64] 8-bytes for alignment
    sub rcx, 72
    ; [rcx + 0] r15 = rsi, pointer to function executed by the fiber
    mov [rcx + 0], rsi
    ; [rcx + 8] r14 = rdx, the void* argument pointer for fiber function
    mov [rcx + 8], rdx
    ; load address of [rcx + 56] into rax
    lea rax, [rcx + 56]
    ; [rcx + 40] rbp will be pointed to [rcx + 56] which will be zeroed later.
    ; When the rbp points to a memory location with zero pointer value, it will
    ; prevent gdb from going apeshit during stack frame unwinding.
    mov [rcx + 40], rax
    ; Next we must set the return vector to _cfib_call into [rcx + 48], and
    ; this is done differently whether we are dynamically linked and relocated
    ; or statically linked
    %ifdef _ELF_SHARED
    ; We are relocated, so we cannot know beforehand what is the abs address
    ; of our initial call vector ... but NASM allows us to get that abs address
    ; by loading the address from symbol into register
    lea r8, [rel _cfib_call]
    mov [rcx + 48], r8
    %else
    ; We are statically linked, and thus we can move the address of _cfib_call
    ; directly to the stack, it will be an absolute address
    mov qword [rcx + 48], _cfib_call
    %endif
    ; Set rax to zero, consequently our return value becomes 0, although
    ; the C function prototype won't declare a return type :P
    xor rax, rax
    ; Set 8-bytes of zero for rbp termination. The rbp of the new stack
    ; (in position [rcx + 40]) was set to point into this address.
    mov [rcx + 56], rax
    ; Save the modified stack pointer value
    mov [rdi], rcx
    ret

; Swap context from current to next.
;
; Arguments:
; rdi = void**, pointer to current context rsp
; rsi = void*, rsp of the next context
_cfib_swap:
    push rbp
    ; Push callee saved registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    ; Save stack pointer to addr pointed by 1st argument (rdi)
    mov [rdi], rsp
    ; Pivot stack to addr pointed by 2nd argument (rsi)
    mov rsp, rsi
    ; Pop callee saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ; ... and return to next context
    ret
