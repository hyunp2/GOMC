#include "Ewald.h"
#include "CalculateEnergy.h"        
#include "EnergyTypes.h"            //Energy structs
#include "EnsemblePreprocessor.h"   //Flags
#include "../lib/BasicTypes.h"             //uint
#include "System.h"                 //For init
#include "StaticVals.h"             //For init
#include "Forcefield.h"             //
#include "MoleculeLookup.h"
#include "MoleculeKind.h"
#include "Coordinates.h"
#include "BoxDimensions.h"
#include "cbmc/TrialMol.h"
#include "../lib/GeomLib.h"
#include "../lib/NumLib.h"
#include <cassert>

//
//    Ewald.h
//    Energy Calculation functions for Ewald summation method
//    Calculating self, correction and reciprocate part of ewald    
//
//    Developed by Y. Li and modified by Mohammad S. Barhaghi
// 
//

using namespace geom;

Ewald::Ewald(StaticVals const& stat, System & sys) :
   forcefield(stat.forcefield), mols(stat.mol), currentCoords(sys.coordinates),
   currentCOM(sys.com), ewald(false), sysPotRef(sys.potential),
#ifdef VARIABLE_PARTICLE_NUMBER
   molLookup(sys.molLookup),
#else
   molLookup(stat.molLookup),
#endif
#ifdef VARIABLE_VOLUME
   currentAxes(sys.boxDimensions)
#else
   currentAxes(stat.boxDimensions)
#endif
{ }



Ewald::~Ewald()
{
  for (int i = 0; i < mols.count; i++)
    {
      delete[] cosMolRef[i];
      delete[] sinMolRef[i];
      delete[] cosMolBoxRecip[i];
      delete[] sinMolBoxRecip[i];
    }
   for (uint b = 0; b < BOX_TOTAL; b++)
   {
      if (kx[b] != NULL)
      {
	 delete[] kx[b];
	 delete[] ky[b];
	 delete[] kz[b];
	 delete[] prefact[b];
	 delete[] sumRnew[b];
	 delete[] sumInew[b];
	 delete[] sumRref[b];
	 delete[] sumIref[b];
      }
   }
   if (kx != NULL)
   {
      delete[] kmax;
      delete[] kx;
      delete[] ky;
      delete[] kz;
      delete[] prefact;
      delete[] sumRnew;
      delete[] sumInew;
      delete[] sumRref;
      delete[] sumIref;
      delete[] cosMolRestore;
      delete[] cosMolRef;
      delete[] sinMolRestore;
      delete[] sinMolRef;
      delete[] cosMolBoxRecip;
      delete[] sinMolBoxRecip;
   }
  
}

void Ewald::Init()
{
   for(uint m = 0; m < mols.count; ++m)
   {
      const MoleculeKind& molKind = mols.GetKind(m);
      for(uint a = 0; a < molKind.NumAtoms(); ++a)
      {
         particleKind.push_back(molKind.AtomKind(a));
         particleMol.push_back(m);
	 particleCharge.push_back(molKind.AtomCharge(a));
      }
   }

   electrostatic = forcefield.electrostatic;
   ewald = forcefield.ewald; 
   if (ewald)
   {
     alpha = forcefield.alpha;
     recip_rcut = forcefield.recip_rcut;
     recip_rcut_Sq = recip_rcut * recip_rcut;
     InitEwald();
   }
      
}


void Ewald::InitEwald()
{   
   if (ewald)
   {
     //get size of image using defined Kmax
     //imageSize = imageTotal;   
     //allocate memory
     //allocate memory
     kmax = new int[BOX_TOTAL];
     imageSize = new int[BOX_TOTAL];
     sumRnew = new double*[BOX_TOTAL];
     sumInew = new double*[BOX_TOTAL];
     sumRref = new double*[BOX_TOTAL];
     sumIref = new double*[BOX_TOTAL];
     kx = new double*[BOX_TOTAL];
     ky = new double*[BOX_TOTAL];
     kz = new double*[BOX_TOTAL];
     prefact = new double*[BOX_TOTAL];
     cosMolRestore = new double[imageTotal];
     sinMolRestore = new double[imageTotal];
     cosMolRef = new double*[mols.count];
     sinMolRef = new double*[mols.count];
     cosMolBoxRecip = new double*[mols.count];
     sinMolBoxRecip = new double*[mols.count];
     
     for (uint b = 0; b < BOX_TOTAL; b++)
     {
        kx[b] = new double[imageTotal];
	ky[b] = new double[imageTotal];
	kz[b] = new double[imageTotal];
	prefact[b] = new double[imageTotal];
	sumRnew[b] = new double[imageTotal];
	sumInew[b] = new double[imageTotal];
	sumRref[b] = new double[imageTotal];
	sumIref[b] = new double[imageTotal];
	if (ewald)
	{
	  RecipCountInit(b, currentAxes);
	}
     }
     //10% more than original, space reserved for image size change
     int initImageSize = findLargeImage() * imageFlucRate;   
     
     for (int i = 0; i < mols.count; i++)
       {
	 cosMolRef[i] = new double[initImageSize];
	 sinMolRef[i] = new double[initImageSize];
	 cosMolBoxRecip[i] = new double[initImageSize];
	 sinMolBoxRecip[i] = new double[initImageSize];
       }
   }       
}

