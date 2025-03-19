# RISC-V-behavioral
Support for RISC-V behavioral model.


# For execution of the Toy simulator (In order):
1. Run this command in the directory (as of now may be updated later...):
* g++ -std=c++11 src/*.cpp -o "name-for-your-file"

2. To observe the memory dump of the operations from the examples (as of now MatrixMul [Simplest AI Kernel]):
* ./"name-for-your-file" examples/<name-of-examplefile-assembly.s>