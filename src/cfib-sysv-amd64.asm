default rel
bits 64
align 16
global _cfib_call:function
global _cfib_init_stack:function
global _cfib_swap:function
extern _cfib_exit_thread
section .text

; Call initializer, reverse-called by _cfib_swap.
; The stack was initialized by _cfib_init_stack aligned to 8-byte boundary.
; Now, _cfib_swap has already popped all callee saved regs (48 bytes),
; and stack is aligned at 16-byte boundary.
;
; _cfib_init_stack saved the callable function and its argument pointer
; to r14 and r15 of the stack.
_cfib_call:
    mov rdi, r15 ; 1st argument rdi = void* args
    ; stack is now aligned to 16-byte boundary
    call r14 ; call the function ptr
    ; if the func returned a value, we would handle rax here ...
%ifndef _ELF_SHARED
    call _cfib_exit_thread
%else
    call _cfib_exit_thread wrt ..plt ; call the thread exit vector via ELF plt
%endif

; This function synthesizes an initial stack for
; a context, so that _cfib_swap can pivot into it!
; rdi = void** sp, pointer to rsp of the allocated stack
; rsi = void (*func)(void*), co-routine function
; rdx = void *args, arguments for co-routine
_cfib_init_stack:
    mov r9, rsp     ; save rsp for unpivoting
    mov rsp, [rdi]  ; pivot stack to 1st arg (rdi)
    ; NOTE: empty stack's rsp should be aligned to page,
    ; and thus it should also must be aligned to 16-bytes.
    sub rsp, 8      ; add 8 bytes of bogus for alignment
    xor rax, rax    ; rax = 0
    push rax        ; push 8-bytes of zero
    ; NOTE: the zeroes we pushed to the stack was
    ; done so that we can point rbp to it later.
    ; This will stop gdb from going apeshit!
    mov r8, rsp    ; r8 (as rbp) = stack bottom, value 0x0
    %ifdef _ELF_SHARED
    ; we are relocated, so we cannot know beforehand what is the abs address
    ; of our initial call vector ... but NASM allows us to get that abs address
    ; by loading the address from symbol into register
    mov rcx, _cfib_call
    push rcx    ; absolute addr of _cfib_call is in rcx, push it to stack!
    %else
    push qword _cfib_call ; push addr of _cfib_call to stack
    %endif
    push r8         ; push [r8] = 0x0, prevent gdb going apeshit
    sub rsp, 24     ; stack space for r13, r12, and rbx
    ; push 2nd and 3rd arguments, that is, rsi and rdx into the pivoted stack
    ; registers r14, and r15.
    push rsi        ; [r14] = void (*func)(void*)
    push rdx        ; [r15] = void* args
    ; NOTE: now the stack is 8-byte aligned! This is necessary
    ; because when _cfib_swap returns to _cfib_call, the 8-byte
    ; addr of _cfib_call is popped, and alignment becomes 16-bytes.
    ; _cfib_call ASSUMES that the stack is 16-byte aligned!
    mov [rdi], rsp  ; save context sp
    mov rsp, r9     ; unpivot stack
    ret

; Swap context from current to next
; rdi = void**, pointer to current context rsp
; rsi = void*, rsp of the next context
; NOTE: both pointers MUST be aligned to 8-byte boundary!
_cfib_swap:
    ; by convention, stack is now aligned to 8-byte boundary
    push rbp
    mov rbp, rsp
    ; push callee saved registers
    push rbx
    push r12
    push r13
    push r14
    push r15
    ; Stack is aligned to 8-byte boundary.
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