/*
uint Ewald::GetImageSize()
{
   uint counter = 0;
   double kmax = GetKmax();
   for (int x = 0.0; x <= kmax; x++)
   {
      int nky_max = sqrt(pow(kmax, 2) - pow(x, 2));
      int nky_min = -nky_max;
      if (x == 0.0)
      { 
	 nky_min = 0;
      }
      for (int y = nky_min; y <= nky_max; y++)
      {
	 int nkz_max = sqrt(pow(kmax, 2) - pow(x, 2) - pow(y, 2));
	 int nkz_min = -nkz_max;
	 if (x == 0.0 && y == 0.0)
         { 
	    nkz_min = 1;
	 }
	 for (int z = nkz_min; z <= nkz_max; z++)
         {
	    counter++;
	 }
      }
   }
  
  return counter;
  
}
*/

void Ewald::RecipInit(uint box, BoxDimensions const& boxAxes)
{
   uint counter = 0;
   double ksqr;
   double alpsqr4 = 1.0 / (4.0 * alpha * alpha);
   double constValue = 2 * M_PI / boxAxes.axis.BoxSize(box);
   double vol = boxAxes.volume[box] / (4 * M_PI);
   kmax[box] = int(recip_rcut * boxAxes.axis.BoxSize(box) / (2 * M_PI)) + 1;

   for (int x = 0; x <= kmax[box]; x++)
   {
      int nky_max = sqrt(pow(kmax[box], 2) - pow(x, 2));
      int nky_min = -nky_max;
      if (x == 0.0)
      { 
	 nky_min = 0;
      }
      for (int y = nky_min; y <= nky_max; y++)
      {
	 int nkz_max = sqrt(pow(kmax[box], 2) - pow(x, 2) - pow(y, 2));
	 int nkz_min = -nkz_max;
	 if (x == 0.0 && y == 0.0)
         { 
	    nkz_min = 1;
	 }
	 for (int z = nkz_min; z <= nkz_max; z++)
         {
	   ksqr = pow((constValue * x), 2) + pow((constValue * y), 2) +
	     pow ((constValue * z), 2);
	    
	    if (ksqr < recip_rcut_Sq)
	    {
	       kx[box][counter] = constValue * x;
	       ky[box][counter] = constValue * y;
	       kz[box][counter] = constValue * z;
	       prefact[box][counter] = num::qqFact * exp(-ksqr * alpsqr4)/
		 (ksqr * vol);
	       counter++;
	    }
	 }
      }
   }
   imageSize[box] = counter;
   printf("box: %d, counter: %d, kmax: %d\n", box, counter, kmax[box]);
   if (counter > imageTotal){
     printf("Warning! The Max number of images is fewer than the images demanded.\n");
     exit(EXIT_FAILURE);
   }
}
void Ewald::RecipCountInit(uint box, BoxDimensions const& boxAxes)
{
   uint counter = 0;
   double ksqr;
   double constValue = 2 * M_PI / boxAxes.axis.BoxSize(box);
   kmax[box] = int(recip_rcut * boxAxes.axis.BoxSize(box) / (2 * M_PI)) + 1;
   kmax[box] += 1;     //increase kmax by 1, reserve RAM for caching molsin/cos
   for (int x = 0; x <= kmax[box]; x++)
   {
      int nky_max = sqrt(pow(kmax[box], 2) - pow(x, 2));
      int nky_min = -nky_max;
      if (x == 0.0)
      { 
	 nky_min = 0;
      }
      for (int y = nky_min; y <= nky_max; y++)
      {
	 int nkz_max = sqrt(pow(kmax[box], 2) - pow(x, 2) - pow(y, 2));
	 int nkz_min = -nkz_max;
	 if (x == 0.0 && y == 0.0)
         { 
	    nkz_min = 1;
	 }
	 for (int z = nkz_min; z <= nkz_max; z++)
         {
	   ksqr = pow((constValue * x), 2) + pow((constValue * y), 2) +
	     pow ((constValue * z), 2);
	    
	    if (ksqr < recip_rcut_Sq)
	       counter++;
	 }
      }
   }
   imageSize[box] = counter;
   printf("box: %d, counter: %d, kmax: %d\n", box, counter, kmax[box]);
   if (counter > imageTotal){
     printf("Warning! The Max number of images is fewer than the images demanded.\n");
     exit(EXIT_FAILURE);
   }
}


