#include <cmath>
#include <cassert>
#include <memory>
#include <stdio.h>
#include <cufft.h>

#include <fstream>
#include <iostream>

#include <cuda_runtime.h>
#include <thrust/transform_reduce.h>
#include <thrust/functional.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

#include "cudaDCT.h"
#include "cudaUtil.h"
#include "poissonSolver.h"

namespace gpl2
{

__global__ void precomputeExpk(cufftComplex *expkM, 
                               cufftComplex *expkN,
                               const int M, 
                               const int N)
{
  const int tID = blockDim.x * blockIdx.x + threadIdx.x;
	
  if(tID <= M / 2)
  {
    int hID = tID;
    cufftComplex W_h_4M = make_float2(__cosf((float)PI * hID / (2 * M)), 
                                     -__sinf((float)PI * hID / (M * 2)));
    expkM[hID] = W_h_4M;
  }
  if(tID <= N / 2)
  {
    int wid = tID;
    cufftComplex W_w_4N = make_float2(__cosf((float)PI * wid / (2 * N)), 
                                     -__sinf((float)PI * wid / (N * 2)));
    expkN[wid] = W_w_4N;
  }
}

__global__ void precomputeExpkForInverse(cufftComplex *expkM, 
                                         cufftComplex *expkN, 
                                         cufftComplex *expkMN_1, 
                                         cufftComplex *expkMN_2, 
                                         const int M, 
                                         const int N)
{
  const int tid = blockDim.x * blockIdx.x + threadIdx.x;
  if(tid < M)
  {
    int hid = tid;
    cufftComplex W_h_4M = make_float2(__cosf((float)PI * hid / (2 * M)), 
                                       -__sinf((float)PI * hid / (M * 2)));
    expkM[hid] = W_h_4M;
    // expkMN_1
    cufftComplex W_h_4M_offset = make_float2(__cosf((float)PI * (hid + M) / (2 * M)), 
                                              -__sinf((float)PI * (hid + M) / (M * 2)));
    expkMN_1[hid] = W_h_4M;
    expkMN_1[hid + M] = W_h_4M_offset;
  
    //expkMN_2
    W_h_4M = make_float2(-__sinf((float)PI * (hid - (N - 1)) / (M * 2)), 
                         -__cosf((float)PI * (hid - (N - 1)) / (2 * M)));

    W_h_4M_offset = make_float2(-__sinf((float)PI * (hid - (N - 1) + M) / (M * 2)), 
                                  -__cosf((float)PI * (hid - (N - 1) + M) / (2 * M)));
    expkMN_2[hid] = W_h_4M;
    expkMN_2[hid + M] = W_h_4M_offset;
  }
  if(tid <= N / 2)
  {
    int wid = tid;
    cufftComplex W_w_4N = make_float2(__cosf((float)PI * wid / (2 * N)), 
                                     -__sinf((float)PI * wid / (N * 2)));
    expkN[wid] = W_w_4N;
  }
}

__global__ void divideByWSquare(const int binCntX,
                                const int binCntY,
                                const int binSizeX,
                                const int binSizeY,
                                cufftReal* input)
{
  //const int binID = blockDim.x * blockIdx.x + threadIdx.x;

  const int wID = blockDim.x * blockIdx.x + threadIdx.x;
  const int hID = blockDim.y * blockIdx.y + threadIdx.y;

  if(wID < binCntX && hID < binCntY)
  {
    int binID = wID + hID * binCntX;

    if(hID == 0 && wID == 0)
      input[binID] = 0.0;
    else
    {
      float denom1 = (2.0 * float(FFT_PI) * wID) / binCntX; 
      float denom2 = (2.0 * float(FFT_PI) * hID) / binCntY * binSizeY / binSizeX;

      input[binID] /= (denom1 * denom1 + denom2 * denom2);
    }
  }
}

__global__ void multiplyW(const int binCntX,
                          const int binCntY,
                          const int binSizeX,
                          const int binSizeY,
                          const cufftReal* auv, 
                          cufftReal* inputForX,
                          cufftReal* inputForY)
{
  //const int binID = blockDim.x * blockIdx.x + threadIdx.x;
  const int wID = blockDim.x * blockIdx.x + threadIdx.x;
  const int hID = blockDim.y * blockIdx.y + threadIdx.y;

  if(wID < binCntX && hID < binCntY)
  {
    int binID = wID + hID * binCntX;

    float w_u = (2.0 * float(FFT_PI) * wID) / binCntX; 
    float w_v = (2.0 * float(FFT_PI) * hID) / binCntY * binSizeY / binSizeX; 

    inputForX[binID] = w_u * auv[binID]; // why w_v?
    inputForY[binID] = w_v * auv[binID]; // why w_u?
  }
}


void
PoissonSolver::solvePoissonPotential(const float* binDensity,
                                           float* potential)
{
  int numThread = 16;

  dim3 gridSize( (binCntX_ + numThread - 1) / numThread, 
                 (binCntY_ + numThread - 1) / numThread, 1);

  dim3 blockSize(numThread, numThread, 1);

  // Step #1. Compute Coefficient (a_uv)
  dct_2d_fft(binCntY_, 
             binCntX_,
             plan_,
             d_expkM_,
             d_expkN_,
             binDensity, 
             d_workSpaceReal1_, 
             d_workSpaceComplex_, 
             d_auv_);

  // Step #2. Divide by (w_u^2 + w_v^2)
  divideByWSquare<<<gridSize, blockSize>>>(binCntX_, binCntY_, 
                                           binSizeX_, binSizeY_,
                                           d_auv_);

  // Step #3. Compute Potential
  idct_2d_fft(binCntY_, 
              binCntX_,
              planInverse_,
              d_expkMForInverse_,
              d_expkNForInverse_,
              d_expkMN1_,
              d_expkMN2_,
              d_auv_, 
              d_workSpaceComplex_, 
              d_workSpaceReal1_, 
              potential);
}



void
PoissonSolver::solvePoisson(const float* binDensity,
                                  float* potential,
                                  float* electroForceX,
                                  float* electroForceY)
{
  int numThread = 16;

  dim3 gridSize( (binCntX_ + numThread - 1) / numThread, 
                 (binCntY_ + numThread - 1) / numThread, 1);

  dim3 blockSize(numThread, numThread, 1);

  // Step #1. Compute Coefficient (a_uv)
  dct_2d_fft(binCntY_, 
             binCntX_,
             plan_,
             d_expkM_,
             d_expkN_,
             binDensity, 
             d_workSpaceReal1_, 
             d_workSpaceComplex_, 
             d_auv_);

  // Step #2. Divide by (w_u^2 + w_v^2)
  divideByWSquare<<<gridSize, blockSize>>>(binCntX_, binCntY_, 
                                           binSizeX_, binSizeY_,
                                           d_auv_);

  // Step #3. Compute Potential
  idct_2d_fft(binCntY_, 
              binCntX_,
              planInverse_,
              d_expkMForInverse_,
              d_expkNForInverse_,
              d_expkMN1_,
              d_expkMN2_,
              d_auv_, 
              d_workSpaceComplex_, 
              d_workSpaceReal1_, 
              potential);

  // Step #4. Multiply w_u , w_v
  multiplyW<<<gridSize, blockSize>>>(binCntX_, 
                                     binCntY_,
                                     binSizeX_,
                                     binSizeY_,
                                     d_auv_, 
                                     d_inputForX_, 
                                     d_inputForY_);

  // Step #5. Compute ElectroForceX
  idxst_idct(binCntY_, 
             binCntX_,
             planInverse_,
             d_expkMForInverse_,
             d_expkNForInverse_,
             d_expkMN1_,
             d_expkMN2_,
             d_inputForX_, 
             d_workSpaceReal1_, 
              d_workSpaceComplex_, 
             d_workSpaceReal2_, 
             d_workSpaceReal3_, 
             electroForceX);

  // Step #6. Compute ElectroForceY
  idct_idxst(binCntY_, 
             binCntX_,
             planInverse_,
             d_expkMForInverse_,
             d_expkNForInverse_,
             d_expkMN1_,
             d_expkMN2_,
             d_inputForY_, 
             d_workSpaceReal1_, 
             d_workSpaceComplex_, 
             d_workSpaceReal2_, 
             d_workSpaceReal3_, 
             electroForceY);

  cudaDeviceSynchronize();
}

void
PoissonSolver::setupForCUDAKernel()
{
  printf("[CUDA-POISSON] Setup for CUDA Kernel.\n");

  CUDA_CHECK(cudaMalloc((void**)&d_binDensity_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_auv_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_potential_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_efX_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_efY_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_workSpaceReal1_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_workSpaceReal2_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_workSpaceReal3_,
                         binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void**)&d_workSpaceComplex_,
                         (binCntX_ / 2 + 1) * binCntY_ * sizeof(cufftComplex)));

