#include<stdio.h>
#include<assert.h>

int main()
{
    int rs, rt, dsp;
    int ach = 5, acl = 5;
    int resulth, resultl, resultdsp;

    rs     = 0x00FF00FF;
    rt     = 0x00010002;
    resulth = 0x05;
    resultl = 0x05FF;
    resultdsp = 0x00;
    __asm
        ("mthi  %0, $ac1\n\t"
         "mtlo  %1, $ac1\n\t"
         "dpaqx_sa.w.ph $ac1, %3, %4\n\t"
         "mfhi  %0, $ac1\n\t"
         "mflo  %1, $ac1\n\t"
         "rddsp %2\n\t"
         : "+r"(ach), "+r"(acl), "=r"(dsp)
         : "r"(rs), "r"(rt)
        );
    assert(dsp == resultdsp);
    assert(ach == resulth);
    assert(acl == resultl);

    return 0;
}
