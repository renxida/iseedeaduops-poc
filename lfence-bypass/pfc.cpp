#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   
#include <cpuid.h>   
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <iomanip>

#include "pfc.h"
int64_t* pfc_mem;
using namespace std;

#ifndef MSR_IA32_PMC0
#define MSR_IA32_PMC0               0x0C1
#endif
#ifndef MSR_IA32_PERFEVTSEL0
#define MSR_IA32_PERFEVTSEL0        0x186
#endif
#ifndef MSR_OFFCORE_RSP0
#define MSR_OFFCORE_RSP0            0x1A6
#endif
#ifndef MSR_OFFCORE_RSP1
#define MSR_OFFCORE_RSP1            0x1A7
#endif
#ifndef MSR_IA32_FIXED_CTR0
#define MSR_IA32_FIXED_CTR0         0x309
#endif
#ifndef MSR_IA32_FIXED_CTR_CTRL
#define MSR_IA32_FIXED_CTR_CTRL     0x38D
#endif
#ifndef MSR_IA32_PERF_GLOBAL_CTRL
#define MSR_IA32_PERF_GLOBAL_CTRL   0x38F
#endif
#ifndef MSR_PEBS_FRONTEND
#define MSR_PEBS_FRONTEND           0x3F7
#endif
#ifndef CORE_X86_MSR_PERF_CTL
#define CORE_X86_MSR_PERF_CTL       0xC0010200
#endif
#ifndef CORE_X86_MSR_PERF_CTR
#define CORE_X86_MSR_PERF_CTR       0xC0010201
#endif


#define print_error(...) fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define print_verbose(...) if (verbose) printf(__VA_ARGS__);
#define print_user_verbose(...) if (verbose) printf(__VA_ARGS__);
#define nb_strtoul(s, base, res) *res = strtoul(s, NULL, base)

struct pfc_config {
    unsigned long evt_num;
    unsigned long umask;
    unsigned long cmask;
    unsigned int any;
    unsigned int edge;
    unsigned int inv;    
    unsigned long msr_3f6h;
    unsigned long msr_pf;
    unsigned long msr_rsp0;
    unsigned long msr_rsp1;
    unsigned int invalid;
    char* description;
};
struct pfc_config pfc_configs[1000] = {{0}};
size_t n_pfc_configs = 0;
char pfc_config_file_content_hardcoded[1024] = "C6.01.CTR=0.MSR_PF=0x12 FRONTEND_RETIRED.L1I_MISS\n79.08 IDQ.DSB_UOPS\nAB.02 DSB2MITE_SWITCHES.PENALTY_CYCLES\n79.04 IDQ.MITE_UOPS";


struct msr_config {
    unsigned long rdmsr;
    unsigned long wrmsr[10];
    unsigned long wrmsr_val[10]; 
    size_t n_wrmsr;
    char* description;
};
struct msr_config msr_configs[1000] = {{0}};
size_t n_msr_configs = 0;
char* msr_config_file_content = NULL;
unsigned long cur_rdmsr = 0;


int is_Intel_CPU = 0;
int is_AMD_CPU = 0;

int n_programmable_counters;

int cpu = -1;

int verbose = 0;
#define MAX_PROGRAMMABLE_COUNTERS 6
// where to put measurement results
int64_t measurement_results[MAX_PROGRAMMABLE_COUNTERS];




void build_cpuid_string(char* buf, unsigned int r0, unsigned int r1, unsigned int r2, unsigned int r3) {
    memcpy(buf,    (char*)&r0, 4);
    memcpy(buf+4,  (char*)&r1, 4);
    memcpy(buf+8,  (char*)&r2, 4);
    memcpy(buf+12, (char*)&r3, 4);
}