//compare number of images in different boxes and select the largest one
int Ewald::findLargeImage()
{
  int imageLarge = 0;

  for (int b = 0; b < BOXES_WITH_U_NB; b++)
  {
    if (imageLarge < imageSize[b])
      imageLarge = imageSize[b];
  }
  return imageLarge;
}


//make a duplicate image of cosMolRef and sinMolRef for restore when running in GEMC
/*
void Ewald::initMolCache(double **arr1Ptr, double **arr2Ptr)
{
  int imageLarge = findLargeImage();
  //  arr1Ptr = new double *[mols.count];
  //  arr2Ptr = new double *[mols.count];
  //  printf("first dimension created\n");
  for (int i = 0; i < mols.count; i++)
  {
    arr1Ptr[i] = new double[imageLarge];
    arr2Ptr[i] = new double[imageLarge];
    printf("reinitial, i: %d, *arr1Ptr[i]: %p, *arr2Ptr[i]: %p\n", i, arr1Ptr[i], arr2Ptr[i]);
  }
}

void Ewald::initMolCache()
{
  int imageLarge = findLargeImage();
  //  arr1Ptr = new double *[mols.count];
  //  arr2Ptr = new double *[mols.count];
  //  printf("first dimension created\n");
  for (int i = 0; i < mols.count; i++)
  {
    cosMolRef[i] = new double[imageLarge];
    sinMolRef[i] = new double[imageLarge];
    printf("reinitial, i: %d, cosMolRef[i]: %p, sinMolRef[i]: %p\n", i, cosMolRef[i], sinMolRef[i]);
  }
}

//deconstruct the duplicate images cosMolBoxRecip and sinMolBoxRecip

void Ewald::deAllocateMolCache(double **arr1Ptr, double **arr2Ptr)
{
  for (int i = 0; i < mols.count; i++)
  {
    printf("deAllocate, i: %d, *arr1Ptr[i]: %p, *arr2Ptr[i]: %p\n", i, arr1Ptr[i], arr2Ptr[i]);
    delete[] arr1Ptr[i];
    delete[] arr2Ptr[i];
  }
  //  delete[] arr1Ptr;
  //  delete[] arr2Ptr;
  //  printf("first dimension deleted\n");
}
*/

//restore the whole cosMolRef & sinMolRef into cosMolBoxRecip & sinMolBoxRecip
void Ewald::exgMolCache()
{
  double **tempCos, **tempSin;
  tempCos = cosMolRef;
  tempSin = sinMolRef;
  cosMolRef = cosMolBoxRecip;
  sinMolRef = sinMolBoxRecip;
  cosMolBoxRecip = tempCos;
  sinMolBoxRecip = tempSin;
}