  // expk
  // For DCT2D
  CUDA_CHECK(cudaMalloc((void **)&d_expkM_,  
                        (binCntY_ / 2 + 1) * sizeof(cufftComplex)));

  CUDA_CHECK(cudaMalloc((void **)&d_expkN_,  
                        (binCntX_ / 2 + 1) * sizeof(cufftComplex)));

  // For IDCT2D & IDXST_IDCT & IDCT_IDXST
  CUDA_CHECK(cudaMalloc((void **)&d_expkMForInverse_,  
                        (binCntY_) * sizeof(cufftComplex)));

  CUDA_CHECK(cudaMalloc((void **)&d_expkNForInverse_,  
                        (binCntX_ / 2 + 1) * sizeof(cufftComplex)));

  CUDA_CHECK(cudaMalloc((void **)&d_expkMN1_,  
                        (binCntX_ + binCntY_) * sizeof(cufftComplex)));

  CUDA_CHECK(cudaMalloc((void **)&d_expkMN2_,  
                        (binCntX_ + binCntY_) * sizeof(cufftComplex)));

  // For Input For IDXST_IDCT & IDCT_IDXST
  CUDA_CHECK(cudaMalloc((void **)&d_inputForX_,  
                        binCntX_ * binCntY_ * sizeof(cufftReal)));

