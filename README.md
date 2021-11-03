# What is this

This is proof-of-concept code for my paper [I See Dead micro-ops: Leaking Secrets via Intel/AMD Micro-Op Caches](https://www.cs.virginia.edu/venkat/papers/isca2021a.pdf), published [here at ISCA 2021](https://ieeexplore.ieee.org/abstract/document/9499837) with [Logan Moody](https://www.linkedin.com/in/loganmoody1), [Mohammadkazem Taram](https://mktrm.github.io/) and [Matthew Jordan](https://www.linkedin.com/in/matthew-jordan-67739615a) (click names to see LinkedIn).

We are advised by [Ashish Venkat](http://www.cs.virginia.edu/venkat/) and [Dean M. Tullsen](https://cseweb.ucsd.edu/~tullsen/) in this publication.

# Files in this

- variant-1: this is a modified Spectre-v1 attack that uses the micro-op cache instead of the LLC
- lfence-bypass: this is a proof-of-concept that the micro-op cache can make inserting LFENCE useless against spectre attacks
- environment1: this contains a script to collect environmental information and a record of the environment on which the POC code was run.

# How to use this

Clone this repository into a linux server with build-essential installed.
We used a [Equinix](https://metal.equinix.com/) s3.large.x86 server (Coffee Lake architecture) to test.

Each folder contains a ./run script and the lfence-bypass contains a README with extra instructions.

