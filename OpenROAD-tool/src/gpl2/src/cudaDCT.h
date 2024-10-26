#include <cufft.h>

#include "cudaUtil.h"

void dct_2d_fft(const int M,
                const int N,
                cufftHandle& plan,
                const cufftComplex* expkM,
                const cufftComplex* expkN,
                const cufftReal* input,
                cufftReal* pre,
                cufftComplex* fft,
                cufftReal* post);

void idct_2d_fft(const int M,
                 const int N,
                 cufftHandle& plan,
                 const cufftComplex* expkM,
                 const cufftComplex* expkN,
                 const cufftComplex* expkMN1,
                 const cufftComplex* expkMN2,
                 const cufftReal* input,
                 cufftComplex* pre,
                 cufftReal* ifft,
                 cufftReal* post);

void idxst_idct(const int M,
                const int N,
                cufftHandle& plan,
                const cufftComplex* expkM,
                const cufftComplex* expkN,
                const cufftComplex* expkMN1,
                const cufftComplex* expkMN2,
                const cufftReal* input,
                cufftReal* workSpaceReal1,
                cufftComplex* workSpaceComplex,
                cufftReal* workSpaceReal2,
                cufftReal* workSpaceReal3,
                cufftReal* output);

void idct_idxst(const int M,
                const int N,
                cufftHandle& plan,
                const cufftComplex* expkM,
                const cufftComplex* expkN,
                const cufftComplex* expkMN1,
                const cufftComplex* expkMN2,
                const cufftReal* input,
                cufftReal* workSpaceReal1,
                cufftComplex* workSpaceComplex,
                cufftReal* workSpaceReal2,
                cufftReal* workSpaceReal3,
                cufftReal* output);