int check_cpuid() {
    unsigned int eax, ebx, ecx, edx;
    __cpuid(0, eax, ebx, ecx, edx);

    char proc_vendor_string[17] = {0};
    build_cpuid_string(proc_vendor_string, ebx, edx, ecx, 0);
    print_user_verbose("Vendor ID: %s\n", proc_vendor_string);

    char proc_brand_string[48];
    __cpuid(0x80000002, eax, ebx, ecx, edx);
    build_cpuid_string(proc_brand_string, eax, ebx, ecx, edx);
    __cpuid(0x80000003, eax, ebx, ecx, edx);
    build_cpuid_string(proc_brand_string+16, eax, ebx, ecx, edx);
    __cpuid(0x80000004, eax, ebx, ecx, edx);
    build_cpuid_string(proc_brand_string+32, eax, ebx, ecx, edx);
    print_user_verbose("Brand: %s\n", proc_brand_string);

    __cpuid(0x01, eax, ebx, ecx, edx);
    unsigned int displ_family = ((eax >> 8) & 0xF);
    if (displ_family == 0x0F) {
        displ_family += ((eax >> 20) & 0xFF);
    }
    unsigned int displ_model = ((eax >> 4) & 0xF);
    if (displ_family == 0x06 || displ_family == 0x0F) {
        displ_model += ((eax >> 12) & 0xF0);
    }
    print_user_verbose("DisplayFamily_DisplayModel: %.2X_%.2XH\n", displ_family, displ_model);
    print_user_verbose("Stepping ID: %u\n", (eax & 0xF));

    if (strcmp(proc_vendor_string, "GenuineIntel") == 0) {
        is_Intel_CPU = 1;

        __cpuid(0x0A, eax, ebx, ecx, edx);
        unsigned int perf_mon_ver = (eax & 0xFF);
        print_user_verbose("Performance monitoring version: %u\n", perf_mon_ver);
        if (perf_mon_ver < 2) {
            print_error("Error: performance monitoring version >= 2 required\n");
            return 1;
        }

        unsigned int n_available_counters = ((eax >> 8) & 0xFF);
        print_user_verbose("Number of general-purpose performance counters: %u\n", n_available_counters);
        if (n_available_counters >= 4) {
            n_programmable_counters = 4;
        } else if (n_available_counters >= 2) {
            n_programmable_counters = 2;
        } else {
            print_error("Error: only %u programmable counters available; nanoBench requires at least 2\n", n_available_counters);
            return 1;
        }

        print_user_verbose("Bit widths of general-purpose performance counters: %u\n", ((eax >> 16) & 0xFF));

    } else if (strcmp(proc_vendor_string, "AuthenticAMD") == 0) {
        is_AMD_CPU = 1;
        n_programmable_counters = 6;
    } else {
        print_error("Error: unsupported CPU found\n");
        return 1;
    }

    return 0;
}



