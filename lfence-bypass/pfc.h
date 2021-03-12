#ifndef PFC_H
#define PFC_H


#include <inttypes.h>
#include <stdint.h>

// memory for reading pfcs into
extern int64_t* pfc_mem;


// functions to export
void pfc_setup();

void pfc_print();

void pfc_reset();

// macros for starting / stopping pfc
#define PFC_TIC __asm("\
        .intel_syntax noprefix                  \t\n\
        push rax                                \t\n\
        lahf                                    \t\n\
        seto al                                 \t\n\
        push rax                                \t\n\
        push rcx                                \t\n\
        push rdx                                \t\n\
        push r15                                \t\n\
        mov r15,  pfc_mem            \t\n\
        mov rcx, 0                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        sub [r15 + 0], rdx                      \t\n\
        mov rcx, 1                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        sub [r15 + 8], rdx                      \t\n\
        mov rcx, 2                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        sub [r15 + 16], rdx                     \t\n\
        mov rcx, 3                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        sub [r15 + 24], rdx                     \t\n\
        lfence                                  \t\n\
        pop r15; lfence                         \t\n\
        pop rdx; lfence                         \t\n\
        pop rcx; lfence                         \t\n\
        pop rax; lfence                         \t\n\
        cmp al, -127; lfence                    \t\n\
        sahf; lfence                            \t\n\
        pop rax;                                \t\n\
        lfence                                  \t\n\
        .att_syntax noprefix                   \t\n");
// insert code in between
#define PFC_TOC __asm ("\
        .intel_syntax noprefix                  \t\n\
        lfence                                  \t\n\
        mov r15, pfc_mem            \t\n\
        mov rcx, 0                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        add [r15 + 0], rdx                      \t\n\
        mov rcx, 1                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        add [r15 + 8], rdx                      \t\n\
        mov rcx, 2                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        add [r15 + 16], rdx                     \t\n\
        mov rcx, 3                              \t\n\
        lfence; rdpmc; lfence                   \t\n\
        shl rdx, 32; or rdx, rax                \t\n\
        add [r15 + 24], rdx                     \t\n\
        lfence                                  \t\n\
        .att_syntax noprefix                    \t\n");



#endif

