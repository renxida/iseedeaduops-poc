# ---
# jupyter:
#   jupytext:
#     text_representation:
#       extension: .py
#       format_name: light
#       format_version: '1.4'
#       jupytext_version: 1.2.1
#   kernelspec:
#     display_name: Python 3
#     language: python
#     name: python3
# ---

# + {"slideshow": {"slide_type": "-"}, "cell_type": "markdown"}
# # Init
# -
import sys
import os
import getpass
import tempfile
import subprocess


def nop_66(length, assembler = "as"):
    assert length >= 1
    if assembler == "gas" or assembler == "as":
        return ".byte " + "0x66, " * (length - 1) + "0x90"
    elif assembler == "nasm":
        return "db " + "66H, " * (length - 1) + "90H"



# # Helper Functions

def jumpaddr(n, opcode_length):
    addr_length = 4
    jump_addr = n * 32 - (opcode_length + addr_length)
    jump_bytes = [0]*4 # 4-byte address
    for i in range(4):
        jump_bytes[i] = jump_addr // (0x100 ** i) % 0x100
    tobyte = lambda x: "{0:#0{1}x}".format(x,4)


    return ", ".join(list(map(tobyte,jump_bytes)))


def jumpblock(n):
    jumpins = ".byte 0xE9, "+jumpaddr(n,opcode_length=1+15)
    return f"""
{nop_66(15)}
{jumpins}
{nop_66(12)}
"""


# # Generate

DSB_NSETS=32


def gen_zebra(dsb_sets, n_ways):
    jumps = []
    for w in range(n_ways):
        for i in range(len(dsb_sets)):

            s = dsb_sets[i]

            if(w == 0 and i == 0):
                if(s != 0):
                    jumps.append(s - 0) # get to the first one from beginning of loop
                continue

            jump = dsb_sets[i] - dsb_sets[i-1]
            if(i == 0):
                jump += DSB_NSETS # go to the nest iteration if w just increased
            jumps.append(jump)

    return jumps


# to trigger an "ILLEGAL INSTRUCTION" exception and crash the program
invalid_block = "\n"+(".byte " + ", ".join(["0xFF"]*8) + "\n")*4 + "\n"


def gen_zebra(dsb_sets, n_ways):
    locs = [0]
    for i_ways in range(n_ways):
        locs += list(map(lambda x: x + DSB_NSETS*(i_ways), dsb_sets))
    locs.append(n_ways*DSB_NSETS)

    blocks = [invalid_block] * (n_ways*DSB_NSETS) # fill with invalid blocks then replace with jump blocks
    for i in range(len(locs)-1):
        blocks[ locs[i] ] = jumpblock(locs[i+1] - locs[i]) # jump from block locs[i] to locs[i+1]

    asm = "".join(blocks)

    return asm


def to_c_inline_asm(asm):

    lines = asm.strip().split("\n")

    # add c __asm prefix and suffix
    lines = ['__asm("']+lines+['");']

    # justify lines
    maxlen = max(map(len, lines))
    lines=map(lambda l: l.ljust(maxlen+2), lines)

    # covert to
    c_inline_asm = "\\t\\n\\\n".join(lines)
    return c_inline_asm



#if len(sys.argv) != 2:
#    print("Please provide upper bound of sets");
#    exit();
#sets_0 = list(range(31,32))

sets_all = list(range(32))

output_file =""
#for i in range(8):
#    output_file += "#define STRIPES_" + str(i) + " " + to_c_inline_asm(gen_zebra(list(range(28-4*i,32-4*i)), 8))
#    output_file += "\n" * 4

#for i in range(16):
#    output_file += "#define STRIPES_" + str(i) + " " + to_c_inline_asm(gen_zebra(list(range(30-2*i,32-2*i)), 6))
#    output_file += "\n" * 4


#for i in range(32):
#    output_file += "#define STRIPES_" + str(i) + " " + to_c_inline_asm(gen_zebra(list([31-i]), 6))
#    output_file += "\n" * 4

output_file += "#define STRIPES_0 " + to_c_inline_asm(gen_zebra(list(range(21,32)), 8))
output_file += "\n" * 4
output_file += "#define STRIPES_SUPER " + to_c_inline_asm(gen_zebra(sets_all, 8))
output_file += "\n" * 4
#dummyoutput_file = "#define NOPS_240 " + to_c_inline_asm(
#f"""{nop_66(15)}
#{nop_66(15)}
#{nop_66(2)}
#"""*240)


print(output_file)


