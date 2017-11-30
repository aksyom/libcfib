# libcfib
-lcfib: C Fibers, an implementation of co-operative threads of execution

Design goals:
- NO inline assembler, it's ugly!
- NO faux language constructs with macros, they are even worse than inline assembler!
- aim for minimalist interface and simple, intelligible design
- all complex operations should be done with C functions
- stack switching and context swapping done with small and simple assembler routines
- assembler routines in separate files sorted by calling convention and CPU architecture
- comment as much as possible
- later, write a very thorough design and implementation document in prose form,
so that every person who reads it while looking at the source actually LEARN to
implement similar functionality on their own!
- keep a reasonably opinionated approach to what works and what doesn't