  CUDA_CHECK(cudaMalloc((void **)&d_inputForY_,  
                        binCntX_ * binCntY_ * sizeof(cufftReal)));

  int numThread = 1024;
  int numBin = std::max(binCntX_, binCntY_);
  int numBlock  = (numBin - 1 + numThread) / numThread;

  precomputeExpk<<<numBlock, numThread>>>(d_expkM_, 
                                          d_expkN_, 
                                          binCntY_, 
                                          binCntX_);

  precomputeExpkForInverse<<<numBlock, numThread>>>(d_expkMForInverse_, 
                                                    d_expkNForInverse_,
                                                    d_expkMN1_, 
                                                    d_expkMN2_,
                                                    binCntY_, 
                                                    binCntX_);

  cufftPlan2d(&plan_,        binCntY_, binCntX_, CUFFT_R2C);
  cufftPlan2d(&planInverse_, binCntY_, binCntX_, CUFFT_C2R);

  printf("[CUDA-POISSON] Finish Setup.\n");
}

void
PoissonSolver::freeDeviceMemory()
{
  CUDA_CHECK(cudaFree(d_binDensity_));
  CUDA_CHECK(cudaFree(d_auv_));
  CUDA_CHECK(cudaFree(d_potential_));

  CUDA_CHECK(cudaFree(d_efX_));
  CUDA_CHECK(cudaFree(d_efY_));

  CUDA_CHECK(cudaFree(d_workSpaceReal1_));
  CUDA_CHECK(cudaFree(d_workSpaceReal2_));
  CUDA_CHECK(cudaFree(d_workSpaceReal3_));

  CUDA_CHECK(cudaFree(d_workSpaceComplex_));

  CUDA_CHECK(cudaFree(d_expkN_));
  CUDA_CHECK(cudaFree(d_expkM_));

  CUDA_CHECK(cudaFree(d_expkNForInverse_));
  CUDA_CHECK(cudaFree(d_expkMForInverse_));

  CUDA_CHECK(cudaFree(d_expkMN1_));
  CUDA_CHECK(cudaFree(d_expkMN2_));

  CUDA_CHECK(cudaFree(d_inputForX_));
  CUDA_CHECK(cudaFree(d_inputForY_));

  cufftDestroy(plan_);
  cufftDestroy(planInverse_);
}

} // namespace 
