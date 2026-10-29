# matmul trace (N=64)
Real memory-access trace of a 64x64 double matrix multiply (gcc -O0), in ccsim trace v1 format.
- Recorded: `valgrind --tool=lackey --trace-mem=yes ./matmul`
- Converted with awk: instruction fetches and access sizes dropped; M -> R then W.
- Unpack: `gunzip -k matmul.trace.gz`
- Not a golden fixture; for later benchmarking only.
