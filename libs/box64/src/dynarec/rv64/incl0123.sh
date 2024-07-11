#!/bin/bash

strings=(
	dynarec_rv64_0f
	dynarec_rv64_64
	dynarec_rv64_66
	dynarec_rv64_660f
	dynarec_rv64_6664
	dynarec_rv64_66f0
	dynarec_rv64_66f20f
	dynarec_rv64_66f30f
	dynarec_rv64_67
	dynarec_rv64_67_32
	dynarec_rv64_d8
	dynarec_rv64_d9
	dynarec_rv64_da
	dynarec_rv64_db
	dynarec_rv64_dc
	dynarec_rv64_dd
	dynarec_rv64_de
	dynarec_rv64_df
	dynarec_rv64_emit_logic
	dynarec_rv64_emit_math
	dynarec_rv64_emit_shift
	dynarec_rv64_emit_tests
	dynarec_rv64_f0
	dynarec_rv64_f20f
	dynarec_rv64_f30f
	dynarec_rv64_helper
)
for f in "${strings[@]}"; do
#tail -n +2 ${f}_0.c > ${f}.c
echo -e "#define STEP 0\n#include \"${f}.c\"" > ${f}_0.c
echo -e "#define STEP 1\n#include \"${f}.c\"" > ${f}_1.c
echo -e "#define STEP 2\n#include \"${f}.c\"" > ${f}_2.c
echo -e "#define STEP 3\n#include \"${f}.c\"" > ${f}_3.c
done