//calculate reciprocate term for a box
double Ewald::BoxReciprocal(int box, XYZArray const& molCoords)
{
   double energyRecip = 0.0; 
   double dotProduct = 0.0;
   double tempReal, tempImaginary, molReal, molImaginary;
   if (box < BOXES_WITH_U_NB)
   {
      MoleculeLookup::box_iterator end = molLookup.BoxEnd(box);
      for (int i = 0; i < imageSize[box]; i++)
      {
	 double sumReal = 0.0;
	 double sumImaginary = 0.0;
	 MoleculeLookup::box_iterator thisMol = molLookup.BoxBegin(box);

	 while (thisMol !=end)
	 {
	   molReal = 0; molImaginary = 0;
	   uint m1 = *thisMol;
	   MoleculeKind const& thisKind = mols.GetKind(m1);
	   for (uint j = 0; j < thisKind.NumAtoms(); j++)
	     {
	       dotProduct = currentAxes.DotProduct(mols.start[m1] + j,
						   kx[box][i], ky[box][i],
						   kz[box][i], molCoords, box);

	       tempReal = (thisKind.AtomCharge(j) * cos(dotProduct));
	       tempImaginary = (thisKind.AtomCharge(j) * sin(dotProduct));
	       molReal += tempReal;
	       sumReal += tempReal;
	       molImaginary += tempImaginary;
	       sumImaginary += tempImaginary;
	     }
	   cosMolRef[m1][i] = molReal;
	   sinMolRef[m1][i] = molImaginary;
	   thisMol++;
	 }
	 sumRnew[box][i] = sumReal;
	 sumInew[box][i] = sumImaginary;
	 energyRecip += ((sumReal * sumReal + sumImaginary * sumImaginary) *
			 prefact[box][i]);
      }
   }

   return energyRecip; 
}


//calculate reciprocate term for displacement and rotation move
double Ewald::MolReciprocal(XYZArray const& molCoords,
			    const uint molIndex,
			    const uint box,
			    XYZ const*const newCOM)
{
   double energyRecipNew = 0.0; 
   
   if (box < BOXES_WITH_U_NB)
   {
      MoleculeKind const& thisKind = mols.GetKind(molIndex);
      uint length = thisKind.NumAtoms();
      uint startAtom = mols.MolStart(molIndex);
      
      for (uint i = 0; i < imageSize[box]; i++)
      { 
	 double sumRealNew = 0.0;
	 double sumImaginaryNew = 0.0;
	 double sumRealOld = cosMolRef[molIndex][i];
	 double sumImaginaryOld = sinMolRef[molIndex][i];
	 cosMolRestore[i] = cosMolRef[molIndex][i];
	 sinMolRestore[i] = sinMolRef[molIndex][i];
	 double dotProductNew = 0.0;  
	 //	 double dotProductOld = 0.0;
	    
	 for (uint p = 0; p < length; ++p)
	 {
	    uint atom = startAtom + p;
	    dotProductNew = currentAxes.DotProduct(p, kx[box][i], ky[box][i],
						kz[box][i], molCoords, box);
	    //	    dotProductOld = currentAxes.DotProduct(atom, kx[box][i], ky[box][i],
	    //					kz[box][i], currentCoords, box);
	    
	    sumRealNew += (thisKind.AtomCharge(p) * cos(dotProductNew));
	    sumImaginaryNew += (thisKind.AtomCharge(p) * sin(dotProductNew));

	    //	    sumRealOld += (thisKind.AtomCharge(p) * cos(dotProductOld));
	    //	    sumImaginaryOld += (thisKind.AtomCharge(p) * sin(dotProductOld));
	 }
	 
	 sumRnew[box][i] = sumRref[box][i] - sumRealOld + sumRealNew;
	 sumInew[box][i] = sumIref[box][i] - sumImaginaryOld + sumImaginaryNew;
	 cosMolRef[molIndex][i] = sumRealNew;
	 sinMolRef[molIndex][i] = sumImaginaryNew;
	 
	 energyRecipNew += (sumRnew[box][i] * sumRnew[box][i] + sumInew[box][i]
			    * sumInew[box][i]) * prefact[box][i];	 
      }
      
   }

   return energyRecipNew - sysPotRef.boxEnergy[box].recip; 
}


//calculate self term for a box
double Ewald::BoxSelf(BoxDimensions const& boxAxes, uint box) const
{
   if (box >= BOXES_WITH_U_NB || !ewald)
     return 0.0;

   double self = 0.0;
   for (uint i = 0; i < mols.kindsCount; i++)
   {
     MoleculeKind const& thisKind = mols.kinds[i];
     uint length = thisKind.NumAtoms();
     double molSelfEnergy = 0.0;
     for (uint j = 0; j < length; j++)
     {
       molSelfEnergy += (thisKind.AtomCharge(j) * thisKind.AtomCharge(j));
     }
     self += (molSelfEnergy * molLookup.NumKindInBox(i, box));
   }
   
   self = -1.0 * self * alpha * num::qqFact / sqrt(M_PI);

   return self;
}


