#ifndef __MY_CPUID_H__
#define __MY_CPUID_H__
#include <stdint.h>
typedef struct x64emu_s x64emu_t;

void my_cpuid(x64emu_t* emu, uint32_t tmp32u);

#endif //__MY_CPUID_H__