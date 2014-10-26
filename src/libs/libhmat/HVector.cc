/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * HVector.cc  -- implementation of HVector class
 *
 * homer reid  -- 12/2009
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include <libhrutil.h>

extern "C" {
 #include "lapack.h" 
}

#include "libhmat.h"

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/
/* implementation of HVector class methods  --------------------*/
/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

/***************************************************************/
/* HVector constructors that create an empty (zero) HVector    */
/* with known size                                             */
/***************************************************************/
HVector::HVector(int pN, int pRealComplex, void *data)
{ InitHVector(pN, pRealComplex, data); }

/* copy constructor  */
HVector::HVector(HVector *V)
 { 
   InitHVector(V->N, V->RealComplex);
   if (RealComplex==LHM_REAL)
    memcpy(DV,V->DV,N*sizeof(double));
   else
    memcpy(ZV,V->ZV,N*sizeof(cdouble));
 }

void HVector::InitHVector(int pN, int pRealComplex, void *data)
{ 
   N=pN;
   RealComplex=pRealComplex;
   ErrMsg=0;

   ownsV = data == NULL;
   if (RealComplex==LHM_REAL)
   { DV=(double *)(data ? data : mallocEC(N*sizeof(double))); 
      ZV=0;
    }
   else
    { DV=0;
      ZV=(cdouble *)(data ? data : mallocEC(N*sizeof(cdouble)));
    };
 
}

/***************************************************************/
/* HVector constructors that attempt to read an HVector from   */
/* a file (either a binary HDF5 file or an ASCII text file.    */
/*                                                             */
/* FileType may be either LHM_HDF5 or LHM_TEXT.                */
/*                                                             */
/* if FileType==LHM_HDF5, then Options should be the name of   */
/* the vector within the HDF5 file.                            */
/*                                                             */
/* if FileType==LHM_TEXT, then Options should be a string of   */
/* zero or more of the following options:                      */
/*                                                             */
/*  --nrow xx : insist that there be a certain number of rows  */
/*                                                             */
/* if an error occurs, the ErrMsg field inside the class will  */
/* be nonzero on return.                                       */
/***************************************************************/
HVector::HVector(const char *FileName, int FileType, const char *Options)
 { ReadFromFile(FileName, FileType, Options); }

void HVector::ReadFromFile(const char *FileName, int FileType, const char *Options)
{
  DV=0;
  ZV=0;
  ErrMsg=0;

  if (FileType == LHM_AUTO)
    FileType = LHM_AUTO_FileType(FileName);

  switch(FileType)
   { 
     case LHM_TEXT: 
       ImportFromText(FileName,Options); 
       break;

     case LHM_HDF5: 
       ImportFromHDF5(FileName,Options);
       break;

     default:
       ErrExit("%s:%i: internal error",__FILE__,__LINE__);
       break;
   };
}

HVector::~HVector()
{ if (ownsV) {
    if (DV) free(DV);
    if (ZV) free(ZV);
  }
}

void HVector::SetEntry(int n, cdouble Entry)
{
  if (RealComplex==LHM_REAL)
   DV[n] = real(Entry);
  else 
   ZV[n] = Entry;
}

void HVector::SetEntry(int n, double Entry)
{
  if (RealComplex==LHM_REAL)
   DV[n] = Entry;
  else 
   ZV[n] = Entry;
}

void HVector::AddEntry(int n, cdouble Entry)
{
  if (RealComplex==LHM_REAL)
   DV[n] += real(Entry);
  else 
   ZV[n] += Entry;
}

void HVector::AddEntry(int n, double Entry)
{
  if (RealComplex==LHM_REAL)
   DV[n] += Entry;
  else 
   ZV[n] += cdouble(Entry,0.0);
}

cdouble HVector::GetEntry(int n)
{
  if (RealComplex==LHM_REAL)
   return DV[n];
  else 
   return ZV[n];
}

double HVector::GetEntryD(int n)
{
  if (RealComplex==LHM_REAL)
   return DV[n];
  else 
   return real(ZV[n]);
}

