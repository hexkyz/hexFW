# The actual start address of the code-binary loaded by the initial ROP isn't always at a fixed address / codegen+0. Load the binary from the end of this loader to codegen+0.

bl l0
l0:
mflr 3
li 4, (_end - l0)
add 4, 4, 3 # r4 = addr of _end.
lwz 5, 0(4)
addi 4,4,4 # r5 = u32 value at _end, then increase r4 by 0x4.
mr 3, 29
li 6, 2
srw 5, 5, 6
mtctr 5 # ctr reg = above u32 value >> 2.

copylp: # Copy the data from _end+4 with size *_end, to the address from r29.
lwz 5, 0(4)
stw 5, 0(3)
addi 4,4,4
addi 3,3,4

bdnz copylp

add 1, 1, 30 # Jump to the code-loading ROP to load the codebin which was copied above.
lwz 3, 4(1)

mtctr 3
bctr

_end:

