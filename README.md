# LLVM IR instrumentation
LLVM passes to help pointer analysis utilities in tracking stored values and indirect calls

__LLVM-14 supported only!__

# Idea
Struct is marked interesting if:
1. It has function pointers
2. It has pointers to interesting structures
3. It has interesting structure as a field

Every interesting structure has an object(singleton), that collects all written values

Every function pointer field has a marker function, that helps to track calls, even if no functions
were written in this field.

Some functions fill objects through provided pointers, but they might not be called in the same object file.
To track written values, such functions are called with singletons.

Some external functions are used as register functions, so passed values normally disappear. All such functions,
that have interesting arguments, are defined to save provided values