/***************************************************************/
/* scale the entire vector by a scalar multiple ****************/
/***************************************************************/
void HVector::Scale(cdouble Alpha)
{
  int n;

  if (RealComplex==LHM_REAL)
   for(n=0; n<N; n++)
    DV[n]*=real(Alpha);
  else
   for(n=0; n<N; n++)
    ZV[n]*=Alpha;

}

void HVector::Scale(double Alpha)
{ Scale(cdouble(Alpha)); }

/***************************************************************/
/* zero out the vector *****************************************/
/***************************************************************/
void HVector::Zero()
{ 
  if (RealComplex==LHM_REAL)
   memset(DV,0,N*sizeof(double));
  else
   memset(ZV,0,N*sizeof(cdouble));
}

/***************************************************************/
/* copy data from another vector *******************************/
/***************************************************************/
void HVector::Copy(HVector *V)
{
  if ( V->N != N|| V->RealComplex!=RealComplex )
   { fprintf(stderr,"\n*\n* WARNING: %s:%i: vector properties mismatch (copy failed)\n*\n",__FILE__, __LINE__);
     return;
   };

  if (RealComplex==LHM_REAL)
   memcpy(DV, V->DV, N*sizeof(double));
  else 
   memcpy(ZV, V->ZV, N*sizeof(cdouble));

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
HVector *LinSpace(double Min, double Max, int Num)
{ 
  if (Num<1) ErrExit("LinSpace: invalid N value (%i)\n",Num);
  HVector *V=new HVector(Num);
  V->SetEntry(0, Min);
  double Delta=(Max-Min) / ( ((double)Num) - 1.0 );
  for(int n=1; n<Num; n++)
   V->SetEntry(n, V->GetEntryD(n-1) + Delta);
  return V;
  
} 

HVector *LogSpace(double Min, double Max, int Num)
{ 
  if (Num<1) ErrExit("LogSpace: invalid N value (%i)\n",Num);
  HVector *V=new HVector(Num);
  V->SetEntry(0, Min);
  double Mult=exp( log(Max/Min)/((double)(Num-1)) );
  for(int n=1; n<Num; n++)
   V->SetEntry(n, V->GetEntry(n-1) * Mult );
  return V;
  
}

/***************************************************************/
/* concatenate two vectors, [V1; V2] in matlab notation.       */
/* returns NULL if both vectors are NULL.                      */
/***************************************************************/
HVector *Concat(HVector *V1, HVector *V2)
{  
  if ( V1==0 && V2==0 )
   return 0;
  
  if ( V1==0 && V2!=0 )
   return new HVector(V2); // invoke copy constructor

  if ( V1!=0 && V2==0 )
   return new HVector(V1); // invoke copy constructor

  int RealComplex = (V1->RealComplex==LHM_REAL && V2->RealComplex==LHM_REAL ) ? LHM_REAL : LHM_COMPLEX;
  
  HVector *V = new HVector(V1->N + V2->N, RealComplex);
  int n, n1, n2;
  for(n=n1=0; n1<V1->N; n1++)
   V->SetEntry( n++, V1->GetEntry(n1) );
  for(n2=0; n2<V2->N; n2++)
   V->SetEntry( n++, V2->GetEntry(n2) );
  return V;

}

/***************************************************************/
/* min, max, average             *******************************/
/***************************************************************/
#define MMM_MIN  0
#define MMM_MAX  0
#define MMM_MEAN 0
#if 0
cdouble HVector::MinMaxMean(int Which)
{ 
  int n;

  if (RealComplex==LHM_REAL)
   { 
     double DMin=DM[0], DMax=DM[0], DMean=DM[0], DStdDev=DM[0]*DM[0];
     for(n=1; n<N; n++)
      { DMin=fmin(DMin, DM[n]);
        DMax=fmax(DMax, DM[n]);
        DMean += DM[n];
        DStdDev += DM[n]*DM[n];
      };
     DMean/=((double)N);
     DStdDev=sqrt(DStdDev-
   };
  
}

cdouble HVector::Min()
{}
#endif