void parse_counter_configs() {
    n_pfc_configs = 0;

    char* line;

    size_t len = strlen(pfc_config_file_content_hardcoded);
    char* pfc_config_file_content = (char*)calloc(len+1, sizeof(char));
    memcpy(pfc_config_file_content, pfc_config_file_content_hardcoded, len);
    
    char* next_line = pfc_config_file_content;
    while ((line = strsep(&next_line, "\n")) != NULL) {
        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        pfc_configs[n_pfc_configs].invalid = 0;

        char* config_str = strsep(&line, " \t");

        if (line && strlen(line) > 0) {
            pfc_configs[n_pfc_configs].description = line;
        } else {
            pfc_configs[n_pfc_configs].description = config_str;
        }

        char buf[50];
        if (strlen(config_str) >= sizeof(buf)) {
            print_error("config string too long: %s\n", config_str);
            continue;
        }
        strcpy(buf, config_str);

        char* tok = buf;

        char* evt_num = strsep(&tok, ".");
        nb_strtoul(evt_num, 16, &(pfc_configs[n_pfc_configs].evt_num));

        if (!tok) {
            print_error("invalid configuration: %s\n", config_str);
            continue;
        }

        char* umask = strsep(&tok, ".");
        nb_strtoul(umask, 16, &(pfc_configs[n_pfc_configs].umask));

        char* ce;
        while ((ce = strsep(&tok, ".")) != NULL) {
            if (!strcmp(ce, "AnyT")) {
                pfc_configs[n_pfc_configs].any = 1;
            } else if (!strcmp(ce, "EDG")) {
                pfc_configs[n_pfc_configs].edge = 1;
            } else if (!strcmp(ce, "INV")) {
                pfc_configs[n_pfc_configs].inv = 1;
            } else if (!strncmp(ce, "CTR=", 4)) {
                unsigned long counter;
                nb_strtoul(ce+4, 0, &counter);
                if (counter > n_programmable_counters) {
                    print_error("invalid counter: %s\n", ce);
                    continue;
                }
                size_t prev_n_pfc_configs = n_pfc_configs;
                while (n_pfc_configs % n_programmable_counters != counter) {
                    pfc_configs[n_pfc_configs].invalid = 1;
                    n_pfc_configs++;
                }
                if (prev_n_pfc_configs != n_pfc_configs) {
                    pfc_configs[n_pfc_configs] = pfc_configs[prev_n_pfc_configs];
                    pfc_configs[n_pfc_configs].invalid = 0;
                }
            } else if (!strncmp(ce, "CMSK=", 5)) {
                nb_strtoul(ce+5, 0, &(pfc_configs[n_pfc_configs].cmask));
            } else if (!strncmp(ce, "MSR_3F6H=", 9)) {
                nb_strtoul(ce+9, 0, &(pfc_configs[n_pfc_configs].msr_3f6h));
            } else if (!strncmp(ce, "MSR_PF=", 7)) {
                nb_strtoul(ce+7, 0, &(pfc_configs[n_pfc_configs].msr_pf));
            } else if (!strncmp(ce, "MSR_RSP0=", 9)) {
                nb_strtoul(ce+9, 0, &(pfc_configs[n_pfc_configs].msr_rsp0));
            } else if (!strncmp(ce, "MSR_RSP1=", 9)) {
                nb_strtoul(ce+9, 0, &(pfc_configs[n_pfc_configs].msr_rsp1));
            }
        }
        n_pfc_configs++;
    }
}


uint64_t read_value_from_cmd(char* cmd) {
    FILE* fp;
    if(!(fp = popen(cmd, "r"))){
        print_error("Error reading from \"%s\"", cmd);
        return 0;
    }

    char buf[20];
    fgets(buf, sizeof(buf), fp);
    pclose(fp);

    uint64_t val;
    nb_strtoul(buf, 0, &val);
    return val;
}

uint64_t read_msr(unsigned int msr) {
    #ifdef __KERNEL__
        return native_read_msr(msr);
    #else
        char cmd[50];
        snprintf(cmd, sizeof(cmd), "rdmsr -c -p%d %#x", cpu, msr);
        return read_value_from_cmd(cmd);
    #endif
}

void write_msr(unsigned int msr, uint64_t value) {
    #ifdef __KERNEL__
        native_write_msr(msr, (uint32_t)value, (uint32_t)(value>>32));
    #else
        char cmd[50];
        snprintf(cmd, sizeof(cmd), "wrmsr -p%d %#x %#lx", cpu, msr, value);
        if (system(cmd)) {
            print_error("\"%s\" failed. You may need to disable Secure Boot (see README.md).", cmd);
            exit(1);
        }
    #endif
}


