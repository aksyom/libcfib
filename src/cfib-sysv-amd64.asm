default rel
bits 64
align 16
global _cfib_call:function
global _cfib_init_stack:function
global _cfib_swap:function
extern _cfib_exit_thread
section .text

; Call initializer, reverse-called by _cfib_swap.
; Previous to _cfib_swap, the stack was initialized by _cfib_init_stack.
; Now, _cfib_swap has already popped all callee saved regs (48 bytes)
; and the stack is aligned to 8-byte boundary.
; - First quad in the stack is 8-bytes of bogus, put there
; only to align the stack pointer to 8-byte boundary.
; - The next two quads in stack are the 1st and 2nd arguments
_cfib_call:
    pop rdi ; 1st arg rdi = void* args
    pop rsi ; rsi = void (*func)(void*)
    ; stack is now aligned to 16-byte boundary
    call rsi ; call the function ptr
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
    ; push 2nd and 3rd argument to stack
    ; these are popped by _cfib_call
    ; into 1st and 2nd argument regs respectively
    push rsi        ; 2nd arg rsi = void (*func)(void*)
    push rdx        ; 3rd arg rdx = void* args
    %ifdef _ELF_SHARED
    ; we are relocated, so we cannot know beforehand what is the abs address
    ; of our initial call vector ... but NASM allows us to get that abs address
    ; by loading the address from symbol into register
    mov rcx, _cfib_call
    push rcx    ; absolute addr of _cfib_call is in rcx, push it to stack!
    %else
    push qword _cfib_call ; push addr of _cfib_call to stack
    %endif
    push r8        ; push [r8] = 0x0, prevent gdb going apeshit
    sub rsp, 40     ; alloca [r15 - rbx]
    ; NOTE: now the stack is 8-byte aligned! This is necessary
    ; because _co_swap and _co_exit require that the
    ; next context stack pointer must be aligned to 8
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