//calculate correction term for a molecule
double Ewald::MolCorrection(uint molIndex, BoxDimensions const& boxAxes,
			    uint box) const
{
   if (box >= BOXES_WITH_U_NB || !ewald)
     return 0.0;

   double dist, distSq;
   double correction = 0.0;
   XYZ virComponents; 
   
   MoleculeKind& thisKind = mols.kinds[mols.kIndex[molIndex]];
   for (uint i = 0; i < thisKind.NumAtoms(); i++)
   {
      for (uint j = i + 1; j < thisKind.NumAtoms(); j++)
      {
	 currentAxes.InRcut(distSq, virComponents, currentCoords,
			    mols.start[molIndex] + i,
			    mols.start[molIndex] + j, box);
	 dist = sqrt(distSq);
	 correction += (thisKind.AtomCharge(i) * thisKind.AtomCharge(j) *
			erf(alpha * dist) / dist);
      }
   }

   return correction;
}

//calculate reciprocate term in destination box for swap move
double Ewald::SwapDestRecip(const cbmc::TrialMol &newMol, const uint box, 
			    const int sourceBox, const int molIndex) 
{
   double energyRecipNew = 0.0; 
   double energyRecipOld = 0.0; 
   
   for (int i = 0; i < imageSize[sourceBox]; i++)
   {
     cosMolRestore[i] = cosMolRef[molIndex][i];
     sinMolRestore[i] = sinMolRef[molIndex][i];
   }

   if (box < BOXES_WITH_U_NB || !ewald)
   {
      MoleculeKind const& thisKind = newMol.GetKind();
      XYZArray molCoords = newMol.GetCoords();
      for (uint i = 0; i < imageSize[box]; i++)
      {
	//	 double sumRealNew = 0.0;
	//	 double sumImaginaryNew = 0.0;
	cosMolRef[molIndex][i] = 0.0;
	sinMolRef[molIndex][i] = 0.0;
	double dotProductNew = 0.0;  
	uint length = thisKind.NumAtoms();
	
	for (uint p = 0; p < length; ++p)
	  {
	    dotProductNew = currentAxes.DotProduct(p, kx[box][i], ky[box][i],
						   kz[box][i], molCoords, box);
	    cosMolRef[molIndex][i] += (thisKind.AtomCharge(p) * cos(dotProductNew));
	    sinMolRef[molIndex][i] += (thisKind.AtomCharge(p) * sin(dotProductNew));
	  }

	sumRnew[box][i] = sumRref[box][i] + cosMolRef[molIndex][i];   //sumRealNew;
	sumInew[box][i] = sumIref[box][i] + sinMolRef[molIndex][i];   //sumImaginaryNew;
	// cosMolRef[molIndex][i] = sumRealNew;
	//sinMolRef[molIndex][i] = sumImaginaryNew;
	
	energyRecipNew += (sumRnew[box][i] * sumRnew[box][i] + sumInew[box][i]
			   * sumInew[box][i]) * prefact[box][i];
      }
      energyRecipOld = sysPotRef.boxEnergy[box].recip;
   }
   return energyRecipNew - energyRecipOld;
}


//calculate reciprocate term in source box for swap move
double Ewald::SwapSourceRecip(const cbmc::TrialMol &oldMol, const uint box, const int molIndex) 
{
   double energyRecipNew = 0.0; 
   double energyRecipOld = 0.0;  
   
   if (box < BOXES_WITH_U_NB || !ewald)
   {
      MoleculeKind const& thisKind = oldMol.GetKind();
      XYZArray molCoords = oldMol.GetCoords();
      for (uint i = 0; i < imageSize[box]; i++)
      {
	 double sumRealNew = 0.0;
	 double sumImaginaryNew = 0.0;
	 double dotProductNew = 0.0;  
	 uint length = thisKind.NumAtoms();
	 /*
	 for (uint p = 0; p < length; ++p)
	 {
	    dotProductNew = currentAxes.DotProduct(p, kx[box][i], ky[box][i],
						kz[box][i], molCoords, box);
	    
	    sumRealNew += (thisKind.AtomCharge(p) * cos(dotProductNew));
	    sumImaginaryNew += (thisKind.AtomCharge(p) * sin(dotProductNew));

	 }
	 */
	 sumRnew[box][i] = sumRref[box][i] - cosMolRestore[i];
	 sumInew[box][i] = sumIref[box][i] - sinMolRestore[i];

	 energyRecipNew += (sumRnew[box][i] * sumRnew[box][i] + sumInew[box][i]
			    * sumInew[box][i]) * prefact[box][i];	 
      }
      energyRecipOld = sysPotRef.boxEnergy[box].recip;
   }

   return energyRecipNew - energyRecipOld;
}


