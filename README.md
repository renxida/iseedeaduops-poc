# What is this

This is proof-of-concept code for our paper [I See Dead micro-ops: Leaking Secrets via Intel/AMD Micro-Op Caches](https://ieeexplore.ieee.org/abstract/document/9499837), published in ISCA 2021.

# Files in this

- variant-1: this is a modified Spectre-v1 attack that uses the micro-op cache instead of the LLC
- lfence-bypass: this is a proof-of-concept that the micro-op cache can make inserting LFENCE useless against spectre attacks
- environment1: this contains a script to collect environmental information and a record of the environment on which the POC code was run.

# How to use this

Clone this repository into a linux server with build-essential installed.
We used a Equinix s3.large.x86 server (Coffee Lake architecture) to test.

Each folder contains a ./run script and the lfence-bypass contains a README with extra instructions.

