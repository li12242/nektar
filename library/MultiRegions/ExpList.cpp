///////////////////////////////////////////////////////////////////////////////
//
// File ExpList.cpp
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
// Description: Expansion list definition
//
///////////////////////////////////////////////////////////////////////////////

#include <MultiRegions/ExpList.h>
#include <MultiRegions/LocalToGlobalC0ContMap.h>
#include <MultiRegions/GlobalLinSys.h>

namespace Nektar
{
    namespace MultiRegions
    {
        /**
         * @class ExpList
         * All multi-elemental expansions \f$u^{\delta}(\boldsymbol{x})\f$ can
         * be considered as the assembly of the various elemental contributions.
         * On a discrete level, this yields,
         * \f[u^{\delta}(\boldsymbol{x}_i)=\sum_{e=1}^{{N_{\mathrm{el}}}}
         * \sum_{n=0}^{N^{e}_m-1}\hat{u}_n^e\phi_n^e(\boldsymbol{x}_i).\f]
         * where \f${N_{\mathrm{el}}}\f$ is the number of elements and
         * \f$N^{e}_m\f$ is the local elemental number of expansion modes.
         * As it is the lowest level class, it contains the definition of the
         * common data and common routines to all multi-elemental expansions.
         *
         * The class stores a vector of expansions, \a m_exp, (each derived from
         * StdRegions#StdExpansion) which define the constituent components of
         * the domain. The coefficients from these expansions are concatenated
         * in \a m_coeffs, while the expansion evaluated at the quadrature
         * points is stored in \a m_phys.
         */

        /**
         * Creates an empty expansion list. The expansion list will typically be
         * populated by a derived class (namely one of MultiRegions#ExpList1D,
         * MultiRegions#ExpList2D or MultiRegions#ExpList3D).
         */
        ExpList::ExpList():
            m_ncoeffs(0),
            m_npoints(0),
            m_coeffs(),
            m_phys(),
            m_transState(eNotSet),
            m_physState(false),
            m_exp(MemoryManager<StdRegions::StdExpansionVector>
                                                        ::AllocateSharedPtr()),
            m_globalOptParam(MemoryManager<NekOptimize::GlobalOptParam>
                                                        ::AllocateSharedPtr()),
            m_blockMat(MemoryManager<BlockMatrixMap>::AllocateSharedPtr())
        {
        }


        /**
         * Copies an existing expansion list.
         * @param   in              Source expansion list.
         */
        ExpList::ExpList(const ExpList &in):
            m_ncoeffs(in.m_ncoeffs),
            m_npoints(in.m_npoints),
            m_coeffs(m_ncoeffs),
            m_phys(m_npoints),
            m_transState(eNotSet),
            m_physState(false),
            m_exp(in.m_exp),
            m_coeff_offset(in.m_coeff_offset), // Need to check if we need these
            m_phys_offset(in.m_phys_offset),   // or at least use shared pointer
            m_globalOptParam(in.m_globalOptParam),
            m_blockMat(in.m_blockMat)
        {
        }


        /**
         * Set up the storage for the concatenated list of coefficients and
         * physical evaluations at the quadrature points. Each expansion (local
         * element) is processed in turn to determine the number of coefficients
         * and physical data points it contributes to the domain. A second pair
         * of arrays, #m_coeff_offset and #m_phys_offset, are also initialised
         * and updated to store the data offsets of each element in the
         * #m_coeffs and #m_phys arrays, respectively.
         */
        void ExpList::SetCoeffPhys()
        {
            int i;

            // Set up offset information and array sizes
            m_coeff_offset = Array<OneD,int>(m_exp->size());
            m_phys_offset  = Array<OneD,int>(m_exp->size());

            m_ncoeffs = m_npoints = 0;

            for(i = 0; i < m_exp->size(); ++i)
            {
                m_coeff_offset[i] = m_ncoeffs;
                m_phys_offset [i] = m_npoints;
                m_ncoeffs += (*m_exp)[i]->GetNcoeffs();
                m_npoints += (*m_exp)[i]->GetNumPoints(0);
            }

            m_coeffs = Array<OneD, NekDouble>(m_ncoeffs);
            m_phys   = Array<OneD, NekDouble>(m_npoints);
        }


        /**
         * For each element, copy the coefficients from \a m_coeffs into their
         * respective element expansion from \a m_exp.
         */
        void ExpList::PutCoeffsInToElmtExp()
        {
            int i, order_e;
            int cnt = 0;

            for(i = 0; i < (*m_exp).size(); ++i)
            {
                order_e = (*m_exp)[i]->GetNcoeffs();
                Vmath::Vcopy(order_e,&m_coeffs[cnt], 1,
                                         &((*m_exp)[i]->UpdateCoeffs())[0],1);
                cnt += order_e;
            }
        }


        /**
         * Copy the coefficients associated with element \a eid from \a m_coeffs
         * to the corresponding element expansion object from \a m_exp.
         * @param   eid         Index of element for which copy is performed.
         */
        void ExpList::PutCoeffsInToElmtExp(int eid)
        {
            int order_e;
            int cnt = 0;

            order_e = (*m_exp)[eid]->GetNcoeffs();
            cnt = m_coeff_offset[eid];
            Vmath::Vcopy(order_e,&m_coeffs[cnt], 1,
                         &((*m_exp)[eid]->UpdateCoeffs())[0],1);
        }


        /**
         * Coefficients from each local expansion are copied into the
         * concatenated list of coefficients for all elements.
         */
        void ExpList::PutElmtExpInToCoeffs(void)
        {
            int i, order_e;
            int cnt = 0;

            for(i = 0; i < (*m_exp).size(); ++i)
            {
                order_e = (*m_exp)[i]->GetNcoeffs();
                Vmath::Vcopy(order_e, &((*m_exp)[i]->UpdateCoeffs())[0],1,
                             &m_coeffs[cnt],1);
                cnt += order_e;
            }
        }


        /**
         * Coefficients for a single element are copied from the associated
         * element expansion to the concatenated coefficient array.
         * @param   eid         Index of element to copy.
         */
        void ExpList::PutElmtExpInToCoeffs(int eid)
        {
            int order_e;
            int cnt = 0;

            order_e = (*m_exp)[eid]->GetNcoeffs();
            cnt = m_coeff_offset[eid];

            Vmath::Vcopy(order_e, &((*m_exp)[eid]->UpdateCoeffs())[0],1,
                             &m_coeffs[cnt],1);
        }


        /**
         * The local expansion objects are populated with the physical
         * evaluation at the quadrature points stored in the \a m_phys storage.
         */
        void ExpList::PutPhysInToElmtExp()
        {
            PutPhysInToElmtExp(m_phys);
        }


        /**
         * The local expansion objects are populated with the supplied physical
         * evaluations at the quadrature points. The layout and order of the
         * supplied data is assumed to conform to the expansion list.
         * @param   in          Physical quadrature data.
         */
        void ExpList::PutPhysInToElmtExp(Array<OneD,const NekDouble> &in)
        {
            int i, npoints_e;
            int cnt = 0;

            for(i = 0; i < (*m_exp).size(); ++i)
            {
                npoints_e = (*m_exp)[i]->GetTotPoints();
                Vmath::Vcopy(npoints_e, &in[cnt],1, 
                                        &((*m_exp)[i]->UpdatePhys())[0],1);
                cnt += npoints_e;
            }
        }


        /**
         * The physical evaluations at the quadrature points from the expansion
         * objects are concatenated and stored in \a out.
         * @param   out         Storage for physical values.
         */
        void ExpList::PutElmtExpInToPhys(Array<OneD,NekDouble> &out)
        {
            int i, npoints_e;
            int cnt = 0;

            for(i = 0; i < (*m_exp).size(); ++i)
            {
                npoints_e = (*m_exp)[i]->GetTotPoints();
                Vmath::Vcopy(npoints_e, &((*m_exp)[i]->GetPhys())[0],1,
                             &out[cnt],1);
                cnt += npoints_e;
            }
        }


        /**
         * The physical evaluations at the quadrature points in the element
         * expansion \a eid are copied to \a out.
         * @param   out         Storage for physical values.
         */
        void ExpList::PutElmtExpInToPhys(int eid, Array<OneD,NekDouble> &out)
        {
            int npoints_e;
            int cnt = m_phys_offset[eid];

            npoints_e = (*m_exp)[eid]->GetTotPoints();
            Vmath::Vcopy(npoints_e, &((*m_exp)[eid]->GetPhys())[0],1,
                         &out[cnt],1);
        }


        ExpList::~ExpList()
        {
        }


        /**
         * The integration is evaluated locally, that is
         * \f[\int
         *    f(\boldsymbol{x})d\boldsymbol{x}=\sum_{e=1}^{{N_{\mathrm{el}}}}
         * \left\{\int_{\Omega_e}f(\boldsymbol{x})d\boldsymbol{x}\right\},  \f]
         * where the integration over the separate elements is done by the
         * function StdRegions#StdExpansion#Integral, which discretely
         * evaluates the integral using Gaussian quadrature.
         *
         * Note that the array #m_phys should be filled with the values of the
         * function \f$f(\boldsymbol{x})\f$ at the quadrature points
         * \f$\boldsymbol{x}_i\f$.
         *
         * @return  The value of the discretely evaluated integral
         *          \f$\int f(\boldsymbol{x})d\boldsymbol{x}\f$.
         */
        NekDouble ExpList::PhysIntegral()
        {
            ASSERTL1(m_physState == true,
                     "local physical space is not true ");

            return PhysIntegral(m_phys);
        }


        /**
         * The integration is evaluated locally, that is
         * \f[\int
         *    f(\boldsymbol{x})d\boldsymbol{x}=\sum_{e=1}^{{N_{\mathrm{el}}}}
         * \left\{\int_{\Omega_e}f(\boldsymbol{x})d\boldsymbol{x}\right\},  \f]
         * where the integration over the separate elements is done by the
         * function StdRegions#StdExpansion#Integral, which discretely
         * evaluates the integral using Gaussian quadrature.
         *
         * @param   inarray         An array of size \f$Q_{\mathrm{tot}}\f$
         *                          containing the values of the function
         *                          \f$f(\boldsymbol{x})\f$ at the quadrature
         *                          points \f$\boldsymbol{x}_i\f$.
         * @return  The value of the discretely evaluated integral
         *          \f$\int f(\boldsymbol{x})d\boldsymbol{x}\f$.
         */
        NekDouble ExpList::PhysIntegral(
                                const Array<OneD, const NekDouble> &inarray)
        {
            int       i;
            int       cnt = 0;
            NekDouble sum = 0.0;

            for(i = 0; i < GetExpSize(); ++i)
            {
                sum += (*m_exp)[i]->Integral(inarray + cnt);
                cnt += (*m_exp)[i]->GetTotPoints();
            }

            return sum;
        }