//calculate self term for CBMC algorithm
void Ewald::SwapSelf(double *self, uint molIndex, uint partIndex, int box, 
	      uint trials) const
{
   if (box >= BOXES_WITH_U_NB || !ewald)
     return;

   MoleculeKind const& thisKind = mols.GetKind(molIndex);

   for (uint t = 0; t < trials; t++)
   {
     self[t] -= (thisKind.AtomCharge(partIndex) *
		 thisKind.AtomCharge(partIndex) * alpha *
		 num::qqFact / sqrt(M_PI)); 
   }

}

//calculate correction term for linear molecule CBMC algorithm
void Ewald::SwapCorrection(double* energy, const cbmc::TrialMol& trialMol, 
		    XYZArray const& trialPos, const uint partIndex, 
		    const uint box, const uint trials) const
{
   if (box >= BOXES_WITH_U_NB || !ewald)
     return;

   double dist;
   const MoleculeKind& thisKind = trialMol.GetKind();

   //loop over all partners of the trial particle
   const uint* partner = thisKind.sortedEwaldNB.Begin(partIndex);
   const uint* end = thisKind.sortedEwaldNB.End(partIndex);
   while (partner != end)
   {
      if (trialMol.AtomExists(*partner))
      {
	 for (uint t = 0; t < trials; ++t)
	 {
	   double distSq;
	   if (currentAxes.InRcut(distSq, trialPos, t, trialMol.GetCoords(),
				  *partner, box))
	   {
	     dist = sqrt(distSq);
	     energy[t] -= (thisKind.AtomCharge(*partner) *
			   thisKind.AtomCharge(partIndex) * erf(alpha * dist) *
			   num::qqFact / dist);
	   }
	 }
      }
      ++partner;
   }
}


//calculate correction term for branched molecule CBMC algorithm
void Ewald::SwapCorrection(double* energy, const cbmc::TrialMol& trialMol,
		    XYZArray *trialPos, const int pickedAtom, 
		    uint *partIndexArray, const uint box, const uint trials,
		    const uint prevIndex, bool prev) const
{
   if (box >= BOXES_WITH_U_NB || !ewald)
     return;

   double dist, distSq;
   const MoleculeKind& thisKind = trialMol.GetKind();
   uint pickedAtomIndex = partIndexArray[pickedAtom];

   if(prev)
      pickedAtomIndex = prevIndex;
	  
   for (int t = 0; t < trials; t++)
   {
      //loop through all previous new atoms generated simultanously,
      //and calculate the pair interactions between the picked atom and
      //the prev atoms.
      for (int newAtom = 0; newAtom < pickedAtom; newAtom++)
      {
	 distSq = 0;
	 if (currentAxes.InRcut(distSq, trialPos[newAtom], t,
				trialPos[pickedAtom], t, box))
	 {
	   dist = sqrt(distSq);
	   energy[t] -= (thisKind.AtomCharge(pickedAtomIndex) *
			 thisKind.AtomCharge(partIndexArray[newAtom]) *
			 erf(alpha * dist) * num::qqFact / dist);
	 }
      }

      //loop through the array of new molecule's atoms, and calculate the pair
      //interactions between the picked atom and the atoms have been created 
      //previously and added
      for (int count = 0; count < thisKind.NumAtoms(); count++)
      {
	 if (trialMol.AtomExists(count))
	 {
	    distSq = 0;
	    if (currentAxes.InRcut(distSq, trialMol.GetCoords(), count,
				   trialPos[pickedAtom], t, box))
	    {
	       dist = sqrt(distSq);
	       energy[t] -= (thisKind.AtomCharge(pickedAtomIndex) * 
			     thisKind.AtomCharge(count) *
			     erf(alpha * dist) * num::qqFact / dist);
	    }
	 }
      }
   }
}

double Ewald::CorrectionOldMol(const cbmc::TrialMol& oldMol,
			       const double distSq, const uint i,
			       const uint j) const
{
  if (oldMol.GetBox() >= BOXES_WITH_U_NB || !ewald)
     return 0.0;

   const MoleculeKind& thisKind = oldMol.GetKind();
   double dist = sqrt(distSq);
   return (-1 * thisKind.AtomCharge(i) * thisKind.AtomCharge(j) *
	   erf(alpha * dist) * num::qqFact / dist);
}
