#ifndef CALCULATE_FORCE_CUDA_KERNEL
#define CALCULATE_FORCE_CUDA_KERNEL

#ifdef GOMC_CUDA
#include <vector>
#include "XYZArray.h"
#include "BoxDimensions.h"
#include "VariablesCUDA.h"

using namespace std;

void CallBoxInterForceGPU(VariablesCUDA *vars,
			  vector<uint> pair1,
			  vector<uint> pair2,
			  XYZArray const &currentCoords,
			  XYZArray const &currentCOM,
			  BoxDimensions const& boxAxes,
			  bool electrostatic,
			  vector<double> particleCharge,
			  vector<int> particleKind,
			  vector<int> particleMol,
			  double &virInter,
			  double &virReal,
			  uint const box);

__global__ void BoxInterForceGPU(int *gpu_pair1,
				 int *gpu_pair2,
				 double *gpu_x,
				 double *gpu_y,
				 double *gpu_z,
				 double *gpu_comx,
				 double *gpu_comy,
				 double *gpu_comz,
				 double xAxes,
				 double yAxes,
				 double zAxes,
				 bool electrostatic,
				 double *gpu_particleCharge,
				 int *gpu_particleKind,
				 int *gpu_particleMol,
				 double *gpu_virInter,
				 double *gpu_virReal,
				 int pairSize,
				 double *gpu_sigmaSq,
				 double *gpu_epsilon_Cn,
				 double *gpu_n,
				 int *gpu_VDW_Kind,
				 int *gpu_isMartini,
				 int *gpu_count,
				 double *gpu_rCut,
				 double *gpu_rCutLow,
				 double *gpu_rOn,
				 double *gpu_alpha,
				 int *gpu_ewald,
				 double *gpu_diElectric_1);

__device__ double CalcCoulombForceGPU(double distSq, double qi_qj,
				      int gpu_VDW_Kind,
				      int gpu_ewald,
				      int gpu_isMartini,
				      double gpu_alpha,
				      double gpu_rCut,
				      double gpu_diElectric_1);
__device__ double CalcEnForceGPU(double distSq, int kind1, int kind2,
				 double *gpu_sigmaSq,
				 double *gpu_n,
				 double *gpu_epsilon_Cn,
				 double gpu_rCut,
				 double gpu_rOn,
				 int gpu_isMartini,
				 int gpu_VDW_Kind,
				 int gpu_count);

//ElectroStatic Calculation
//**************************************************************//
__device__ double CalcCoulombVirParticleGPU(double distSq, double qi_qj,
					    double gpu_alpha);
__device__ double CalcCoulombVirShiftGPU(double distSq, double qi_qj,
					 int gpu_ewald, double gpu_alpha);
__device__ double CalcCoulombVirSwitchMartiniGPU(double distSq, double qi_qj,
						 int gpu_ewald,
						 double gpu_alpha,
						 double gpu_rCut,
						 double gpu_diElectric_1);
__device__ double CalcCoulombVirSwitchGPU(double distSq, double qi_qj,
					  int gpu_ewald, double gpu_alpha,
					  double gpu_rCut);

//VDW Calculation
//*****************************************************************//
__device__ double CalcVirParticleGPU(double distSq, int index,
				     double *gpu_sigmaSq, double *gpu_n,
				     double *gpu_epsilon_Cn);
__device__ double CalcVirShiftGPU(double distSq, int index,
				  double *gpu_sigmaSq, double *gpu_n,
				  double *gpu_epsilon_Cn);
__device__ double CalcVirSwitchMartiniGPU(double distSq, int index,
					  double *gpu_sigmaSq, double *gpu_n,
					  double *gpu_epsilon_Cn,
					  double gpu_rCut, double rOn);
__device__ double CalcVirSwitchGPU(double distSq, int index,
				   double *gpu_sigmaSq, double *gpu_epsilon_Cn,
				   double *gpu_n, double gpu_rCut,
				   double gpu_rOn);

#endif /*GOMC_CUDA*/
#endif /*CALCULATE_FORCE_CUDA_KERNEL*/