        /**
         * Retrieves the block matrix specified by \a bkey, and computes
         * \f$ y=Mx \f$.
         * @param   gkey        GlobalMatrixKey specifying the block matrix to
         *                      use in the matrix-vector multiply.
         * @param   inarray     Input vector \f$ x \f$.
         * @param   outarray    Output vector \f$ y \f$.
         */
        void ExpList::MultiplyByBlockMatrix(
                                const GlobalMatrixKey             &gkey,
                                const Array<OneD,const NekDouble> &inarray,
                                      Array<OneD,      NekDouble> &outarray)
        {
            // Retrieve the block matrix using the given key.
            const DNekScalBlkMatSharedPtr& blockmat = GetBlockMatrix(gkey);
            int nrows = blockmat->GetRows();
            int ncols = blockmat->GetColumns();

            // Create NekVectors from the given data arrays
            NekVector<const NekDouble> in (ncols,inarray, eWrapper);
            NekVector<      NekDouble> out(nrows,outarray,eWrapper);

            // Perform matrix-vector multiply.
            out = (*blockmat)*in;
        }


        /**
         * The operation is evaluated locally for every element by the function
         * StdRegions#StdExpansion#IProductWRTBase.
         *
         * @param   inarray         An array of size \f$Q_{\mathrm{tot}}\f$
         *                          containing the values of the function
         *                          \f$f(\boldsymbol{x})\f$ at the quadrature
         *                          points \f$\boldsymbol{x}_i\f$.
         * @param   outarray        An array of size \f$N_{\mathrm{eof}}\f$
         *                          used to store the result.
         */
        void ExpList::IProductWRTBase_IterPerExp(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD,       NekDouble> &outarray)
        {
            bool doBlockMatOp
                = m_globalOptParam->DoBlockMatOp(StdRegions::eIProductWRTBase);

            if(doBlockMatOp)
            {
                GlobalMatrixKey mkey(StdRegions::eIProductWRTBase);
                MultiplyByBlockMatrix(mkey,inarray,outarray);
            }
            else
            {
                int    i;
                int    cnt  = 0;
                int    cnt1 = 0;

                Array<OneD,NekDouble> e_outarray;

                for(i = 0; i < GetExpSize(); ++i)
                {
                    (*m_exp)[i]->IProductWRTBase(inarray+cnt,
                                                 e_outarray = outarray+cnt1);
                    cnt  += (*m_exp)[i]->GetTotPoints();
                    cnt1 += (*m_exp)[i]->GetNcoeffs();
                }
                m_transState = eLocal;
            }
        }


        /**
         * The operation is evaluated locally for every element by the function
         * StdRegions#StdExpansion#IProductWRTDerivBase.
         *
         * @param   dir             {0,1} is the direction in which the
         *                          derivative of the basis should be taken
         * @param   inarray         An array of size \f$Q_{\mathrm{tot}}\f$
         *                          containing the values of the function
         *                          \f$f(\boldsymbol{x})\f$ at the quadrature
         *                          points \f$\boldsymbol{x}_i\f$.
         * @param   outarray        An array of size \f$N_{\mathrm{eof}}\f$
         *                          used to store the result.
         */
        void ExpList::IProductWRTDerivBase(const int dir,
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD, NekDouble> &outarray)
        {
            int    i;
            int    cnt  = 0;
            int    cnt1 = 0;

            Array<OneD,NekDouble> e_outarray;

            for(i = 0; i < GetExpSize(); ++i)
            {
                (*m_exp)[i]->IProductWRTDerivBase(dir,inarray+cnt,
                                             e_outarray = outarray+cnt1);
                cnt  += (*m_exp)[i]->GetTotPoints();
                cnt1 += (*m_exp)[i]->GetNcoeffs();
            }
            m_transState = eLocal;
        }

        /**
         * Given a function \f$f(\boldsymbol{x})\f$ evaluated at
         * the quadrature points, this function calculates the
         * derivatives \f$\frac{d}{dx_1}\f$, \f$\frac{d}{dx_2}\f$
         * and \f$\frac{d}{dx_3}\f$ of the function
         * \f$f(\boldsymbol{x})\f$ at the same quadrature
         * points. The local distribution of the quadrature points
         * allows an elemental evaluation of the derivative. This
         * is done by a call to the function
         * StdRegions#StdExpansion#PhysDeriv.
         *
         * @param   inarray         An array of size \f$Q_{\mathrm{tot}}\f$
         *                          containing the values of the function
         *                          \f$f(\boldsymbol{x})\f$ at the quadrature
         *                          points \f$\boldsymbol{x}_i\f$.
         * @param   out_d0          The discrete evaluation of the
         *                          derivative\f$\frac{d}{dx_1}\f$ will
         *                          be stored in this array of size
         *                          \f$Q_{\mathrm{tot}}\f$.
         * @param   out_d1          The discrete evaluation of the
         *                          derivative\f$\frac{d}{dx_2}\f$ will be
         *                          stored in this array of size
         *                          \f$Q_{\mathrm{tot}}\f$. Note that if no
         *                          memory is allocated for \a out_d1, the
         *                          derivative \f$\frac{d}{dx_2}\f$ will not be
         *                          calculated.
         * @param   out_d2          The discrete evaluation of the
         *                          derivative\f$\frac{d}{dx_3}\f$ will be
         *                          stored in this array of size
         *                          \f$Q_{\mathrm{tot}}\f$. Note that if no
         *                          memory is allocated for \a out_d2, the
         *                          derivative \f$\frac{d}{dx_3}\f$ will not be
         *                          calculated.
         */
        void ExpList::PhysDeriv(const Array<OneD, const NekDouble> &inarray,
                                Array<OneD, NekDouble> &out_d0,
                                Array<OneD, NekDouble> &out_d1,
                                Array<OneD, NekDouble> &out_d2)
        {
            int  cnt = 0;
            int  i;
            Array<OneD, NekDouble> e_out_d0;
            Array<OneD, NekDouble> e_out_d1;
            Array<OneD, NekDouble> e_out_d2;

            for(i= 0; i < GetExpSize(); ++i)
            {
                e_out_d0 = out_d0 + cnt;
                if(out_d1.num_elements())
                {
                    e_out_d1 = out_d1 + cnt;
                }

                if(out_d2.num_elements())
                {
                    e_out_d2 = out_d2 + cnt;
                }

                (*m_exp)[i]->PhysDeriv(inarray+cnt,e_out_d0,e_out_d1,e_out_d2);
                cnt  += (*m_exp)[i]->GetTotPoints();
            }
        }

        void ExpList::PhysDeriv(const int dir,
                                const Array<OneD, const NekDouble> &inarray,
                                Array<OneD, NekDouble> &out_d)
        {
            int  cnt = 0;
            int  i;
            Array<OneD, NekDouble> e_out_d;

            for(i= 0; i < GetExpSize(); ++i)
            {
                e_out_d = out_d + cnt;
                (*m_exp)[i]->PhysDeriv(dir, inarray+cnt, e_out_d);
                cnt  += (*m_exp)[i]->GetTotPoints();
            }
        }


        /**
         * The coefficients of the function to be acted upon
         * should be contained in the \param inarray. The
         * resulting coefficients are stored in \param outarray
         *
         * @param   inarray         An array of size \f$N_{\mathrm{eof}}\f$
         *                          containing the inner product.
         */
        void ExpList::MultiplyByElmtInvMass(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD, NekDouble> &outarray)
        {
            GlobalMatrixKey mkey(StdRegions::eInvMass);
            const DNekScalBlkMatSharedPtr& InvMass = GetBlockMatrix(mkey);

            // Inverse mass matrix
            NekVector<NekDouble> out(m_ncoeffs,outarray,eWrapper);
            if(inarray.get() == outarray.get())
            {
                NekVector<const NekDouble> in(m_ncoeffs,inarray); // copy data
                out = (*InvMass)*in;
            }
            else
            {
                NekVector<const NekDouble> in(m_ncoeffs,inarray,eWrapper);
                out = (*InvMass)*in;
            }
        }

        /**
         * Given a function \f$u(\boldsymbol{x})\f$ defined at the
         * quadrature points, this function determines the
         * transformed elemental coefficients \f$\hat{u}_n^e\f$
         * employing a discrete elemental Galerkin projection from
         * physical space to coefficient space. For each element,
         * the operation is evaluated locally by the function
         * StdRegions#StdExpansion#IproductWRTBase followed by a
         * call to #MultiRegions#MultiplyByElmtInvMass.
         *
         * @param   inarray         An array of size \f$Q_{\mathrm{tot}}\f$
         *                          containing the values of the function
         *                          \f$f(\boldsymbol{x})\f$ at the quadrature
         *                          points \f$\boldsymbol{x}_i\f$.
         * @param   outarray        The resulting coefficients
         *                          \f$\hat{u}_n^e\f$ will be stored in this
         *                          array of size \f$N_{\mathrm{eof}}\f$.
         */
        void ExpList::FwdTrans_IterPerExp(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD, NekDouble> &outarray)
        {
            Array<OneD,NekDouble> f(m_ncoeffs);

            IProductWRTBase_IterPerExp(inarray,f);
            MultiplyByElmtInvMass(f,outarray);

        }

        void ExpList::FwdTrans_BndConstrained(
                                const Array<OneD, const NekDouble>& inarray,
                                      Array<OneD, NekDouble> &outarray)
        {
            int cnt  = 0;
            int cnt1 = 0;
            int i;

            Array<OneD,NekDouble> e_outarray;

            for(i= 0; i < GetExpSize(); ++i)
            {
                (*m_exp)[i]->FwdTrans_BndConstrained(inarray+cnt,
                                      e_outarray = outarray+cnt1);
                cnt  += (*m_exp)[i]->GetTotPoints();
                cnt1 += (*m_exp)[i]->GetNcoeffs();
            }
        }