void configure_perf_ctrs_FF(unsigned int usr, unsigned int os) {
    if (is_Intel_CPU) {
        uint64_t global_ctrl = read_msr(MSR_IA32_PERF_GLOBAL_CTRL);
        global_ctrl |= ((uint64_t)7 << 32) | 15;
        write_msr(MSR_IA32_PERF_GLOBAL_CTRL, global_ctrl);

        uint64_t fixed_ctrl = read_msr(MSR_IA32_FIXED_CTR_CTRL);
        // disable fixed counters
        fixed_ctrl &= ~((1 << 12) - 1);
        write_msr(MSR_IA32_FIXED_CTR_CTRL, fixed_ctrl);
        // clear
        for (int i=0; i<3; i++) {
            write_msr(MSR_IA32_FIXED_CTR0+i, 0);
        }
        //enable fixed counters
        fixed_ctrl |= (os << 8) | (os << 4) | os;
        fixed_ctrl |= (usr << 9) | (usr << 5) | (usr << 1);
        write_msr(MSR_IA32_FIXED_CTR_CTRL, fixed_ctrl);
    }
}
void configure_perf_ctrs_programmable(int start, int end, unsigned int usr, unsigned int os) {
    if (is_Intel_CPU) {
        uint64_t global_ctrl = read_msr(MSR_IA32_PERF_GLOBAL_CTRL);
        global_ctrl |= ((uint64_t)7 << 32) | 15;
        write_msr(MSR_IA32_PERF_GLOBAL_CTRL, global_ctrl);

        for (int i=0; i<n_programmable_counters; i++) {
            uint64_t perfevtselx = read_msr(MSR_IA32_PERFEVTSEL0+i);

            // disable counter i
            perfevtselx &= ~(((uint64_t)1 << 32) - 1);
            write_msr(MSR_IA32_PERFEVTSEL0+i, perfevtselx);

            // clear
            write_msr(MSR_IA32_PMC0+i, 0);

            if (start+i >= end) {
                continue;
            }

            // configure counter i
            struct pfc_config config = pfc_configs[start+i];
            //printf("Configuring:%s\n", config.description);
            if (config.invalid) {
                printf("Read invalid cfg\n");
                continue;
            }
            perfevtselx |= ((config.cmask & 0xFF) << 24);
            perfevtselx |= (config.inv << 23);
            perfevtselx |= (1ULL << 22);
            perfevtselx |= (config.any << 21);
            perfevtselx |= (config.edge << 18);
            perfevtselx |= (os << 17);
            perfevtselx |= (usr << 16);
            perfevtselx |= ((config.umask & 0xFF) << 8);
            perfevtselx |= (config.evt_num & 0xFF);
            write_msr(MSR_IA32_PERFEVTSEL0+i, perfevtselx);

            if (config.msr_3f6h) {
                write_msr(0x3f6, config.msr_3f6h);
            }

            if (config.msr_pf) {
                write_msr(MSR_PEBS_FRONTEND, config.msr_pf);
            }

            if (config.msr_rsp0) {
                write_msr(MSR_OFFCORE_RSP0, config.msr_rsp0);
            }
            if (config.msr_rsp1) {
                write_msr(MSR_OFFCORE_RSP1, config.msr_rsp1);
            }
        }
    } else {
        for (int i=0; i<n_programmable_counters; i++) {
            printf("Measurement code not adapted fro AMD");
            exit(-1);
            // clear
            write_msr(CORE_X86_MSR_PERF_CTR+(2*i), 0);

            if (start+i >= end) {
                write_msr(CORE_X86_MSR_PERF_CTL + (2*i), 0);
                continue;
            }

            struct pfc_config config = pfc_configs[start+i];

            uint64_t perf_ctl = 0;
            perf_ctl |= ((config.evt_num) & 0xF00) << 24;
            perf_ctl |= (config.evt_num) & 0xFF;
            perf_ctl |= ((config.umask) & 0xFF) << 8;
            perf_ctl |= ((config.cmask) & 0x7F) << 24;
            perf_ctl |= (config.inv << 23);
            perf_ctl |= (1ULL << 22);
            perf_ctl |= (config.edge << 18);
            perf_ctl |= (os << 17);
            perf_ctl |= (usr << 16);

            write_msr(CORE_X86_MSR_PERF_CTL + (2*i), perf_ctl);
        }
    }
}


