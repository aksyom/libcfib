# libcfib
-lcfib: C Fibers, an implementation of co-operative threads of execution

Design goals:
- NO inline assembler, it's ugly!
- reasonable usage of macros where it makes for speed without breaking things
- aim for minimalist interface and simple, intelligible design
- all complex operations should be done with C functions
- stack switching and context swapping done with small and simple assembler routines
- assembler routines in separate files sorted by calling convention and CPU architecture
- port the code at least to the following ABIs: SYSV/AMD64, CDECL/x86, Microsoft/x64 (WIN64 ABI)
- if time permits, port also to ARM/EABI and ARM/AARCH64
- ditch GNU Autotools, they are pain to use, slow and ugly. Will use SCons as build system for now.
- comment as much as possible
- later, write a very thorough design and implementation document in prose form,
so that every person who reads it while looking at the source actually LEARN to
implement similar functionality on their own!
- keep a reasonably opinionated approach to what works and what doesn't