        /**
         * This function assembles the block diagonal matrix
         * \f$\underline{\boldsymbol{M}}^e\f$, which is the
         * concatenation of the local matrices
         * \f$\boldsymbol{M}^e\f$ of the type \a mtype, that is
         *
         * \f[
         * \underline{\boldsymbol{M}}^e = \left[
         * \begin{array}{cccc}
         * \boldsymbol{M}^1 & 0 & \hspace{3mm}0 \hspace{3mm}& 0 \\
         *  0 & \boldsymbol{M}^2 & 0 & 0 \\
         *  0 &  0 & \ddots &  0 \\
         *  0 &  0 & 0 & \boldsymbol{M}^{N_{\mathrm{el}}} \end{array}\right].\f]
         *
         * @param   mtype           the type of matrix to be assembled
         * @param   scalar          an optional parameter
         * @param   constant        an optional parameter
         */
        const DNekScalBlkMatSharedPtr ExpList::GenBlockMatrix(
                                const GlobalMatrixKey &gkey)
        {
            int i,j,cnt1,matrixid;
            int n_exp = GetExpSize();
            Array<OneD,unsigned int> nrows(n_exp);
            Array<OneD,unsigned int> ncols(n_exp);
            DNekScalMatSharedPtr    loc_mat;
            DNekScalBlkMatSharedPtr BlkMatrix;

            switch(gkey.GetMatrixType())
            {
            case StdRegions::eBwdTrans:
                {
                    // set up an array of integers for block matrix construction
                    for(i = 0; i < n_exp; ++i)
                    {
                        nrows[i] = (*m_exp)[i]->GetTotPoints();
                        ncols[i] = (*m_exp)[i]->GetNcoeffs();
                    }
                }
                break;
            case StdRegions::eIProductWRTBase:
                {
                    // set up an array of integers for block matrix construction
                    for(i = 0; i < n_exp; ++i)
                    {
                        nrows[i] = (*m_exp)[i]->GetNcoeffs();
                        ncols[i] = (*m_exp)[i]->GetTotPoints();
                    }
                }
                break;
            case StdRegions::eMass:
            case StdRegions::eInvMass:
            case StdRegions::eHelmholtz:
            case StdRegions::eLaplacian:
            case StdRegions::eInvHybridDGHelmholtz:
                {
                    // set up an array of integers for block matrix construction
                    for(i = 0; i < n_exp; ++i)
                    {
                        nrows[i] = (*m_exp)[i]->GetNcoeffs();
                        ncols[i] = (*m_exp)[i]->GetNcoeffs();
                    }
                }
                break;
            case StdRegions::eHybridDGLamToU:
                {
                    // set up an array of integers for block matrix construction
                    for(i = 0; i < n_exp; ++i)
                    {
                        nrows[i] = (*m_exp)[i]->GetNcoeffs();
                        ncols[i] = (*m_exp)[i]->NumDGBndryCoeffs();
                    }
                }
                break;

            default:
                {
                    NEKERROR(ErrorUtil::efatal, 
                             "Global Matrix creation not defined for this type "
                             "of matrix");
                }
            }

            MatrixStorage blkmatStorage = eDIAGONAL;
            BlkMatrix = MemoryManager<DNekScalBlkMat>
                                ::AllocateSharedPtr(nrows,ncols,blkmatStorage);

            int totnq, nvarcoeffs = gkey.GetNvariableCoefficients();
            Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);

            for(i = cnt1 = matrixid = 0; i < n_exp; ++i)
            {
                totnq = GetCoordim(i)*( (*m_exp)[i]->GetTotPoints() );
                
                if(nvarcoeffs>0)
                {
                    // When two varcoeffs in a specific order               
                    for(j = 0; j < nvarcoeffs; j++)
                    {
                        varcoeffs[j] = Array<OneD, NekDouble>(totnq,0.0);
                        Vmath::Vcopy(totnq, &(gkey.GetVariableCoefficient(j))[cnt1], 1, &varcoeffs[j][0],1);
                    }
                    
                    cnt1  += totnq;
                    matrixid++;
                }

                int Nconstants = gkey.GetNconstants();
                Array<OneD, NekDouble> Constants(Nconstants);
                if(Nconstants>2)
                {
                    Constants[0] = gkey.GetConstant(i);
                    Constants[1] = gkey.GetConstant(Nconstants-1);
                }

                else if(Nconstants==2)
                {
                    Constants[0] = gkey.GetConstant(0);
                    Constants[1] = gkey.GetConstant(1);
                }


                LocalRegions::MatrixKey matkey(gkey.GetMatrixType(),
                                               (*m_exp)[i]->DetExpansionType(),
                                               *(*m_exp)[i],
                                               Constants,
                                               varcoeffs,
                                               matrixid);
                
                loc_mat = (*m_exp)[i]->GetLocMatrix(matkey);
                BlkMatrix->SetBlock(i,i,loc_mat);
            }

            return BlkMatrix;
        }

        const DNekScalBlkMatSharedPtr& ExpList::GetBlockMatrix(
                                const GlobalMatrixKey &gkey)
        {
            BlockMatrixMap::iterator matrixIter = m_blockMat->find(gkey);

            if(matrixIter == m_blockMat->end())
            {
                return ((*m_blockMat)[gkey] = GenBlockMatrix(gkey));
            }
            else
            {
                return matrixIter->second;
            }
        }

