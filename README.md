# libcfib
-lcfib: C Fibers, an implementation of co-operative threads of execution

UPDATE: I finally uploaded the stuff I was working on my local repo. Currently only works on POSIX systems that use the SysV AMD64 ABI (Linux, BSD ...). Next I am going to concentrate on figuring out how to properly document the library API. Probably will end up integrating Doxygen with SConstruct. Later, finish the cdecl-x86 ABI, and start working on Windows support.

I decided on the LGPLv3 license for this project because there are already other fiber-like libraries available, most with less restrictive licenses, some more extensive than this. The main difference between this project and others is that my goals for the end product is for it to be as minimalist as possible; that is, implement only the mandatory features. Being minimalist makes for more easily intelligible source. In fact, I intend to develop this software so that people can learn how it works, and make their own variant if this one doesn't fit their needs.

Design goals (thus far):
- NO inline assembler, it's ugly!
- reasonable usage of macros where it makes for speed without breaking things
- aim for minimalist API and simple, intelligible design
- all complex operations should be done with C functions
- stack switching and context swapping done with small and simple assembler routines
- assembler routines in separate files sorted by calling convention and CPU architecture
- port the code at least to the following ABIs: SYSV/AMD64, CDECL/x86, Microsoft/x64 (WIN64 ABI)
- if time permits, port also to ARM/EABI and ARM/AARCH64
- will probably skip porting to STDCALL/X86, because it is mainly used by 32-bit Windows systems (WINCE, XP, Vista and the like). Hardly anybody develops new software for these systems anymore.
- ditch GNU Autotools, they are pain to use, slow and ugly. Will use SCons as build system for now.
- comment as much as possible
- later, write a very thorough design and implementation document in prose form,
so that every person who reads it while looking at the source actually LEARN to
implement similar functionality on their own!
- keep a reasonably opinionated approach to what works and what doesn't
