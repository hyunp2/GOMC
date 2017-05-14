#ifndef CONSTANT_DEFINITIONS_CUDA_KERNEL
#define CONSTANT_DEFINITIONS_CUDA_KERNEL

#ifdef GOMC_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#include "GeomLib.h"

extern __constant__ double gpu_sigmaSq[1000];
extern __constant__ double gpu_epsilon_Cn[1000];
extern __constant__ double gpu_n[1000];
extern __constant__ int gpu_VDW_Kind;
extern __constant__ int gpu_count;
extern __constant__ bool gpu_isMartini;
extern __constant__ double gpu_rCut;
extern __constant__ double gpu_rCutLow;
extern __constant__ double gpu_rOn;
extern __constant__ double gpu_alpha;
extern __constant__ bool gpu_ewald;
extern __constant__ double gpu_diElectric_1;

#define GPU_VDW_STD_KIND 0
#define GPU_VDW_SHIFT_KIND 1
#define GPU_VDW_SWITCH_KIND 2

inline void InitGPUForceField(double const *sigmaSq, double const *epsilon_Cn,
		       double const *n, uint VDW_Kind, bool isMartini,
		       int count, int Rcut, int RcutLow, int Ron, double alpha,
		       bool ewald, double diElectric_1)
{
  int countSq = count * count;
  cudaMemcpyToSymbol("gpu_VDW_Kind", &VDW_Kind, sizeof(int));
  cudaMemcpyToSymbol("gpu_isMartini", &isMartini, sizeof(bool));
  cudaMemcpyToSymbol("gpu_sigmaSq", &sigmaSq, countSq * sizeof(double));
  cudaMemcpyToSymbol("gpu_epsilon_Cn", &epsilon_Cn, countSq * sizeof(double));
  cudaMemcpyToSymbol("gpu_n", &n, countSq * sizeof(double));
  cudaMemcpyToSymbol("gpu_rCut", &Rcut, sizeof(double));
  cudaMemcpyToSymbol("gpu_rCutLow", &RcutLow, sizeof(double));
  cudaMemcpyToSymbol("gpu_rOn", &Ron, sizeof(double));
  cudaMemcpyToSymbol("gpu_count", &count, sizeof(int));
  cudaMemcpyToSymbol("gpu_alpha", &alpha, sizeof(double));
  cudaMemcpyToSymbol("gpu_ewald", &ewald, sizeof(bool));
  cudaMemcpyToSymbol("gpu_diElectric_1", &diElectric_1, sizeof(double));
}

__device__ inline int FlatIndexGPU(int i, int j)
{
  return i + j * gpu_count;
}

#endif /*GOMC_CUDA*/
#endif /*CONSTANT_DEFINITIONS_CUDA_KERNEL*/