        void ExpList::GeneralMatrixOp_IterPerExp(
                                const GlobalMatrixKey             &gkey,
                                const Array<OneD,const NekDouble> &inarray,
                                      Array<OneD,      NekDouble> &outarray)
        {
            bool doBlockMatOp = m_globalOptParam->DoBlockMatOp(
                                                        gkey.GetMatrixType());

            if(doBlockMatOp)
            {
                MultiplyByBlockMatrix(gkey,inarray,outarray);
            }
            else
            {
                int  i,j;
                int  cnt  = 0;
                int  cnt1 = 0;
                Array<OneD,NekDouble>      e_outarray;

                int nvarcoeffs = gkey.GetNvariableCoefficients();
                Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);

                for(i= 0; i < GetExpSize(); ++i)
                {
                    if(nvarcoeffs>0)
                    {
                        for(j = 0; j < nvarcoeffs; j++)
                        {
                            varcoeffs[j] = gkey.GetVariableCoefficient(j) + cnt1;
                        }
                        cnt1  += (*m_exp)[i]->GetTotPoints();
                    }

                    StdRegions::StdMatrixKey mkey(gkey.GetMatrixType(),
                                                (*m_exp)[i]->DetExpansionType(),
                                                *((*m_exp)[i]),
                                                  gkey.GetConstants(),varcoeffs);

                    (*m_exp)[i]->GeneralMatrixOp(inarray + cnt,
                                                 e_outarray = outarray+cnt,
                                                 mkey);

                    cnt   += (*m_exp)[i]->GetNcoeffs();
                }
            }
        }


        /**
         * Retrieves local matrices from each expansion in the expansion list
         * and combines them together to generate a global matrix system.
         * @param   mkey        Matrix key for the matrix to be generated.
         * @param   locToGloMap Local to global mapping.
         * @returns Shared pointer to the generated global matrix.
         */
        GlobalMatrixSharedPtr ExpList::GenGlobalMatrix(
                            const GlobalMatrixKey &mkey,
                            const LocalToGlobalC0ContMapSharedPtr &locToGloMap)
        {
            int i,j,n,gid1,gid2,cntdim1,cntdim2,cnt1;
            NekDouble sign1,sign2,matrixid;
            DNekScalMatSharedPtr loc_mat;

            unsigned int glob_rows;
            unsigned int glob_cols;
            unsigned int loc_rows;
            unsigned int loc_cols;

            bool assembleFirstDim;
            bool assembleSecondDim;

            switch(mkey.GetMatrixType())
            {
            case StdRegions::eBwdTrans:
                {
                    glob_rows = m_npoints;
                    glob_cols = locToGloMap->GetNumGlobalCoeffs();

                    assembleFirstDim  = false;
                    assembleSecondDim = true;
                }
                break;
            case StdRegions::eIProductWRTBase:
                {
                    glob_rows = locToGloMap->GetNumGlobalCoeffs();
                    glob_cols = m_npoints;

                    assembleFirstDim  = true;
                    assembleSecondDim = false;
                }
                break;
            case StdRegions::eMass:
            case StdRegions::eHelmholtz:
            case StdRegions::eLaplacian:
                {
                    glob_rows = locToGloMap->GetNumGlobalCoeffs();
                    glob_cols = locToGloMap->GetNumGlobalCoeffs();

                    assembleFirstDim  = true;
                    assembleSecondDim = true;
                }
                break;
            default:
                {
                    NEKERROR(ErrorUtil::efatal, 
                             "Global Matrix creation not defined for this type "
                             "of matrix");
                }
            }

            map< pair< int,  int>, NekDouble > spcoomat;
            pair<int,int> coord;

            int nvarcoeffs = mkey.GetNvariableCoefficients();
            Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);

            // fill global matrix
            for(n = cntdim1 = cntdim2 = cnt1 = matrixid = 0; n < (*m_exp).size(); ++n)
            {
                if(nvarcoeffs>0)
                {
                    for(j = 0; j < nvarcoeffs; j++)
                    {
                        
                        ASSERTL0(false,"method not set up for non-Dirichlet conditions");

                        varcoeffs[j] = mkey.GetVariableCoefficient(j) + cnt1;
                    }
                    cnt1  += (*m_exp)[n]->GetTotPoints();
                    matrixid++;
                }

                LocalRegions::MatrixKey matkey(mkey.GetMatrixType(),
                                               (*m_exp)[n]->DetExpansionType(),
                                               *(*m_exp)[n],
                                               mkey.GetConstants(),
                                               varcoeffs,
                                               matrixid);

                loc_mat = (*m_exp)[n]->GetLocMatrix(matkey);
                loc_rows = loc_mat->GetRows();
                loc_cols = loc_mat->GetColumns();

                for(i = 0; i < loc_rows; ++i)
                {
                    if(assembleFirstDim)
                    {
                        gid1  = locToGloMap->GetLocalToGlobalMap (cntdim1 + i);
                        sign1 = locToGloMap->GetLocalToGlobalSign(cntdim1 + i);
                    }
                    else
                    {
                        gid1  = cntdim1 + i;
                        sign1 = 1.0;
                    }

                    for(j = 0; j < loc_cols; ++j)
                    {
                        if(assembleSecondDim)
                        {
                            gid2  = locToGloMap
                                            ->GetLocalToGlobalMap(cntdim2 + j);
                            sign2 = locToGloMap
                                            ->GetLocalToGlobalSign(cntdim2 + j);
                        }
                        else
                        {
                            gid2  = cntdim2 + j;
                            sign2 = 1.0;
                        }

                        // sparse matrix fill
                        coord = make_pair(gid1,gid2);
                        if( spcoomat.count(coord) == 0 )
                        {
                            spcoomat[coord] = sign1*sign2*(*loc_mat)(i,j);
                        }
                        else
                        {
                            spcoomat[coord] += sign1*sign2*(*loc_mat)(i,j);
                        }
                    }
                }
                cntdim1 += loc_rows;
                cntdim2 += loc_cols;
            }

            return MemoryManager<GlobalMatrix>
                            ::AllocateSharedPtr(glob_rows,glob_cols,spcoomat);
        }


        /**
         * Consider a linear system
         *   \f$\boldsymbol{M\hat{u}}_g=\boldsymbol{\hat{f}}\f$
         * to be solved, where \f$\boldsymbol{M}\f$ is a matrix of type
         * specified by \a mkey. This function assembles the global system
         * matrix \f$\boldsymbol{M}\f$ out of the elemental submatrices
         * \f$\boldsymbol{M}^e\f$. This is equivalent to:
         * \f[ \boldsymbol{M}=\boldsymbol{\mathcal{A}}^T
         * \underline{\boldsymbol{M}}^e\boldsymbol{\mathcal{A}}.\f]
         * where the matrix \f$\boldsymbol{\mathcal{A}}\f$ is a sparse
         * permutation matrix of size \f$N_{\mathrm{eof}}\times
         * N_{\mathrm{dof}}\f$. However, due to the size and sparsity of the
         * matrix \f$\boldsymbol{\mathcal{A}}\f$, it is more efficient to
         * assemble the global matrix using the mapping array \a
         * map\f$[e][i]\f$ contained in the input argument \a locToGloMap.
         * The global assembly is then evaluated as:
         * \f[ \boldsymbol{M}\left[\mathrm{\texttt{map}}[e][i]\right]
         * \left[\mathrm{\texttt{map}}[e][j]\right]
         *       =\mathrm{\texttt{sign}}[e][i]\cdot
         * \mathrm{\texttt{sign}}[e][j] \cdot\boldsymbol{M}^e[i][j]\f]
         * where the values \a sign\f$[e][i]\f$ ensure the correct connectivity.
         *
         * @param   mkey            A key which uniquely defines the global
         *                          matrix to be constructed.
         * @param   locToGloMap     Contains the mapping array and
         *                          required information for the transformation
         *                          from local to global degrees of freedom.
         * @return  (A shared pointer to) the global linear system formed by
         *          the global matrix \f$\boldsymbol{M}\f$.
         */
        GlobalLinSysSharedPtr ExpList::GenGlobalLinSysFullDirect(
                            const GlobalLinSysKey &mkey,
                            const LocalToGlobalC0ContMapSharedPtr &locToGloMap)
        {
            int n,j;
            int cnt1;
            int n_exp = GetExpSize();
            Array<OneD, unsigned int> nCoeffsPerElmt(n_exp);
            for(j = 0; j < n_exp; j++)
            {
                nCoeffsPerElmt[j] = (*m_exp)[j]->GetNcoeffs();
            }

            MatrixStorage blkmatStorage = eDIAGONAL;
            DNekScalBlkMatSharedPtr A = MemoryManager<DNekScalBlkMat>::
                AllocateSharedPtr(nCoeffsPerElmt,nCoeffsPerElmt,blkmatStorage);

            DNekScalMatSharedPtr loc_mat; 

            int nvarcoeffs = mkey.GetNvariableCoefficients();
            Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);
            
            for(n = cnt1 = 0; n < n_exp; ++n)
            {
                if(nvarcoeffs>0)
                {
                    ASSERTL0(false,"method not set up for non-Dirichlet conditions");

                    for(j = 0; j < nvarcoeffs; j++)
                    {
                        varcoeffs[j] = mkey.GetVariableCoefficient(j) + cnt1;
                    }

                    cnt1  +=  (*m_exp)[n]->GetTotPoints();
                }

                LocalRegions::MatrixKey matkey(mkey.GetMatrixType(),
                                               (*m_exp)[n]->DetExpansionType(),
                                               *(*m_exp)[n],
                                               mkey.GetConstants(),
                                               varcoeffs);
                
                loc_mat = (*m_exp)[n]->GetLocMatrix(matkey);  

                A->SetBlock(n,n,loc_mat);
            }

            return MemoryManager<GlobalLinSys>::AllocateSharedPtr(mkey,A,locToGloMap);
        }


        DNekMatSharedPtr ExpList::GenGlobalMatrixFull(const GlobalLinSysKey &mkey, const LocalToGlobalC0ContMapSharedPtr &locToGloMap)
        {
            int i,j,n,gid1,gid2,loc_lda,cnt,cnt1;
            NekDouble sign1,sign2,value;
            DNekScalMatSharedPtr loc_mat;

            int totDofs     = locToGloMap->GetNumGlobalCoeffs();
            int NumDirBCs   = locToGloMap->GetNumGlobalDirBndCoeffs();

            unsigned int rows = totDofs - NumDirBCs;
            unsigned int cols = totDofs - NumDirBCs;
            NekDouble zero = 0.0;

            DNekMatSharedPtr Gmat;
            int bwidth = locToGloMap->GetFullSystemBandWidth();

            int nvarcoeffs = mkey.GetNvariableCoefficients();
            Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);
            MatrixStorage matStorage;

            switch(mkey.GetMatrixType())
            {
                // case for all symmetric matices
            case StdRegions::eHelmholtz:
            case StdRegions::eLaplacian:
                if( (2*(bwidth+1)) < rows)
                {
                    matStorage = ePOSITIVE_DEFINITE_SYMMETRIC_BANDED;
                    Gmat = MemoryManager<DNekMat>::AllocateSharedPtr(rows,cols,zero,matStorage,bwidth,bwidth);
                }
                else
                {
                    matStorage = ePOSITIVE_DEFINITE_SYMMETRIC;
                    Gmat = MemoryManager<DNekMat>::AllocateSharedPtr(rows,cols,zero,matStorage);
                }

                break;
            default: // Assume general matrix - currently only set up for full invert
                {
                    matStorage = eFULL;
                    Gmat = MemoryManager<DNekMat>::AllocateSharedPtr(rows,cols,zero,matStorage);            
                }       
            }             

            // fill global symmetric matrix
            for(n = cnt = cnt1 = 0; n < (*m_exp).size(); ++n)
            {
                if(nvarcoeffs>0)
                {
                    ASSERTL0(false,"method not set up for non-Dirichlet conditions");

                        for(j = 0; j < nvarcoeffs; j++)
                        {
                            varcoeffs[j] = mkey.GetVariableCoefficient(j) + cnt1;
                        }
                        cnt1  += (*m_exp)[n]->GetTotPoints();
                }

                LocalRegions::MatrixKey matkey(mkey.GetMatrixType(),
                                               (*m_exp)[n]->DetExpansionType(),
                                               *(*m_exp)[n],
                                               mkey.GetConstants(),
                                               varcoeffs);

                loc_mat = (*m_exp)[n]->GetLocMatrix(matkey);
                loc_lda = loc_mat->GetColumns();

                for(i = 0; i < loc_lda; ++i)
                {
                    gid1 = locToGloMap->GetLocalToGlobalMap(cnt + i) - NumDirBCs;
                    sign1 =  locToGloMap->GetLocalToGlobalSign(cnt + i);
                    if(gid1 >= 0)
                    {
                        for(j = 0; j < loc_lda; ++j)
                        {
                            gid2 = locToGloMap->GetLocalToGlobalMap(cnt + j) - NumDirBCs;
                            sign2 = locToGloMap->GetLocalToGlobalSign(cnt + j);
                            if(gid2 >= 0)
                            {
                                // When global matrix is symmetric,
                                // only add the value for the upper
                                // triangular part in order to avoid
                                // entries to be entered twice
                                if((matStorage == eFULL)||(gid2 >= gid1))
                                {
                                    value = Gmat->GetValue(gid1,gid2) + sign1*sign2*(*loc_mat)(i,j);
                                    Gmat->SetValue(gid1,gid2,value);
                                }
                            }
                        }
                    }
                }
                cnt   += (*m_exp)[n]->GetNcoeffs();
            }