size_t mmap_file(char* filename, char** content) {
    int fd = open(filename, O_RDONLY);
    size_t len = lseek(fd, 0, SEEK_END);
    *content = (char*)mmap(0, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (*content == MAP_FAILED) {
        fprintf(stderr, "Error reading %s\n", filename);
        exit(1);
    }
    close(fd);
    return len;
}

void pfc_print(){
    for(size_t i = 0; i < 4; i++){
        struct pfc_config config = pfc_configs[i];
        int64_t val;
        val = pfc_mem[i];
        cout << right << setw(20) << val << "\t" << config.description << endl;
    }
    cout << right << setw(20) << pfc_mem[1] + pfc_mem[3] << "\tTOTAL_UOPS" << endl;
}

void pfc_setup(){
    int usr = 1; // count events in user-mode
    int os = 0;  // no count events in os-mode (privileged mode)

    char config_file_name[16] = "config.txt";
    /*************************************
     * Check CPUID
     ************************************/
    if (check_cpuid()) {
        printf("check_cpuid() failed\n");
        exit(-1);
    }


    /*************************************
     * Pin thread to CPU
     ************************************/
    if (cpu == -1) {
        cpu = sched_getcpu();
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        fprintf(stderr, "Error: Could not pin thread to core %d\n", cpu);
        exit(-1);
    }

    /*************************************
     * Enable Performance Counting
     ************************************/
    configure_perf_ctrs_FF(usr, os);


    /*************************************
     * Get hardcoded Configs
     ************************************/
    parse_counter_configs();


    /*************************************
     * Apply Configs
     ************************************/
    if(n_pfc_configs >n_programmable_counters){
        printf("More counters specified than CPU can handle: %zd > %d\n", n_pfc_configs, n_programmable_counters);
        exit(-1);
    }
    configure_perf_ctrs_programmable(0, n_pfc_configs, usr, os);

    /*************************************
     * Set Performance Counter Registers to zero
     ************************************/
    pfc_mem = (int64_t*)calloc(sizeof(int64_t), MAX_PROGRAMMABLE_COUNTERS);
}

void pfc_reset(){
    for(int i = 0; i < MAX_PROGRAMMABLE_COUNTERS; i++){
        pfc_mem[i]=0;
    }
}

void inline __attribute__((always_inline)) pfc_tic(){
    asm volatile(
        ".intel_syntax noprefix                  \n"
        "push rax                                \n"
        "lahf                                    \n"
        "seto al                                 \n"
        "push rax                                \n"
        "push rcx                                \n"
        "push rdx                                \n"
        "push r15                                \n"
        "mov r15,  pfc_mem            \n"
        "mov qword ptr [r15 + 0], 0              \n"
        "mov qword ptr [r15 + 8], 0              \n"
        "mov qword ptr [r15 + 16], 0             \n"
        "mov qword ptr [r15 + 24], 0             \n"
        "mov rcx, 0                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "sub [r15 + 0], rdx                      \n"
        "mov rcx, 1                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "sub [r15 + 8], rdx                      \n"
        "mov rcx, 2                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "sub [r15 + 16], rdx                     \n"
        "mov rcx, 3                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "sub [r15 + 24], rdx                     \n"
        "lfence                                  \n"
        "pop r15; lfence                         \n"
        "pop rdx; lfence                         \n"
        "pop rcx; lfence                         \n"
        "pop rax; lfence                         \n"
        "cmp al, -127; lfence                    \n"
        "sahf; lfence                            \n"
        "pop rax;                                \n"
        "lfence                                  \n"
        ".att_syntax noprefix                    ");
}

void inline __attribute__((always_inline)) pfc_toc(){

    asm volatile(
        ".intel_syntax noprefix                  \n"
        "lfence                                  \n"
        "mov r15, pfc_mem            \n"
        "mov rcx, 0                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "add [r15 + 0], rdx                      \n"
        "mov rcx, 1                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "add [r15 + 8], rdx                      \n"
        "mov rcx, 2                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "add [r15 + 16], rdx                     \n"
        "mov rcx, 3                              \n"
        "lfence; rdpmc; lfence                   \n"
        "shl rdx, 32; or rdx, rax                \n"
        "add [r15 + 24], rdx                     \n"
        "lfence                                  \n"
        ".att_syntax noprefix                    ");
}


int example_main(int argc, char** argv){
    pfc_setup();


    pfc_tic();

    int a = 1;
    for(int i = 0; i < 1000; i ++){
        a += 1;
    }

    pfc_toc();

    pfc_print();

    cout << "a:" << a << endl;
    return 0;
}
/*
int main(int argc, char** argv){
	printf("here\n");
    	pfc_setup();
	pfc_reset();
	printf("1\n");
   PFC_TIC; 

	printf("2\n");
    int a = 1;
    for(int i = 0; i < 1000; i ++){
        a += 1;
    }
	printf("3\n");
    PFC_TOC;
	printf("4");
    pfc_print();

    cout << "a:" << a << endl;
    return 0;
}*/
