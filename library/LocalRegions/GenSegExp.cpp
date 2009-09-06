///////////////////////////////////////////////////////////////////////////////
//
// File GenSegExp.cpp
//
// For more information, please see: http://www.nektar.info
//
// The MIT License
//
// Copyright (c) 2006 Division of Applied Mathematics, Brown University (USA),
// Department of Aeronautics, Imperial College London (UK), and Scientific
// Computing and Imaging Institute, University of Utah (USA).
//
// License for the specific language governing rights and limitations under
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// Description: GenSegExp routines
//
///////////////////////////////////////////////////////////////////////////////
#include <LocalRegions/GenSegExp.h>

namespace Nektar
{
    namespace LocalRegions 
    {

        GenSegExp::GenSegExp(const LibUtilities::BasisKey &Ba, 
                             const SpatialDomains::Geometry1DSharedPtr &geom):
            SegExp(Ba,geom)
        {
        }

        GenSegExp::GenSegExp(const GenSegExp &S):
            SegExp(S),
            m_physNormal(S.m_physNormal),
            m_physBiNormal(S.m_physBiNormal)
        {
        }
        // by default the StdExpansion1D destructor will be called        
        GenSegExp::~GenSegExp()
        {
        }
        
        void GenSegExp::SetUpPhysNormals(const StdRegions::StdExpansionSharedPtr &exp2D, const int edge)
        {            
            int k;
            int coordim = exp2D->GetCoordim();
            int nq      = m_base[0]->GetNumPoints();
            
            m_physNormal =  exp2D->GetMetricInfo()->GenNormals2D(exp2D->DetExpansionType(),edge,m_base[0]->GetPointsKey());
            
            if(exp2D->GetEorient(edge) == StdRegions::eBackwards)
            {
                if(exp2D->GetMetricInfo()->GetGtype() == SpatialDomains::eDeformed)
                {
                    for(k = 0; k < coordim; ++k)
                    {
                        Vmath::Reverse(nq,&m_physNormal[k*nq],1,
                                       &m_physNormal[k*nq],1);
                    }
                }

                Vmath::Neg(nq*coordim,m_physNormal,1);
            }
        }

        void GenSegExp::NormVectorIProductWRTBase(const Array<OneD, const NekDouble> &Fx, const Array<OneD, const NekDouble> &Fy, Array< OneD, NekDouble> &outarray, bool NegateNormal)
        {
            int nq = m_base[0]->GetNumPoints();
            Array<OneD, NekDouble > Fn(nq);

            Vmath::Vmul (nq,&Fx[0],1,&m_physNormal[0], 1,&Fn[0],1);
            Vmath::Vvtvp(nq,&Fy[0],1,&m_physNormal[nq],1,&Fn[0],1,&Fn[0],1);

            if(NegateNormal == true)
            {
                Vmath::Neg(nq,Fn,1);
            }
            
            IProductWRTBase(Fn,outarray);
        }

            
                                                  
    } // end of namespace    
}//end of namespace

// $Log: GenSegExp.cpp,v $
// Revision 1.3  2008/09/09 15:05:09  sherwin
// Updates related to cuved geometries. Normals have been removed from m_metricinfo and replaced with a direct evaluation call. Interp methods have been moved to LibUtilities
//
// Revision 1.2  2008/08/14 22:12:56  sherwin
// Introduced Expansion classes and used them to define HDG routines, has required quite a number of virtual functions to be added
//
// Revision 1.1  2008/07/29 22:24:49  sherwin
// Generalised Segment expansion which include a normal and binormal at the physical quadrature points
//