//             for(i = NumDirBCs; i < NumDirBCs+NumRobinBCs; ++i)
//             {
//                 // Find a way to deal with second parameter of the Robin BC
//                 NekDouble b=1.0;
//                 (*Gmat)((locToGloMap->GetBndCondGlobalID())[i]-NumDirBCs,
//                         (locToGloMap->GetBndCondGlobalID())[i]-NumDirBCs)
//                     -= mkey.GetScaleFactor() * b;
//             }


            return Gmat;
        }


        /**
         * Consider the linear system
         * \f$\boldsymbol{M\hat{u}}_g=\boldsymbol{\hat{f}}\f$.
         * Distinguishing between the boundary and interior components of
         * \f$\boldsymbol{\hat{u}}_g\f$ and \f$\boldsymbol{\hat{f}}\f$ using
         * \f$\boldsymbol{\hat{u}}_b\f$,\f$\boldsymbol{\hat{u}}_i\f$ and
         * \f$\boldsymbol{\hat{f}}_b\f$,\f$\boldsymbol{\hat{f}}_i\f$
         * respectively, this system can be split into its constituent parts as
         * \f[\left[\begin{array}{cc}
         * \boldsymbol{M}_b&\boldsymbol{M}_{c1}\\
         * \boldsymbol{M}_{c2}&\boldsymbol{M}_i\\
         * \end{array}\right]
         * \left[\begin{array}{c}
         * \boldsymbol{\hat{u}_b}\\
         * \boldsymbol{\hat{u}_i}\\
         * \end{array}\right]=
         * \left[\begin{array}{c}
         * \boldsymbol{\hat{f}_b}\\
         * \boldsymbol{\hat{f}_i}\\
         * \end{array}\right]\f]
         * where \f$\boldsymbol{M}_b\f$ represents the components of
         * \f$\boldsymbol{M}\f$ resulting from boundary-boundary mode
         * interactions,
         * \f$\boldsymbol{M}_{c1}\f$ and \f$\boldsymbol{M}_{c2}\f$ represent the
         * components resulting from coupling between the boundary-interior
         * modes, and \f$\boldsymbol{M}_i\f$ represents the components of
         * \f$\boldsymbol{M}\f$ resulting from interior-interior mode
         * interactions.
         *
         * The solution of the linear system can now be determined in two steps:
         * \f{eqnarray*}
         * \mathrm{step 1:}&\quad&(\boldsymbol{M}_b-\boldsymbol{M}_{c1}
         * \boldsymbol{M}_i^{-1}\boldsymbol{M}_{c2}) \boldsymbol{\hat{u}_b} =
         * \boldsymbol{\hat{f}}_b - \boldsymbol{M}_{c1}\boldsymbol{M}_i^{-1}
         * \boldsymbol{\hat{f}}_i,\nonumber \\
         * \mathrm{step 2:}&\quad&\boldsymbol{\hat{u}_i}=\boldsymbol{M}_i^{-1}
         * \left( \boldsymbol{\hat{f}}_i
         *      - \boldsymbol{M}_{c2}\boldsymbol{\hat{u}_b}
         * \right). \nonumber \\ \f}
         * As the inverse of \f$\boldsymbol{M}_i^{-1}\f$ is
         * \f[ \boldsymbol{M}_i^{-1} = \left [\underline{\boldsymbol{M}^e_i}
         * \right ]^{-1} = \underline{[\boldsymbol{M}^e_i]}^{-1} \f]
         * and the following operations can be evaluated as,
         * \f{eqnarray*}
         * \boldsymbol{M}_{c1}\boldsymbol{M}_i^{-1}\boldsymbol{\hat{f}}_i &
         * =& \boldsymbol{\mathcal{A}}_b^T \underline{\boldsymbol{M}^e_{c1}}
         * \underline{[\boldsymbol{M}^e_i]}^{-1} \boldsymbol{\hat{f}}_i \\
         * \boldsymbol{M}_{c2} \boldsymbol{\hat{u}_b} &=&
         * \underline{\boldsymbol{M}^e_{c2}} \boldsymbol{\mathcal{A}}_b
         * \boldsymbol{\hat{u}_b}.\f}
         * where \f$\boldsymbol{\mathcal{A}}_b \f$ is the permutation matrix
         * which scatters from global to local degrees of freedom, only the
         * following four matrices should be constructed:
         * - \f$\underline{[\boldsymbol{M}^e_i]}^{-1}\f$
         * - \f$\underline{\boldsymbol{M}^e_{c1}}
         *                          \underline{[\boldsymbol{M}^e_i]}^{-1}\f$
         * - \f$\underline{\boldsymbol{M}^e_{c2}}\f$
         * - The Schur complement: \f$\boldsymbol{M}_{\mathrm{Schur}}=
         *   \quad\boldsymbol{M}_b-\boldsymbol{M}_{c1}\boldsymbol{M}_i^{-1}
         *   \boldsymbol{M}_{c2}\f$
         *
         * The first three matrices are just a concatenation of the
         * corresponding local matrices and they can be created as such. They
         * also allow for an elemental evaluation of the operations concerned.
         *
         * The global Schur complement however should be assembled from the
         * concatenation of the local elemental Schur complements, that is,
         * \f[ \boldsymbol{M}_{\mathrm{Schur}}=\boldsymbol{M}_b
         *          - \boldsymbol{M}_{c1}
         * \boldsymbol{M}_i^{-1} \boldsymbol{M}_{c2} =
         * \boldsymbol{\mathcal{A}}_b^T \left [\underline{\boldsymbol{M}^e_b -
         * \boldsymbol{M}^e_{c1} [\boldsymbol{M}^e_i]^{-1}
         * (\boldsymbol{M}^e_{c2})} \right ] \boldsymbol{\mathcal{A}}_b \f]
         * and it is the only matrix operation that need to be evaluated on a
         * global level when using static condensation.
         * However, due to the size and sparsity of the matrix
         * \f$\boldsymbol{\mathcal{A}}_b\f$, it is more efficient to assemble
         * the global Schur matrix using the mapping array bmap\f$[e][i]\f$
         * contained in the input argument \a locToGloMap. The global Schur
         * complement is then constructed as:
         * \f[\boldsymbol{M}_{\mathrm{Schur}}\left[\mathrm{\a bmap}[e][i]\right]
         * \left[\mathrm{\a bmap}[e][j]\right]=\mathrm{\a bsign}[e][i]\cdot
         * \mathrm{\a bsign}[e][j]
         * \cdot\boldsymbol{M}^e_{\mathrm{Schur}}[i][j]\f]
         * All four matrices are stored in the \a GlobalLinSys returned by this
         * function.
         *
         * @param   mkey            A key which uniquely defines the global
         *                          matrix to be constructed.
         * @param   locToGloMap     Contains the mapping array and required
         *                          information for the transformation from
         *                          local to global degrees of freedom.
         * @return  (A shared pointer to) the statically condensed global
         *          linear system.
         */
        GlobalLinSysSharedPtr ExpList::GenGlobalLinSysStaticCond(
                        const GlobalLinSysKey &mkey,
                        const LocalToGlobalC0ContMapSharedPtr &locToGloMap)
        {
            int n,j;
            int cnt1;

            // Setup Block Matrix systems
            int n_exp = GetExpSize();
            const Array<OneD,const unsigned int>& nbdry_size = locToGloMap->GetNumLocalBndCoeffsPerPatch();
            const Array<OneD,const unsigned int>& nint_size  = locToGloMap->GetNumLocalIntCoeffsPerPatch();

            DNekScalBlkMatSharedPtr BinvD;
            DNekScalBlkMatSharedPtr invD;
            DNekScalBlkMatSharedPtr C;
            DNekScalBlkMatSharedPtr SchurCompl;

            MatrixStorage blkmatStorage = eDIAGONAL;
            SchurCompl = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nbdry_size, nbdry_size, blkmatStorage);
            BinvD      = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nbdry_size, nint_size , blkmatStorage);
            C          = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nint_size , nbdry_size, blkmatStorage);
            invD       = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nint_size , nint_size , blkmatStorage);

            DNekScalBlkMatSharedPtr loc_mat;
            DNekScalMatSharedPtr tmp_mat; 

            int nvarcoeffs = mkey.GetNvariableCoefficients();
            Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);

            for(n = cnt1 = 0; n < n_exp; ++n)
            {
                if(nvarcoeffs>0)
                {
                    ASSERTL0(false,"method not set up for non-Dirichlet conditions");

                    for(j = 0; j < nvarcoeffs; j++)
                    {
                        varcoeffs[j] = mkey.GetVariableCoefficient(j) + cnt1;
                    }
                    cnt1  += (*m_exp)[n]->GetTotPoints();
                }

                LocalRegions::MatrixKey matkey(mkey.GetMatrixType(),
                                               (*m_exp)[n]->DetExpansionType(),
                                               *(*m_exp)[n],
                                               mkey.GetConstants(),
                                               varcoeffs);

                loc_mat = (*m_exp)[n]->GetLocStaticCondMatrix(matkey);

                SchurCompl->SetBlock(n,n, tmp_mat = loc_mat->GetBlock(0,0));
                BinvD     ->SetBlock(n,n, tmp_mat = loc_mat->GetBlock(0,1));
                C         ->SetBlock(n,n, tmp_mat = loc_mat->GetBlock(1,0));
                invD      ->SetBlock(n,n, tmp_mat = loc_mat->GetBlock(1,1));
            }

            return MemoryManager<GlobalLinSys>::AllocateSharedPtr(mkey,SchurCompl,BinvD,C,invD,locToGloMap);
        }


        /**
         * Consider a linear system
         * \f$\boldsymbol{M\hat{u}}_g=\boldsymbol{f}\f$ to be solved. Dependent
         * on the solution method, this function constructs
         * - <b>The full linear system</b><BR>
         *   A call to the function #GenGlobalLinSysFullDirect
         * - <b>The statically condensed linear system</b><BR>
         *   A call to the function #GenGlobalLinSysStaticCond
         *
         * @param   mkey            A key which uniquely defines the global
         *                          matrix to be constructed.
         * @param   locToGloMap     Contains the mapping array and required
         *                          information for the transformation from
         *                          local to global degrees of freedom.
         * @return  (A shared pointer to) the global linear system in
         *          required format.
         */
        GlobalLinSysSharedPtr ExpList::GenGlobalLinSys(
                        const GlobalLinSysKey &mkey,
                        const LocalToGlobalC0ContMapSharedPtr &locToGloMap)
        {
            GlobalLinSysSharedPtr returnlinsys;

            switch(mkey.GetGlobalSysSolnType())
            {
            case eDirectFullMatrix:
                {
                    returnlinsys = GenGlobalLinSysFullDirect(mkey, locToGloMap);
                }
                break;
            case eDirectStaticCond:  
                {
                    ASSERTL1(locToGloMap->GetGlobalSysSolnType()==eDirectStaticCond,
                             "The local to global map is not set up for this solution type");
                    returnlinsys = GenGlobalLinSysStaticCond(mkey, locToGloMap);
                }
                break;
            case eDirectMultiLevelStaticCond:
                {
                    ASSERTL1(locToGloMap->GetGlobalSysSolnType()==eDirectMultiLevelStaticCond,
                             "The local to global map is not set up for this solution type");
                    returnlinsys = GenGlobalLinSysStaticCond(mkey, locToGloMap);                    
                }
                break;
            default:
                ASSERTL0(false,"Matrix solution type not defined");
                break;
            }

            return returnlinsys;
        }

        GlobalLinSysSharedPtr ExpList::GenGlobalBndLinSys(
                        const GlobalLinSysKey     &mkey,
                        const LocalToGlobalBaseMapSharedPtr &locToGloMap)
        {
            StdRegions::MatrixType linsystype = mkey.GetMatrixType();
            ASSERTL0(linsystype == StdRegions::eHybridDGHelmBndLam,
                     "Routine currently only tested for HybridDGHelmholtz");
            ASSERTL1(mkey.GetGlobalSysSolnType()!=eDirectFullMatrix,
                     "This BndLinSys cannot be constructed in case of a full matrix global solve");
            ASSERTL1(mkey.GetGlobalSysSolnType()==locToGloMap->GetGlobalSysSolnType(),
                     "The local to global map is not set up for the requested solution type");

            // We will set up this matrix as a statically condensed system 
            // where the interior blocks are zero
            int n,j;
            int cnt1,matrixid;

            NekDouble factor1, factor2;

            // Setup Block Matrix systems
            int n_exp = GetExpSize();
            const Array<OneD,const unsigned int>& nbdry_size = locToGloMap->GetNumLocalBndCoeffsPerPatch();
            const Array<OneD,const unsigned int>& nint_size  = locToGloMap->GetNumLocalIntCoeffsPerPatch();

            DNekScalBlkMatSharedPtr BinvD;
            DNekScalBlkMatSharedPtr invD;
            DNekScalBlkMatSharedPtr C;
            DNekScalBlkMatSharedPtr SchurCompl;

            MatrixStorage blkmatStorage = eDIAGONAL;
            SchurCompl = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nbdry_size, nbdry_size, blkmatStorage);
            BinvD      = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nbdry_size, nint_size , blkmatStorage);
            C          = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nint_size , nbdry_size, blkmatStorage);
            invD       = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nint_size , nint_size , blkmatStorage);

            DNekScalMatSharedPtr loc_mat;

            int totnq, nvarcoeffs = mkey.GetNvariableCoefficients();
            Array<OneD, Array<OneD,NekDouble> > varcoeffs(nvarcoeffs);
            for(n = cnt1 = matrixid = 0; n < n_exp; ++n)
            {
                 totnq = GetCoordim(n)*( (*m_exp)[n]->GetTotPoints() );
                if(nvarcoeffs>0)
                {
                    // When two varcoeffs in a specific order            
                    for(j = 0; j < nvarcoeffs; j++)
                    {
                        varcoeffs[j] = Array<OneD, NekDouble>(totnq,0.0);
                        Vmath::Vcopy(totnq, &(mkey.GetVariableCoefficient(j))[cnt1], 1, &varcoeffs[j][0],1);
                    }
                    
                    cnt1  += totnq;
                    matrixid++;
                }

                int Nconstants = mkey.GetNconstants();
                if(Nconstants>2)
                {
                    factor1 = mkey.GetConstant(n);
                    factor2 = mkey.GetConstant(Nconstants-1);
                }

                else
                {
                    factor1 = mkey.GetConstant(0);
                    factor2 = mkey.GetConstant(1);
                }

                LocalRegions::MatrixKey Umatkey(linsystype, 
                                                (*m_exp)[n]->DetExpansionType(),
                                                *((*m_exp)[n]), factor1,factor2,varcoeffs,matrixid);

                DNekScalMat &BndSys = *((*m_exp)[n]->GetLocMatrix(Umatkey)); 

               LocalRegions::MatrixKey matkey(linsystype,
                                               (*m_exp)[n]->DetExpansionType(),
                                               *(*m_exp)[n],factor1,factor2,varcoeffs,matrixid);

                loc_mat = (*m_exp)[n]->GetLocMatrix(matkey);    

                SchurCompl->SetBlock(n,n,loc_mat);              
            }
           
            return MemoryManager<GlobalLinSys>::AllocateSharedPtr(mkey,SchurCompl,BinvD,C,invD,locToGloMap);
        }


        /**
         * Given the elemental coefficients \f$\hat{u}_n^e\f$ of
         * an expansion, this function evaluates the spectral/hp
         * expansion \f$u^{\delta}(\boldsymbol{x})\f$ at the
         * quadrature points \f$\boldsymbol{x}_i\f$. The operation
         * is evaluated locally by the elemental function
         * StdRegions#StdExpansion#BwdTrans.
         *
         * @param   inarray         An array of size \f$N_{\mathrm{eof}}\f$
         *                          containing the local coefficients
         *                          \f$\hat{u}_n^e\f$.
         * @param   outarray        The resulting physical values at the
         *                          quadrature points
         *                          \f$u^{\delta}(\boldsymbol{x}_i)\f$
         *                          will be stored in this array of size
         *                          \f$Q_{\mathrm{tot}}\f$.
         */
        void ExpList::BwdTrans_IterPerExp(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD, NekDouble> &outarray)
        {
            bool doBlockMatOp
                    = m_globalOptParam->DoBlockMatOp(StdRegions::eBwdTrans);

            if(doBlockMatOp)
            {
                GlobalMatrixKey mkey(StdRegions::eBwdTrans);
                MultiplyByBlockMatrix(mkey,inarray,outarray);
            }
            else
            {
                int  i;
                int  cnt  = 0;
                int  cnt1 = 0;
                Array<OneD,NekDouble> e_outarray;

                for(i= 0; i < GetExpSize(); ++i)
                {
                    (*m_exp)[i]->BwdTrans(inarray + cnt,
                                          e_outarray = outarray+cnt1);
                    cnt   += (*m_exp)[i]->GetNcoeffs();
                    cnt1  += (*m_exp)[i]->GetTotPoints();
                }
            }
        }

        StdRegions::StdExpansionSharedPtr& ExpList::GetExp(
                    const Array<OneD, const NekDouble> &gloCoord)
        {
            Array<OneD, NekDouble> stdCoord(GetCoordim(0),0.0);
            for (int i = 0; i < GetExpSize(); ++i)
            {
                if ((*m_exp)[i]->GetGeom()->ContainsPoint(gloCoord))
                {
                    return (*m_exp)[i];
                }
            }
        }

        int ExpList::GetExpIndex(
                    const Array<OneD, const NekDouble> &gloCoord)
        {
            Array<OneD, NekDouble> stdCoord(GetCoordim(0),0.0);
            for (int i = 0; i < GetExpSize(); ++i)
            {
                if ((*m_exp)[i]->GetGeom()->ContainsPoint(gloCoord))
                {
                    return i;
                }
            }
        }

        /**
         * The operation is evaluated locally by the elemental
         * function StdRegions#StdExpansion#GetCoords.
         *
         * @param   coord_0         After calculation, the \f$x_1\f$ coordinate
         *                          will be stored in this array.
         * @param   coord_1         After calculation, the \f$x_2\f$ coordinate
         *                          will be stored in this array.
         * @param   coord_2         After calculation, the \f$x_3\f$ coordinate
         *                          will be stored in this array.
         */
        void ExpList::GetCoords(Array<OneD, NekDouble> &coord_0,
                                Array<OneD, NekDouble> &coord_1,
                                Array<OneD, NekDouble> &coord_2)
        {
            int    i, cnt = 0;
            Array<OneD, NekDouble> e_coord_0;
            Array<OneD, NekDouble> e_coord_1;
            Array<OneD, NekDouble> e_coord_2;

            switch(GetExp(0)->GetCoordim())
            {
            case 1:
                for(i= 0; i < GetExpSize(); ++i)
                {
                    e_coord_0 = coord_0 + cnt;
                    (*m_exp)[i]->GetCoords(e_coord_0);
                    cnt  += (*m_exp)[i]->GetTotPoints();
                }
                break;
            case 2:
                ASSERTL0(coord_1.num_elements() != 0,
                         "output coord_1 is not defined");

                for(i= 0; i < GetExpSize(); ++i)
                {
                    e_coord_0 = coord_0 + cnt;
                    e_coord_1 = coord_1 + cnt;
                    (*m_exp)[i]->GetCoords(e_coord_0,e_coord_1);
                    cnt  += (*m_exp)[i]->GetTotPoints();
                }
                break;
            case 3:
                ASSERTL0(coord_1.num_elements() != 0,
                         "output coord_1 is not defined");
                ASSERTL0(coord_2.num_elements() != 0,
                         "output coord_2 is not defined");

                for(i= 0; i < GetExpSize(); ++i)
                {
                    e_coord_0 = coord_0 + cnt;
                    e_coord_1 = coord_1 + cnt;
                    e_coord_2 = coord_2 + cnt;

                    (*m_exp)[i]->GetCoords(e_coord_0,e_coord_1,e_coord_2);
                    cnt  += (*m_exp)[i]->GetTotPoints();
                }
                break;
            }
        }

        /**
         * The operation is evaluated locally by the elemental
         * function StdRegions#StdExpansion#GetSurfaceNormal.
         */
        void ExpList::GetSurfaceNormal(Array<OneD, NekDouble> &SurfaceNormal,
                                const int k)
        {
            int i, cnt=0;
            Array<OneD, NekDouble> e_SN;

            for(i = 0; i < GetExpSize(); ++i)
            {
                e_SN = SurfaceNormal + cnt;
                (*m_exp)[i]->GetSurfaceNormal(e_SN, k);
                cnt += (*m_exp)[i]->GetTotPoints();
            }
        }

        void ExpList::GetTangents(
                Array<OneD, Array<OneD, Array<OneD, NekDouble> > > &tangents)
        {
            int i,j,k,e_npoints,offset;
            Array<OneD,Array<OneD, NekDouble> > loctangent;

            // Assume whole array is of same coordinate dimension
            int coordim = (*m_exp)[0]->GetGeom()->GetCoordim();

            ASSERTL0(tangents.num_elements() > 0,
                     "Must have storage for at least one tangent");
            ASSERTL1(tangents[0].num_elements() >= coordim,
                     "Output vector does not have sufficient dimensions to "
                     "match coordim");

            // Process each expansion.
            for(i = 0; i < m_exp->size(); ++i)
            {
                // Get the number of points and normals for this expansion.
                e_npoints  = (*m_exp)[i]->GetTotPoints();
                offset = m_phys_offset[i];
                
                for (j = 0; j < tangents.num_elements(); ++j)
                {
                    loctangent = (*m_exp)[i]->GetMetricInfo()->GetTangent(j);
                    // Get the physical data offset for this expansion.

                    for (k = 0; k < coordim; ++k)
                    {
                        Vmath::Vcopy(e_npoints, &(loctangent[k][0]), 1, 
                                                &(tangents[j][k][offset]), 1);
                    }
                }
            }
            
        }
        
        
        /**
         * Configures geometric info, such as tangent direction, on each
         * expansion.
         * @param   graph2D         Mesh
         */
        void ExpList::ApplyGeomInfo(SpatialDomains::MeshGraph &graph)
        {
            std::string dir = "TangentX";
            Array<OneD,NekDouble> coords(2);
            
            // Retrieve geometric info from session.
            if(graph.CheckForGeomInfo("TangentDir"))
            {
                dir = graph.GetGeomInfo("TangentDir");
            }
            if (graph.CheckForGeomInfo("TangentCentreX")
                    && graph.CheckForGeomInfo("TangentCentreY"))
            {
                
                coords[0] = atof(graph.GetGeomInfo("TangentCentreX").c_str());
                coords[1] = atof(graph.GetGeomInfo("TangentCentreY").c_str());
            }
            
            // Apply geometric info to each expansion.
            for (int i = 0; i < m_exp->size(); ++i)
            {
                (*m_exp)[i]->GetMetricInfo()->SetTangentOrientation(dir);
                (*m_exp)[i]->GetMetricInfo()->SetTangentCircularCentre(coords);
            }
        }
        
        
        /**
         * The coordinates of the quadrature points, together with
         * the content of the array #m_phys, are written to the
         * file \a out.
         *
         * @param   out             The file to which the solution should be
         *                          written.
         */
        void ExpList::WriteToFile(std::ofstream &out, OutputFormat format,
                                std::string var)
        {
            if(format==eTecplot)
            {
                int i,cnt = 0;

                Array<OneD, const NekDouble> phys = m_phys;

                if(m_physState == false)
                {
                    BwdTrans(m_coeffs,m_phys);
                }

                (*m_exp)[0]->SetPhys(phys);
                (*m_exp)[0]->WriteToFile(out,eTecplot,true,var);
                cnt  += (*m_exp)[0]->GetTotPoints();

                for(i= 1; i < GetExpSize(); ++i)
                {
                    (*m_exp)[i]->SetPhys(phys+cnt);
                    (*m_exp)[i]->WriteToFile(out,eTecplot,false,var);
                    cnt  += (*m_exp)[i]->GetTotPoints();
                }
            }
            else if(format==eGnuplot)
            {
                int i,cnt = 0;

                Array<OneD, const NekDouble> phys = m_phys;

                if(m_physState == false)
                {
                    BwdTrans(m_coeffs,m_phys);
                }

                (*m_exp)[0]->SetPhys(phys);
                (*m_exp)[0]->WriteToFile(out,eGnuplot,true,var);
                cnt  += (*m_exp)[0]->GetTotPoints();

                for(i= 1; i < GetExpSize(); ++i)
                {
                    (*m_exp)[i]->SetPhys(phys+cnt);
                    (*m_exp)[i]->WriteToFile(out,eGnuplot,false,var);
                    cnt  += (*m_exp)[i]->GetTotPoints();
                }
            }
            else if(format==eGmsh)
            {

                out<<"View.MaxRecursionLevel = 8;"<<endl;
                out<<"View.TargetError = 0.00;"<<endl;

                int i,j,k;
                int nElementalCoeffs =  (*m_exp)[0]->GetBasisNumModes(0);
                StdRegions::ExpansionType locShape
                                            = (*m_exp)[0]->DetExpansionType();

                int nDumpCoeffs =  nElementalCoeffs*nElementalCoeffs;
                Array<TwoD, int> exponentMap(nDumpCoeffs,3,0);
                int cnt = 0;
                for(i = 0; i < nElementalCoeffs; i++)
                {
                    for(j = 0; j < nElementalCoeffs; j++)
                    {
                        exponentMap[cnt][0] = j;
                        exponentMap[cnt++][1] = i;
                    }
                }

                PutCoeffsInToElmtExp();
                bool dumpNewView = true;
                bool closeView = false;
                for(i= 0; i < GetExpSize(); ++i)
                {
                    if(nElementalCoeffs != (*m_exp)[i]->GetBasisNumModes(0))
                    {
                        ASSERTL0(false,"Not all elements have the same number "
                                       "of expansions, this will probably lead "
                                       "to a corrupt Gmsh-output file.")
                    }

                    if(i>0)
                    {
                        if ( ((*m_exp)[i]->DetExpansionType())
                                        !=((*m_exp)[i-1]->DetExpansionType()) )
                        {
                            dumpNewView = true;
                        }
                        else
                        {
                            dumpNewView = false;
                        }
                    }
                    if(i<GetExpSize()-1)
                    {
                        if ( ((*m_exp)[i]->DetExpansionType())
                                        !=((*m_exp)[i+1]->DetExpansionType()) )
                        {
                            closeView = true;
                        }
                        else
                        {
                            closeView = false;
                        }
                    }
                    else
                    {
                            closeView = true;
                    }

                    if(dumpNewView)
                    {
                        out<<"View \" \" {"<<endl;
                    }

                    (*m_exp)[i]->WriteToFile(out,eGmsh,false);

                    if(closeView)
                    {
                        out<<"INTERPOLATION_SCHEME"<<endl;
                        out<<"{"<<endl;
                        for(k=0; k < nDumpCoeffs; k++)
                        {
                            out<<"{";
                            for(j = 0; j < nDumpCoeffs; j++)
                            {
                                if(k==j)
                                {
                                    out<<"1.00";
                                }
                                else
                                {
                                    out<<"0.00";
                                }
                                if(j < nDumpCoeffs - 1)
                                {
                                    out<<", ";
                                }
                            }
                            if(k < nDumpCoeffs - 1)
                            {
                                out<<"},"<<endl;
                            }
                            else
                            {
                                out<<"}"<<endl<<"}"<<endl;
                            }
                        }

                        out<<"{"<<endl;
                        for(k=0; k < nDumpCoeffs; k++)
                        {
                            out<<"{";
                            for(j = 0; j < 3; j++)
                            {
                                out<<exponentMap[k][j];
                                if(j < 2)
                                {
                                    out<<", ";
                                }
                            }
                            if(k < nDumpCoeffs - 1)
                            {
                                out<<"},"<<endl;
                            }
                            else
                            {
                                out<<"}"<<endl<<"};"<<endl;
                            }
                        }
                        out<<"};"<<endl;
                    }
                }
                out<<"Combine ElementsFromAllViews;"<<endl;
                out<<"View.Name = \"\";"<<endl;
            }
            else
            {
                ASSERTL0(false, "Output routine not implemented for requested "
                                "type of output");
            }
        }


        /**
         * Write Tecplot Files Header
         * @param   outfile Output file name.
         * @param   var                 variables names
         */
        void ExpList::WriteTecplotHeader(std::ofstream &outfile, 
                        std::string var)
        {

            int coordim  = GetExp(0)->GetCoordim();
            outfile << "Variables = x";

            if(coordim == 2)
            {
                outfile << ", y";
            }
            else if (coordim == 3)
            {
                outfile << ", y, z";
            }
            outfile << ", "<< var << std::endl << std::endl;
        }


        /**
         * Write Tecplot Files Zone
         * @param   outfile    Output file name.
         * @param   expansion  Expansion that is considered
         */
        void ExpList::WriteTecplotZone(std::ofstream &outfile, int expansion)
        {
            (*m_exp)[expansion]->WriteTecplotZone(outfile);
        }


        /**
         * Write Tecplot Files Field
         * @param   outfile    Output file name.
         * @param   expansion  Expansion that is considered
         */
        void ExpList::WriteTecplotField(std::ofstream &outfile, int expansion)
        {
            int cnt = 0;
            for(int i= 0; i < expansion; ++i)
            {
                cnt  += (*m_exp)[i]->GetTotPoints();
            }
            (*m_exp)[expansion]->SetPhys(m_phys+cnt);
            (*m_exp)[expansion]->WriteTecplotField(outfile);
        }


        void ExpList::WriteVtkHeader(std::ofstream &outfile)
        {
            outfile << "<?xml version=\"1.0\"?>" << endl;
            outfile << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" "
                    << "byte_order=\"LittleEndian\">" << endl;
            outfile << "  <UnstructuredGrid>" << endl;
        }

        void ExpList::WriteVtkFooter(std::ofstream &outfile)
        {
            outfile << "  </UnstructuredGrid>" << endl;
            outfile << "</VTKFile>" << endl;
        }

        void ExpList::WriteVtkPieceHeader(std::ofstream &outfile, int expansion)
        {
            (*m_exp)[expansion]->WriteVtkPieceHeader(outfile);
        }

        void ExpList::WriteVtkPieceFooter(std::ofstream &outfile, int expansion)
        {
            (*m_exp)[expansion]->WriteVtkPieceFooter(outfile);
        }

        void ExpList::WriteVtkPieceData(std::ofstream &outfile, int expansion,
                                        std::string var)
        {
            int cnt = 0;
            for (int i = 0; i < expansion; ++i)
            {
                cnt += (*m_exp)[i]->GetTotPoints();
            }
            (*m_exp)[expansion]->SetPhys(m_phys + cnt);
            (*m_exp)[expansion]->WriteVtkPieceData(outfile, var);
        }

        void ExpList::ReadFromFile(std::ifstream &in, OutputFormat format)
        {
            if(format==eTecplot)
            {
                int i,npts,cnt = 0;
                Array<OneD, NekDouble> phys = m_phys;

                npts = (*m_exp)[0]->GetTotPoints();
                (*m_exp)[0]->ReadFromFile(in,eTecplot,true);
                Vmath::Vcopy(npts,&(*m_exp)[0]->GetPhys()[0],1,&phys[cnt],1);
                cnt  += npts;

                for(i= 1; i < GetExpSize(); ++i)
                {
                    npts = (*m_exp)[i]->GetTotPoints();
                    (*m_exp)[i]->ReadFromFile(in,eTecplot,false);
                    Vmath::Vcopy(npts,&((*m_exp)[i]->GetPhys())[0],1,
                                 &phys[cnt],1);
                    cnt  += npts;
                }
                FwdTrans(m_phys,m_coeffs);
            }
            else
            {
                ASSERTL0(false, "Output routine not implemented for requested "
                                "type of output");
            }
        }

        /**
         * Given a spectral/hp approximation
         * \f$u^{\delta}(\boldsymbol{x})\f$ evaluated at the quadrature points
         * (which should be contained in #m_phys), this function calculates the
         * \f$L_\infty\f$ error of this approximation with respect to an exact
         * solution. The local distribution of the quadrature points allows an
         * elemental evaluation of this operation through the functions
         * StdRegions#StdExpansion#Linf.
         *
         * The exact solution, also evaluated at the quadrature
         * points, should be contained in the variable #m_phys of
         * the ExpList object \a Sol.
         *
         * @param   soln            A 1D array, containing the discrete
         *                          evaluation of the exact solution at the
         *                          quadrature points in its array #m_phys.
         * @return  The \f$L_\infty\f$ error of the approximation.
         */
        NekDouble  ExpList::Linf(const Array<OneD, const NekDouble> &soln)
        {
            NekDouble err = 0.0;
            int       i,cnt = 0;

            for(i= 0; i < GetExpSize(); ++i)
            {
                // set up physical solution in local element
                (*m_exp)[i]->SetPhys(m_phys+cnt);
                err  = std::max(err,(*m_exp)[i]->Linf(soln + cnt));
                cnt  += (*m_exp)[i]->GetTotPoints();
            }

            return err;
        }

        /**
         * Given a spectral/hp approximation \f$u^{\delta}(\boldsymbol{x})\f$
         * evaluated at the quadrature points (which should be contained in
         * #m_phys), this function calculates the \f$L_2\f$ error of this
         * approximation with respect to an exact solution. The local
         * distribution of the quadrature points allows an elemental evaluation
         * of this operation through the functions StdRegions#StdExpansion#L2.
         *
         * The exact solution, also evaluated at the quadrature points, should
         * be contained in the variable #m_phys of the ExpList object \a Sol.
         *
         * @param   Sol             An ExpList, containing the discrete
         *                          evaluation of the exact solution at the
         *                          quadrature points in its array #m_phys.
         * @return  The \f$L_2\f$ error of the approximation.
         */
        NekDouble ExpList::L2(const Array<OneD, const NekDouble> &soln)
        {

            NekDouble err = 0.0,errl2;
            int    i,cnt = 0;

            for(i= 0; i < GetExpSize(); ++i)
            {
                // set up physical solution in local element
                (*m_exp)[i]->SetPhys(m_phys+cnt);
                errl2 = (*m_exp)[i]->L2(soln+cnt);
                err += errl2*errl2;
                cnt  += (*m_exp)[i]->GetTotPoints();
            }

            return sqrt(err);
        }


        /**
         * Given a spectral/hp approximation
         * \f$u^{\delta}(\boldsymbol{x})\f$ evaluated at the
         * quadrature points (which should be contained in #m_phys),
         * this function calculates the \f$L_2\f$ measure of this
         * approximation. The local distribution of the quadrature
         * points allows an elemental evaluation of this operation
         * through the functions StdRegions#StdExpansion#L2.
         *
         * The exact solution, also evaluated at the quadrature points, should
         * be contained in the variable #m_phys of the ExpList object \a Sol.
         *
         * @param   soln            A 1D array, containing the discrete
         *                          evaluation of the exact solution at the
         *                          quadrature points.
         * @return  The \f$L_2\f$ error of the approximation.
         */
        NekDouble ExpList::L2(void)
        {

            NekDouble err = 0.0,errl2;
            int    i,cnt = 0;

            for(i= 0; i < GetExpSize(); ++i)
            {
                // set up physical solution in local element
                (*m_exp)[i]->SetPhys(m_phys+cnt);
                errl2 = (*m_exp)[i]->L2();
                err += errl2*errl2;
                cnt  += (*m_exp)[i]->GetTotPoints();
            }

            return sqrt(err);
        }

        /**
         * Given a spectral/hp approximation
         * \f$u^{\delta}(\boldsymbol{x})\f$ evaluated at the quadrature points
         * (which should be contained in #m_phys), this function calculates the
         * \f$H^1_2\f$ error of this approximation with respect to an exact
         * solution. The local distribution of the quadrature points allows an
         * elemental evaluation of this operation through the functions
         * StdRegions#StdExpansion#H1.
         *
         * The exact solution, also evaluated at the quadrature points, should
         * be contained in the variable #m_phys of the ExpList object \a Sol.
         *
         * @param   soln        An 1D array, containing the discrete evaluation
         *                      of the exact solution at the quadrature points.
         *
         * @return  The \f$H^1_2\f$ error of the approximation.
         */
        NekDouble ExpList::H1(const Array<OneD, const NekDouble> &soln)
        {

            NekDouble err = 0.0,errh1;
            int    i,cnt = 0;

            for(i= 0; i < GetExpSize(); ++i)
            {
                // set up physical solution in local element
                (*m_exp)[i]->SetPhys(m_phys+cnt);
                errh1 =  (*m_exp)[i]->H1(soln+cnt);
                err  += errh1*errh1;
                cnt  += (*m_exp)[i]->GetTotPoints();
            }

            return sqrt(err);
        }

        std::vector<SpatialDomains::FieldDefinitionsSharedPtr> ExpList::GetFieldDefinitions(void)
        {

            std::vector<SpatialDomains::FieldDefinitionsSharedPtr> returnval;

            int startenum, endenum, s;

            // count number of shapes
            switch((*m_exp)[0]->GetShapeDimension())
            {
            case 1:
                startenum = (int) SpatialDomains::eSegment;
                endenum   = (int) SpatialDomains::eSegment;
                break;
            case 2:
                startenum = (int) SpatialDomains::eTriangle;
                endenum   = (int) SpatialDomains::eQuadrilateral;
                break;
            case 3:
                startenum = (int) SpatialDomains::eTetrahedron;
                endenum   = (int) SpatialDomains::eHexahedron;
                break;
            }

            for(s = startenum; s <= endenum; ++s)
            {
                SpatialDomains::GeomShapeType         shape;
                std::vector<unsigned int>             elementIDs;
                std::vector<LibUtilities::BasisType>  basis;
                std::vector<unsigned int>             numModes;
                std::vector<std::string>              fields;

                bool first    = true;
                bool UniOrder = true;

                for(int i = 0; i < (*m_exp).size(); ++i)
                {
                    if((*m_exp)[i]->GetGeom()->GetGeomShapeType() == (SpatialDomains::GeomShapeType) s)
                    {
                        elementIDs.push_back((*m_exp)[i]->GetGeom()->GetGlobalID());
                        if(first)
                        {
                            shape = (SpatialDomains::GeomShapeType) s;
                            for(int j = 0; j < (*m_exp)[i]->GetNumBases(); ++j)
                            {
                                basis.push_back((*m_exp)[i]->GetBasis(j)->GetBasisType());
                                numModes.push_back((*m_exp)[i]->GetBasis(j)->GetNumModes());
                            }
                            first = false;
                        }
                        else
                        {
                            ASSERTL0((*m_exp)[i]->GetBasis(0)->GetBasisType() == basis[0],"Routine is not yet set up for multiple basese definitions");

                            for(int j = 0; j < (*m_exp)[i]->GetNumBases(); ++j)
                            {
                                numModes.push_back((*m_exp)[i]->GetBasis(j)->GetNumModes());
                                if(numModes[j] != (*m_exp)[i]->GetBasis(j)->GetNumModes())
                                {
                                    UniOrder = false;
                                }
                            }
                        }
                    }
                }

                if(elementIDs.size() > 0)
                {
                    SpatialDomains::FieldDefinitionsSharedPtr fielddef  = MemoryManager<SpatialDomains::FieldDefinitions>::AllocateSharedPtr(shape, elementIDs, basis, UniOrder, numModes,fields);
                    returnval.push_back(fielddef);
                }
            }

            return returnval;
        }

        //Append the element data listed in elements
        //fielddef->m_ElementIDs onto fielddata
        void ExpList::AppendFieldData(SpatialDomains::FieldDefinitionsSharedPtr &fielddef, std::vector<NekDouble> &fielddata)
        {
            for(int i = 0; i < fielddef->m_ElementIDs.size(); ++i)
            {
                int eid = fielddef->m_ElementIDs[i];
                int datalen = (*m_exp)[eid]->GetNcoeffs();
                fielddata.insert(fielddata.end(),&m_coeffs[m_coeff_offset[eid]],&m_coeffs[m_coeff_offset[eid]]+datalen);
            }
        }

        //Extract the data in fielddata into the m_coeff list
        void ExpList::ExtractDataToCoeffs(SpatialDomains::FieldDefinitionsSharedPtr &fielddef, std::vector<NekDouble> &fielddata, std::string &field)
        {
            int cnt = 0;
            int i;
            int offset = 0;
            int datalen = fielddata.size()/fielddef->m_Fields.size();

            // Find data location according to field definition
            for(i = 0; i < fielddef->m_Fields.size(); ++i)
            {
                if(fielddef->m_Fields[i] == field)
                {
                    break;
                }
                offset += datalen;
            }

            ASSERTL0(i!= fielddef->m_Fields.size(),"Field not found in data file");

            for(int i = 0; i < fielddef->m_ElementIDs.size(); ++i)
            {
                int eid = fielddef->m_ElementIDs[i];
                int datalen = (*m_exp)[eid]->GetNcoeffs();
                Vmath::Vcopy(datalen,&fielddata[offset + cnt],1,&m_coeffs[m_coeff_offset[eid]],1);
                cnt += datalen;
            }
        }

        //
        // Virtual functions
        //
        
        const Array<OneD,const boost::shared_ptr<ExpList1D> >
                                        &ExpList::v_GetBndCondExpansions()
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
            static Array<OneD,const boost::shared_ptr<ExpList1D> > result;
            return result;
        }

        boost::shared_ptr<ExpList1D> &ExpList::v_GetTrace()
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
            static boost::shared_ptr<ExpList1D> returnVal;
            return returnVal;
        }

        boost::shared_ptr<LocalToGlobalDGMap> &ExpList::v_GetTraceMap()
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
            static boost::shared_ptr<LocalToGlobalDGMap> result;
            return result;
        }

        void ExpList::v_AddTraceIntegral(
                                const Array<OneD, const NekDouble> &Fx,
                                const Array<OneD, const NekDouble> &Fy,
                                      Array<OneD, NekDouble> &outarray)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_AddTraceIntegral(
                                const Array<OneD, const NekDouble> &Fn,
                                      Array<OneD, NekDouble> &outarray)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_AddTraceBiIntegral(
                                const Array<OneD, const NekDouble> &Fwd,
                                const Array<OneD, const NekDouble> &Bwd,
                                      Array<OneD, NekDouble> &outarray)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_GetFwdBwdTracePhys(Array<OneD,NekDouble> &Fwd,
                                           Array<OneD,NekDouble> &Bwd)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_GetFwdBwdTracePhys(
                                const Array<OneD,const NekDouble>  &field,
                                      Array<OneD,NekDouble> &Fwd,
                                      Array<OneD,NekDouble> &Bwd)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_ExtractTracePhys(Array<OneD,NekDouble> &outarray)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_ExtractTracePhys(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD,NekDouble> &outarray)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_MultiplyByInvMassMatrix(
                                const Array<OneD,const NekDouble> &inarray,
                                      Array<OneD,      NekDouble> &outarray,
                                bool  UseContCoeffs)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_HelmSolve(
                const Array<OneD, const NekDouble> &inarray,
                      Array<OneD,       NekDouble> &outarray,
                      NekDouble lambda,
                const Array<OneD, const NekDouble> &varLambda,
                const Array<OneD, const Array<OneD, NekDouble> > &varCoeff)
        {
            ASSERTL0(false, "HelmSolve not implemented.");
            // For ContFieldX classes, -> ContFieldX::v_HelmSolveCG
            // For DisContFieldX classes, -> DisContFieldX::v_HelmSolveDG
        }
        
        void ExpList::v_HelmSolveCG(
                const Array<OneD, const NekDouble> &inarray,
                      Array<OneD,       NekDouble> &outarray,
                      NekDouble lambda,
                const Array<OneD, const NekDouble> &varLambda,
                const Array<OneD, const Array<OneD, NekDouble> > &varCoeff,
                      bool UseContCoeffs,
                const Array<OneD, const NekDouble> &dirForcing)
        {
            ASSERTL0(false, "HelmSolveCG not implemented.");
            // Only implemented in ContFieldX classes
        }

        void ExpList::v_HelmSolveDG(
                const Array<OneD, const NekDouble> &inarray,
                      Array<OneD,       NekDouble> &outarray,
                      NekDouble lambda,
                const Array<OneD, const NekDouble> &varLambda,
                const Array<OneD, const Array<OneD, NekDouble> > &varCoeff,
                      NekDouble tau)
        {
            ASSERTL0(false, "HelmSolveDG not implemented.");
            // Only implemented in DisContFieldX classes
        }            

        void ExpList::v_HelmSolve(const Array<OneD, const NekDouble> &inarray,
                                  Array<OneD,       NekDouble> &outarray,
                                  const Array<OneD, const Array<OneD, NekDouble> > &varCoeff,
                                  const Array<OneD, NekDouble> &lambda,
                                  NekDouble tau)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        // wrapper functions about virtual functions
        Array<OneD, NekDouble> &ExpList::v_UpdateContCoeffs()
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
            return NullNekDouble1DArray;
        }


        const Array<OneD, const NekDouble> &ExpList::v_GetContCoeffs()  const
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
            return NullNekDouble1DArray;
        }

        void ExpList::v_LocalToGlobal(void)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_GlobalToLocal(void)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }


        void ExpList::v_BwdTrans(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD,       NekDouble> &outarray,
                                bool  UseContCoeffs)
        {
            BwdTrans_IterPerExp(inarray,outarray);
        }

        void ExpList::v_FwdTrans(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD,       NekDouble> &outarray,
                                bool  UseContCoeffs)
        {
            FwdTrans_IterPerExp(inarray,outarray);
        }

        void ExpList::v_IProductWRTBase(
                                const Array<OneD, const NekDouble> &inarray,
                                      Array<OneD,       NekDouble> &outarray,
                                bool  UseContCoeffs)
        {
            IProductWRTBase_IterPerExp(inarray,outarray);
        }

        void ExpList::v_GeneralMatrixOp(
                                const GlobalMatrixKey             &gkey,
                                const Array<OneD,const NekDouble> &inarray,
                                      Array<OneD,      NekDouble> &outarray,
                                bool  UseContCoeffs)
        {
            GeneralMatrixOp_IterPerExp(gkey,inarray,outarray);
        }

        void ExpList::v_SetUpPhysNormals(
                                const StdRegions::StdExpansionVector &locexp)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        void ExpList::v_GetBoundaryToElmtMap(Array<OneD, int> &ElmtID,
                                            Array<OneD,int> &EdgeID)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

        const Array<OneD,const SpatialDomains::BoundaryConditionShPtr>
                                            &ExpList::v_GetBndConditions()
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
            static Array<OneD,const SpatialDomains::BoundaryConditionShPtr>
                                                                        result;
            return result;
        }

        void ExpList::v_EvaluateBoundaryConditions(const NekDouble time)
        {
            ASSERTL0(false,
                     "This method is not defined or valid for this class type");
        }

    } //end of namespace
} //end of namespace

