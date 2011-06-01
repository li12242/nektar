///////////////////////////////////////////////////////////////////////////////
//
// File TetExp.cpp
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
// Description:
//
///////////////////////////////////////////////////////////////////////////////
#include <LocalRegions/LocalRegions.h>
#include <LocalRegions/TetExp.h>

namespace Nektar
{
    namespace LocalRegions
    {

        TetExp::TetExp( const LibUtilities::BasisKey &Ba,
                        const LibUtilities::BasisKey &Bb,
                        const LibUtilities::BasisKey &Bc,
                        const SpatialDomains::TetGeomSharedPtr &geom
                        ):
            StdRegions::StdTetExp(Ba,Bb,Bc),
            m_geom(geom),
            m_metricinfo(m_geom->GetGeomFactors(m_base)),
            m_matrixManager(std::string("TetExpMatrix")),
            m_staticCondMatrixManager(std::string("TetExpStaticCondMatrix"))
        {
            for(int i = 0; i < StdRegions::SIZE_MatrixType; ++i)
            {
                m_matrixManager.RegisterCreator(MatrixKey((StdRegions::MatrixType) i,
                                                          StdRegions::eNoExpansionType,*this),
                                                boost::bind(&TetExp::CreateMatrix, this, _1));
                m_staticCondMatrixManager.RegisterCreator(MatrixKey((StdRegions::MatrixType) i,
                                                                    StdRegions::eNoExpansionType,*this),
                                                          boost::bind(&TetExp::CreateStaticCondMatrix, this, _1));
            }
        }


        TetExp::TetExp(const TetExp &T):
            StdRegions::StdTetExp(T),
            m_geom(T.m_geom),
            m_metricinfo(T.m_metricinfo),
            m_matrixManager(std::string("TetExpMatrix")),
            m_staticCondMatrixManager(std::string("TetExpStaticCondMatrix"))
        {
        }


        TetExp::~TetExp()
        {
        }


        /**
         * \f$ \begin{array}{rcl} I_{pqr} = (\phi_{pqr}, u)_{\delta}
         *   & = & \sum_{i=0}^{nq_0} \sum_{j=0}^{nq_1} \sum_{k=0}^{nq_2}
         *     \psi_{p}^{a} (\eta_{1i}) \psi_{pq}^{b} (\eta_{2j}) \psi_{pqr}^{c}
         *     (\eta_{3k}) w_i w_j w_k u(\eta_{1,i} \eta_{2,j} \eta_{3,k})
         * J_{i,j,k}\\ & = & \sum_{i=0}^{nq_0} \psi_p^a(\eta_{1,i})
         *   \sum_{j=0}^{nq_1} \psi_{pq}^b(\eta_{2,j}) \sum_{k=0}^{nq_2}
         *   \psi_{pqr}^c u(\eta_{1i},\eta_{2j},\eta_{3k}) J_{i,j,k}
         * \end{array} \f$ \n
         * where
         * \f$ \phi_{pqr} (\xi_1 , \xi_2 , \xi_3)
         *   = \psi_p^a (\eta_1) \psi_{pq}^b (\eta_2) \psi_{pqr}^c (\eta_3) \f$
         * which can be implemented as \n
         * \f$f_{pqr} (\xi_{3k})
         *   = \sum_{k=0}^{nq_3} \psi_{pqr}^c u(\eta_{1i},\eta_{2j},\eta_{3k})
         * J_{i,j,k} = {\bf B_3 U}   \f$ \n
         * \f$ g_{pq} (\xi_{3k})
         *   = \sum_{j=0}^{nq_1} \psi_{pq}^b (\xi_{2j}) f_{pqr} (\xi_{3k})
         *   = {\bf B_2 F}  \f$ \n
         * \f$ (\phi_{pqr}, u)_{\delta}
         *   = \sum_{k=0}^{nq_0} \psi_{p}^a (\xi_{3k}) g_{pq} (\xi_{3k})
         *   = {\bf B_1 G} \f$
         */
        void TetExp::v_IProductWRTBase(
                            const Array<OneD, const NekDouble>& inarray,
                            Array<OneD, NekDouble> & outarray)
        {
            int    nquad0 = m_base[0]->GetNumPoints();
            int    nquad1 = m_base[1]->GetNumPoints();
            int    nquad2 = m_base[2]->GetNumPoints();
            Array<OneD, const NekDouble> jac = m_metricinfo->GetJac();
            Array<OneD,NekDouble> tmp(nquad0*nquad1*nquad2);

            // multiply inarray with Jacobian
            if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
            {
                Vmath::Vmul(nquad0*nquad1*nquad2,&jac[0],1,
                            (NekDouble*)&inarray[0],1,&tmp[0],1);
            }
            else
            {
                Vmath::Smul(nquad0*nquad1*nquad2,jac[0],
                            (NekDouble*)&inarray[0],1,&tmp[0],1);
            }

            StdTetExp::v_IProductWRTBase(tmp,outarray);
        }

        /// @todo add functionality for IsUsingQuadMetrics
        void TetExp::MultiplyByQuadratureMetric(
                    const Array<OneD, const NekDouble>& inarray,
                          Array<OneD, NekDouble> &outarray)
        {
            int i, j;

            int  nquad0 = m_base[0]->GetNumPoints();
            int  nquad1 = m_base[1]->GetNumPoints();
            int  nquad2 = m_base[2]->GetNumPoints();
            int  nqtot  = nquad0*nquad1*nquad2;

            const Array<OneD, const NekDouble>& jac = m_metricinfo->GetJac();
            const Array<OneD, const NekDouble>& w0 = m_base[0]->GetW();
            const Array<OneD, const NekDouble>& w1 = m_base[1]->GetW();
            const Array<OneD, const NekDouble>& w2 = m_base[2]->GetW();

            const Array<OneD, const NekDouble>& z1 = m_base[1]->GetZ();
            const Array<OneD, const NekDouble>& z2 = m_base[2]->GetZ();

            if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
            {
                Vmath::Vmul(nqtot, jac, 1, inarray, 1, outarray, 1);
            }
            else
            {
                Vmath::Smul(nqtot, jac[0], inarray, 1, outarray, 1);
            }

            // multiply by integration constants
            for(i = 0; i < nquad1*nquad2; ++i)
            {
                Vmath::Vmul(nquad0, outarray.get()+i*nquad0,1,
                            w0.get(),1, outarray.get()+i*nquad0,1);
            }

            switch(m_base[1]->GetPointsType())
            {
            // Legendre inner product.
            case LibUtilities::eGaussLobattoLegendre:

                for(j = 0; j < nquad2; ++j)
                {
                    for(i = 0; i < nquad1; ++i)
                    {
                        Blas::Dscal(nquad0,
                                    0.5*(1-z1[i])*w1[i],
                                    &outarray[0]+i*nquad0 + j*nquad0*nquad1,
                                    1 );
                    }
                }
                break;

            // (1,0) Jacobi Inner product.
            case LibUtilities::eGaussRadauMAlpha1Beta0:
                for(i = 0; i < nquad1*nquad2; ++i)
                {
                    Vmath::Smul(nquad0, 0.5*w1[i%nquad2], outarray.get()+i*nquad0, 1,
                                outarray.get()+i*nquad0, 1);
                }
                break;
            }

            switch(m_base[2]->GetPointsType())
            {
            // Legendre inner product.
            case LibUtilities::eGaussLobattoLegendre:
                for(i = 0; i < nquad2; ++i)
                {
                    Blas::Dscal(nquad0*nquad1,0.25*(1-z2[i])*(1-z2[i])*w2[i],
                                &outarray[0]+i*nquad0*nquad1,1);
                }
                break;
            // (2,0) Jacobi inner product.
            case LibUtilities::eGaussRadauMAlpha2Beta0:
                for(i = 0; i < nquad2; ++i)
                {
                    Vmath::Smul(nquad0*nquad1, 0.25*w2[i], outarray.get()+i*nquad0*nquad1, 1,
                                outarray.get()+i*nquad0*nquad1, 1);
                }
                break;
            }
        }

        /**
         * @param   inarray     Array of physical quadrature points to be
         *                      transformed.
         * @param   outarray    Array of coefficients to update.
         */
        void TetExp::v_FwdTrans( const Array<OneD, const NekDouble> & inarray,
                            Array<OneD,NekDouble> &outarray)
        {
            if((m_base[0]->Collocation())&&(m_base[1]->Collocation())&&(m_base[2]->Collocation()))
            {
                Vmath::Vcopy(GetNcoeffs(),&inarray[0],1,&m_coeffs[0],1);
            }
            else
            {
                IProductWRTBase(inarray,outarray);

                // get Mass matrix inverse
                MatrixKey             masskey(StdRegions::eInvMass,
                                              DetExpansionType(),*this);
                DNekScalMatSharedPtr  matsys = m_matrixManager[masskey];

                // copy inarray in case inarray == outarray
                DNekVec in (m_ncoeffs,outarray);
                DNekVec out(m_ncoeffs,outarray,eWrapper);

                out = (*matsys)*in;
            }
        }


        /**
         * @param   inarray     Definition of function to be returned at
         *                      quadrature point of expansion.
         * @returns \f$\int^1_{-1}\int^1_{-1} \int^1_{-1}
         *   u(\eta_1, \eta_2, \eta_3) J[i,j,k] d \eta_1 d \eta_2 d \eta_3 \f$
         * where \f$inarray[i,j,k] = u(\eta_{1i},\eta_{2j},\eta_{3k})
         * \f$ and \f$ J[i,j,k] \f$ is the Jacobian evaluated at the quadrature
         * point.
         */
        NekDouble TetExp::v_Integral(
                            const Array<OneD, const NekDouble> &inarray)
        {
            int    nquad0 = m_base[0]->GetNumPoints();
            int    nquad1 = m_base[1]->GetNumPoints();
            int    nquad2 = m_base[2]->GetNumPoints();
            Array<OneD, const NekDouble> jac = m_metricinfo->GetJac();
            NekDouble retrunVal;
            Array<OneD,NekDouble> tmp(nquad0*nquad1*nquad2);

            // multiply inarray with Jacobian
            if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
            {
                Vmath::Vmul(nquad0*nquad1*nquad2,&jac[0],1,
                            (NekDouble*)&inarray[0],1, &tmp[0],1);
            }
            else
            {
                Vmath::Smul(nquad0*nquad1*nquad2,(NekDouble) jac[0],
                            (NekDouble*)&inarray[0],1,&tmp[0],1);
            }

            // call StdTetExp version;
            retrunVal = StdTetExp::Integral(tmp);

            return retrunVal;
        }


        void TetExp::GeneralMatrixOp_MatOp(
                            const Array<OneD, const NekDouble> &inarray,
                            Array<OneD,NekDouble> &outarray,
                            const StdRegions::StdMatrixKey &mkey)
        {
            int nConsts = mkey.GetNconstants();
            DNekScalMatSharedPtr   mat;

            switch(nConsts)
            {
            case 0:
                {
                    mat = GetLocMatrix(mkey.GetMatrixType());
                }
                break;
            case 1:
                {
                    mat = GetLocMatrix(mkey.GetMatrixType(),mkey.GetConstant(0));
                }
                break;
            case 2:
                {
                    mat = GetLocMatrix(mkey.GetMatrixType(),mkey.GetConstant(0),mkey.GetConstant(1));
                }
                break;

            default:
                {
                    NEKERROR(ErrorUtil::efatal, "Unknown number of constants");
                }
                break;
            }

            if(inarray.get() == outarray.get())
            {
                Array<OneD,NekDouble> tmp(m_ncoeffs);
                Vmath::Vcopy(m_ncoeffs,inarray.get(),1,tmp.get(),1);

                Blas::Dgemv('N',m_ncoeffs,m_ncoeffs,mat->Scale(),(mat->GetOwnedMatrix())->GetPtr().get(),
                            m_ncoeffs, tmp.get(), 1, 0.0, outarray.get(), 1);
            }
            else
            {
                Blas::Dgemv('N',m_ncoeffs,m_ncoeffs,mat->Scale(),(mat->GetOwnedMatrix())->GetPtr().get(),
                            m_ncoeffs, inarray.get(), 1, 0.0, outarray.get(), 1);
            }
        }


        /**
         * @param   inarray     Input array of values at quadrature points to
         *                      be differentiated.
         * @param   out_d0      Derivative in first coordinate direction.
         * @param   out_d1      Derivative in second coordinate direction.
         * @param   out_d2      Derivative in third coordinate direction.
         */
        void TetExp::v_PhysDeriv(const Array<OneD, const NekDouble> & inarray,
                               Array<OneD,NekDouble> &out_d0,
                               Array<OneD,NekDouble> &out_d1,
                               Array<OneD,NekDouble> &out_d2)
        {
            int    nquad0 = m_base[0]->GetNumPoints();
            int    nquad1 = m_base[1]->GetNumPoints();
            int    nquad2 = m_base[2]->GetNumPoints();
            Array<TwoD, const NekDouble> gmat = m_metricinfo->GetGmat();
            Array<OneD,NekDouble> Diff0 = Array<OneD,NekDouble>(nquad0*nquad1*nquad2);
            Array<OneD,NekDouble> Diff1 = Array<OneD,NekDouble>(nquad0*nquad1*nquad2);
            Array<OneD,NekDouble> Diff2 = Array<OneD,NekDouble>(nquad0*nquad1*nquad2);

            StdTetExp::v_PhysDeriv(inarray, Diff0, Diff1, Diff2);

            if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
            {
                if(out_d0.num_elements())
                {
                    Vmath::Vmul  (nquad0*nquad1*nquad2,&gmat[0][0],1,&Diff0[0],1, &out_d0[0], 1);
                    Vmath::Vvtvp (nquad0*nquad1*nquad2,&gmat[1][0],1,&Diff1[0],1, &out_d0[0], 1,&out_d0[0],1);
                    Vmath::Vvtvp (nquad0*nquad1*nquad2,&gmat[2][0],1,&Diff2[0],1, &out_d0[0], 1,&out_d0[0],1);
                }

                if(out_d1.num_elements())
                {
                    Vmath::Vmul  (nquad0*nquad1*nquad2,&gmat[3][0],1,&Diff0[0],1, &out_d1[0], 1);
                    Vmath::Vvtvp (nquad0*nquad1*nquad2,&gmat[4][0],1,&Diff1[0],1, &out_d1[0], 1,&out_d1[0],1);
                    Vmath::Vvtvp (nquad0*nquad1*nquad2,&gmat[5][0],1,&Diff2[0],1, &out_d1[0], 1,&out_d1[0],1);
                }

                if(out_d2.num_elements())
                {
                    Vmath::Vmul  (nquad0*nquad1*nquad2,&gmat[6][0],1,&Diff0[0],1, &out_d2[0], 1);
                    Vmath::Vvtvp (nquad0*nquad1*nquad2,&gmat[7][0],1,&Diff1[0],1, &out_d2[0], 1, &out_d2[0],1);
                    Vmath::Vvtvp (nquad0*nquad1*nquad2,&gmat[8][0],1,&Diff2[0],1, &out_d2[0], 1, &out_d2[0],1);
                }
            }
            else // regular geometry
            {
                if(out_d0.num_elements())
                {
                    Vmath::Smul  (nquad0*nquad1*nquad2,gmat[0][0],&Diff0[0],1, &out_d0[0], 1);
                    Blas::Daxpy (nquad0*nquad1*nquad2,gmat[1][0],&Diff1[0],1, &out_d0[0], 1);
                    Blas::Daxpy (nquad0*nquad1*nquad2,gmat[2][0],&Diff2[0],1, &out_d0[0], 1);
                }

                if(out_d1.num_elements())
                {
                    Vmath::Smul  (nquad0*nquad1*nquad2,gmat[3][0],&Diff0[0],1, &out_d1[0], 1);
                    Blas::Daxpy (nquad0*nquad1*nquad2,gmat[4][0],&Diff1[0],1, &out_d1[0], 1);
                    Blas::Daxpy (nquad0*nquad1*nquad2,gmat[5][0],&Diff2[0],1, &out_d1[0], 1);
                }

                if(out_d2.num_elements())
                {
                    Vmath::Smul  (nquad0*nquad1*nquad2,gmat[6][0],&Diff0[0],1, &out_d2[0], 1);
                    Blas::Daxpy (nquad0*nquad1*nquad2,gmat[7][0],&Diff1[0],1, &out_d2[0], 1);
                    Blas::Daxpy (nquad0*nquad1*nquad2,gmat[8][0],&Diff2[0],1, &out_d2[0], 1);
                }
            }
        }


        /**
         * @param   coord       Physical space coordinate
         * @returns Evaluation of expansion at given coordinate.
         */
        NekDouble TetExp::v_PhysEvaluate(
                            const Array<OneD, const NekDouble> &coord)
        {
            ASSERTL0(m_geom,"m_geom not defined");

            Array<OneD,NekDouble> Lcoord = Array<OneD,NekDouble>(3);

            // Get the local (eta) coordinates of the point
            m_geom->GetLocCoords(coord,Lcoord);

            // Evaluate point in local (eta) coordinates.
            return StdExpansion3D::v_PhysEvaluate(Lcoord);
        }


        void TetExp::v_GetCoords(Array<OneD,NekDouble> &coords_0,
                               Array<OneD,NekDouble> &coords_1,
                               Array<OneD,NekDouble> &coords_2)
        {
            LibUtilities::BasisSharedPtr CBasis0;
            LibUtilities::BasisSharedPtr CBasis1;
            LibUtilities::BasisSharedPtr CBasis2;
            Array<OneD,NekDouble>  x;

            ASSERTL0(m_geom, "m_geom not define");

            // get physical points defined in Geom
            m_geom->FillGeom();  //TODO: implement

            switch(m_geom->GetCoordim())
            {
            case 3:
                ASSERTL0(coords_2.num_elements(), "output coords_2 is not defined");
                CBasis0 = m_geom->GetBasis(2,0);
                CBasis1 = m_geom->GetBasis(2,1);
                CBasis2 = m_geom->GetBasis(2,2);

                if((m_base[0]->GetBasisKey().SamePoints(CBasis0->GetBasisKey()))&&
                   (m_base[1]->GetBasisKey().SamePoints(CBasis1->GetBasisKey()))&&
                   (m_base[2]->GetBasisKey().SamePoints(CBasis2->GetBasisKey())))
                {
                    x = m_geom->UpdatePhys(2);
                    Blas::Dcopy(m_base[0]->GetNumPoints()*m_base[1]->GetNumPoints()*m_base[2]->GetNumPoints(),
                                x, 1, coords_2, 1);
                }
                else // Interpolate to Expansion point distribution
                {
                    LibUtilities::Interp3D(CBasis0->GetPointsKey(), CBasis1->GetPointsKey(), CBasis2->GetPointsKey(), &(m_geom->UpdatePhys(2))[0],
                             m_base[0]->GetPointsKey(), m_base[1]->GetPointsKey(), m_base[2]->GetPointsKey(), &coords_2[0]);
                }
            case 2:
                ASSERTL0(coords_1.num_elements(), "output coords_1 is not defined");

                CBasis0 = m_geom->GetBasis(1,0);
                CBasis1 = m_geom->GetBasis(1,1);
                CBasis2 = m_geom->GetBasis(1,2);

                if((m_base[0]->GetBasisKey().SamePoints(CBasis0->GetBasisKey()))&&
                   (m_base[1]->GetBasisKey().SamePoints(CBasis1->GetBasisKey()))&&
                   (m_base[2]->GetBasisKey().SamePoints(CBasis2->GetBasisKey())))
                {
                    x = m_geom->UpdatePhys(1);
                    Blas::Dcopy(m_base[0]->GetNumPoints()*m_base[1]->GetNumPoints()*m_base[2]->GetNumPoints(),
                                x, 1, coords_1, 1);
                }
                else // Interpolate to Expansion point distribution
                {
                    LibUtilities::Interp3D(CBasis0->GetPointsKey(), CBasis1->GetPointsKey(), CBasis2->GetPointsKey(), &(m_geom->UpdatePhys(1))[0],
                             m_base[0]->GetPointsKey(), m_base[1]->GetPointsKey(), m_base[2]->GetPointsKey(), &coords_1[0]);
                }
            case 1:
                ASSERTL0(coords_0.num_elements(), "output coords_0 is not defined");

                CBasis0 = m_geom->GetBasis(0,0);
                CBasis1 = m_geom->GetBasis(0,1);
                CBasis2 = m_geom->GetBasis(0,2);

                if((m_base[0]->GetBasisKey().SamePoints(CBasis0->GetBasisKey()))&&
                   (m_base[1]->GetBasisKey().SamePoints(CBasis1->GetBasisKey()))&&
                   (m_base[2]->GetBasisKey().SamePoints(CBasis2->GetBasisKey())))
                {
                    x = m_geom->UpdatePhys(0);
                    Blas::Dcopy(m_base[0]->GetNumPoints()*m_base[1]->GetNumPoints()*m_base[2]->GetNumPoints(),
                                x, 1, coords_0, 1);
                }
                else // Interpolate to Expansion point distribution
                {
                    LibUtilities::Interp3D(CBasis0->GetPointsKey(), CBasis1->GetPointsKey(), CBasis2->GetPointsKey(), &(m_geom->UpdatePhys(0))[0],
                             m_base[0]->GetPointsKey(),m_base[1]->GetPointsKey(),m_base[2]->GetPointsKey(),&coords_0[0]);
                }
                break;
            default:
                ASSERTL0(false,"Number of dimensions are greater than 3");
                break;
            }
        }


        // get the coordinates "coords" at the local coordinates "Lcoords"
        void TetExp::v_GetCoord(const Array<OneD, const NekDouble> &Lcoords, Array<OneD,NekDouble> &coords)
        {
            int  i;

            ASSERTL1(Lcoords[0] <= -1.0 && Lcoords[0] >= 1.0 &&
                     Lcoords[1] <= -1.0 && Lcoords[1] >= 1.0 &&
                     Lcoords[2] <= -1.0 && Lcoords[2] >= 1.0,
                     "Local coordinates are not in region [-1,1]");

            // m_geom->FillGeom(); // TODO: implement FillGeom()

            for(i = 0; i < m_geom->GetCoordDim(); ++i)
            {
                coords[i] = m_geom->GetCoord(i,Lcoords);
            }
        }


        void TetExp::v_WriteToFile(std::ofstream &outfile, OutputFormat format, const bool dumpVar, std::string var)
        {
            int i,j,k;
            int nquad0 = m_base[0]->GetNumPoints();
            int nquad1 = m_base[1]->GetNumPoints();
            int nquad2 = m_base[2]->GetNumPoints();
            Array<OneD,NekDouble> coords[3];

            ASSERTL0(m_geom,"m_geom not defined");

            int     coordim  = m_geom->GetCoordim();

            coords[0] = Array<OneD,NekDouble>(nquad0*nquad1*nquad2);
            coords[1] = Array<OneD,NekDouble>(nquad0*nquad1*nquad2);
            coords[2] = Array<OneD,NekDouble>(nquad0*nquad1*nquad2);

            GetCoords(coords[0],coords[1],coords[2]);

            if(format==eTecplot)
            {
                if(dumpVar)
                {
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

                outfile << "Zone, I=" << nquad0 << ", J=" << nquad1 << ", K=" << nquad2 << ", F=Point" << std::endl;

                for(i = 0; i < nquad0*nquad1*nquad2; ++i)
                {
                    for(j = 0; j < coordim; ++j)
                    {
                        for(k=0; k < coordim; ++k)
                        {
                            outfile << coords[k][j] << " ";
                        }
                        outfile << m_phys[j] << std::endl;
                    }
                    outfile << m_phys[i] << std::endl;
                }
            }
            else if (format==eGnuplot)
            {
                for(int k = 0; k < nquad2; ++k)
                {
                    for(int j = 0; j < nquad1; ++j)
                    {
                        for(int i = 0; i < nquad0; ++i)
                        {
                            int n = (k*nquad1 + j)*nquad0 + i;
                            outfile <<  coords[0][n] <<  " " << coords[1][n] << " "
                                    << coords[2][n] << " "
                                    << m_phys[i + nquad0*(j + nquad1*k)] << endl;
                        }
                        outfile << endl;
                    }
                    outfile << endl;
                }
            }
            else
            {
                ASSERTL0(false, "Output routine not implemented for requested type of output");
            }
        }


        const SpatialDomains::GeometrySharedPtr TetExp::v_GetGeom() const
        {
            return m_geom;
        }


        const SpatialDomains::Geometry3DSharedPtr& TetExp::v_GetGeom3D() const
        {
            return m_geom;
        }


        void TetExp::v_HelmholtzMatrixOp(
                            const Array<OneD, const NekDouble> &inarray,
                            Array<OneD,NekDouble> &outarray,
                            const StdRegions::StdMatrixKey &mkey)
        {
            bool doMatOp = NekOptimize::ElementalOptimization<
                        StdRegions::eTetExp, NekOptimize::eHelmholtzMatrixOp, 3>
                            ::DoMatOp(  m_base[0]->GetNumModes(),
                                        m_base[1]->GetNumModes(),
                                        m_base[2]->GetNumModes());

            if(doMatOp)
            {
                TetExp::GeneralMatrixOp_MatOp(inarray,outarray,mkey);
            }
            else
            {
                TetExp::v_HelmholtzMatrixOp_MatFree(inarray,outarray,mkey);
            }
        }


        /**
         * To construct the Helmholtz operator in a physical tetrahedron
         * requires coordinate transforms from both the collapsed coordinate
         * system to the standard region and from the standard region to the
         * local region. This double application of the chain rule requires the
         * calculation of two sets of geometric factors:
         * @f[ h_{ij} = \frac{\partial \eta_i}{\partial \xi_j} @f]
         * and
         * @f[ g_{ij} = \frac{\partial \xi_i}{\partial x_j} @f]
         *
         * From the definition of the collapsed coordinates, the @f$h_{ij}@f$
         * terms are (Sherwin & Karniadakis, p152)
         * @f[
         *      \mathbf{H} = \left[\begin{array}{ccc}
         *          \frac{4}{(1-\eta_2)(1-\eta_3)} &
         *          \frac{2(1+\eta_1)}{(1-\eta_2)(1-\eta_3)} &
         *          \frac{2(1+\eta_1)}{(1-\eta_2)(1-\eta_3)} \\
         *          0 &
         *          \frac{2}{1-eta_3} &
         *          \frac{1+\eta_2}{1-\eta_3} \\
         *          0 &
         *          0 &
         *          1
         *      \end{array}\right]
         * @f]
         * This maps from the collapsed coordinate system to the standard
         * tetrahedral region. The mapping to the local region is then given
         * by the @f$g_{ij}@f$ computed in the GeomFactors3D class. The
         * cumulative factors for mapping the collapsed coordinate system to
         * the physical region are therefore given by
         * @f$\mathbf{F} = \mathbf{GH^{\top}}@f$, i.e.
         * @f[
         *      f_{ij} = \frac{\partial \eta_i}{\partial x_j}
         *              = \sum_k g_{ik} h_{kj}
         * @f]
         *
         * Finally, the evaluation of the Helmholtz matrix operator requires
         * the summation of these factors as follows. For the case of deformed
         * elements, these coefficients are vectors, whereas for regular
         * elements they are just scalars.
         * @f[
         *      \begin{array}{l}
         *      p_0 = \sum_k f_{1k}^2 \\
         *      p_1 = \sum_k f_{2k}^2 \\
         *      p_2 = \sum_k f_{3k}^2 \\
         *      p_3 = \sum_k f_{1k}f_{2k} \\
         *      p_4 = \sum_k f_{1k}f_{3k} \\
         *      p_5 = \sum_k f_{2k}f_{3k}
         *      \end{array}
         * @f]
         * to give the Helmholtz operator:
         * @f{align}
         *      \mathbf{L^e\hat{u}}
         *          = \mathbf{B^{\top}D_{\eta_1}^{\top}Wp_0D_{\eta_1}B\hat{u}}
         *          + \mathbf{B^{\top}D_{\eta_2}^{\top}Wp_1D_{\eta_2}B\hat{u}}
         *          + \mathbf{B^{\top}D_{\eta_3}^{\top}Wp_2D_{\eta_3}B\hat{u}}\\
         *          + \mathbf{B^{\top}D_{\eta_1}^{\top}Wp_3D_{\eta_2}B\hat{u}}
         *          + \mathbf{B^{\top}D_{\eta_1}^{\top}Wp_4D_{\eta_3}B\hat{u}}
         *          + \mathbf{B^{\top}D_{\eta_2}^{\top}Wp_5D_{\eta_3}B\hat{u}}
         * @f}
         * Therefore, we construct the operator as follows:
         * -# Apply the mass matrix for the @f$\lambda@f$ term
         *    @f$ \mathbf{B^{\top}WB\hat{u}} @f$.
         *    and compute the derivatives @f$ \mathbf{D_{\xi_i}B} @f$.
         * -# Compute the non-trivial @f$ \mathbf{H} @f$ matrix terms.
         * -# Compute the intermediate factors @f$ \mathbf{G} @f$ and
         *    @f$ f_{ij} @f$ and then compute the combined terms @f$ p_i @f$.
         * -# Apply quadrature weights and inner product with respect to the
         *    derivative bases.
         * -# Combine to produce the complete operator.
         */
        void TetExp::v_HelmholtzMatrixOp_MatFree(
                            const Array<OneD, const NekDouble> &inarray,
                            Array<OneD,NekDouble> &outarray,
                            const StdRegions::StdMatrixKey &mkey)
        {
            if(m_metricinfo->IsUsingLaplMetrics())
            {
                ASSERTL0(false,"Finish implementing TetExp Helmholtz for Lapl Metrics");
/*                int       nquad0  = m_base[0]->GetNumPoints();
                int       nquad1  = m_base[1]->GetNumPoints();
                int       nqtot   = nquad0*nquad1;
                int       nmodes0 = m_base[0]->GetNumModes();
                int       nmodes1 = m_base[1]->GetNumModes();
                int       wspsize = max(max(max(nqtot,m_ncoeffs),nquad1*nmodes0),nquad0*nmodes1);
                NekDouble lambda  = mkey.GetConstant(0);

                const Array<OneD, const NekDouble>& base0  = m_base[0]->GetBdata();
                const Array<OneD, const NekDouble>& base1  = m_base[1]->GetBdata();
                const Array<OneD, const NekDouble>& dbase0 = m_base[0]->GetDbdata();
                const Array<OneD, const NekDouble>& dbase1 = m_base[1]->GetDbdata();
                const Array<TwoD, const NekDouble>& metric = m_metricinfo->GetLaplacianMetrics();

                // Allocate temporary storage
                Array<OneD,NekDouble> wsp0(4*wspsize);
                Array<OneD,NekDouble> wsp1(wsp0+wspsize);
                Array<OneD,NekDouble> wsp2(wsp0+2*wspsize);
                Array<OneD,NekDouble> wsp3(wsp0+3*wspsize);

                if(!(m_base[0]->Collocation() && m_base[1]->Collocation()))
                {
                    // MASS MATRIX OPERATION
                    // The following is being calculated:
                    // wsp0     = B   * u_hat = u
                    // wsp1     = W   * wsp0
                    // outarray = B^T * wsp1  = B^T * W * B * u_hat = M * u_hat
                    BwdTrans_SumFacKernel       (base0,base1,inarray,wsp0,    wsp1,true,true);
                    MultiplyByQuadratureMetric  (wsp0,wsp2);
                    IProductWRTBase_SumFacKernel(base0,base1,wsp2,   outarray,wsp1,true,true);

                    // LAPLACIAN MATRIX OPERATION
                    // wsp1 = du_dxi1 = D_xi1 * wsp0 = D_xi1 * u
                    // wsp2 = du_dxi2 = D_xi2 * wsp0 = D_xi2 * u
                    StdExpansion2D::PhysTensorDeriv(wsp0,wsp1,wsp2);
                }
                else
                {
                    // specialised implementation for the classical spectral element method
                    StdExpansion2D::PhysTensorDeriv(inarray,wsp1,wsp2);
                    MultiplyByQuadratureMetric(inarray,outarray);
                }

                // wsp0 = k = g0 * wsp1 + g1 * wsp2 = g0 * du_dxi1 + g1 * du_dxi2
                // wsp2 = l = g1 * wsp1 + g2 * wsp2 = g1 * du_dxi1 + g2 * du_dxi2
                // where g0, g1 and g2 are the metric terms set up in the GeomFactors class
                // especially for this purpose
                if(!m_metricinfo->LaplacianMetricIsZero(1))
                {
                    Vmath::Vvtvvtp(nqtot,&metric[0][0],1,&wsp1[0],1,&metric[1][0],1,&wsp2[0],1,&wsp0[0],1);
                    Vmath::Vvtvvtp(nqtot,&metric[1][0],1,&wsp1[0],1,&metric[2][0],1,&wsp2[0],1,&wsp2[0],1);
                }
                else
                {
                    // special implementation in case g1 = 0 (which should hold for undistorted quads)
                    // wsp0 = k = g0 * wsp1 = g0 * du_dxi1
                    // wsp2 = l = g2 * wsp2 = g2 * du_dxi2
                    Vmath::Vmul(nqtot,&metric[0][0],1,&wsp1[0],1,&wsp0[0],1);
                    Vmath::Vmul(nqtot,&metric[2][0],1,&wsp2[0],1,&wsp2[0],1);
                }

                // wsp1 = m = (D_xi1 * B)^T * k
                // wsp0 = n = (D_xi2 * B)^T * l
                IProductWRTBase_SumFacKernel(dbase0,base1,wsp0,wsp1,wsp3,false,true);
                IProductWRTBase_SumFacKernel(base0,dbase1,wsp2,wsp0,wsp3,true,false);

                // outarray = lambda * outarray + (wsp0 + wsp1)
                //          = (lambda * M + L ) * u_hat
                Vmath::Vstvpp(m_ncoeffs,lambda,&outarray[0],1,&wsp1[0],1,&wsp0[0],1,&outarray[0],1);
*/            }
            else
            {
                int       nquad0  = m_base[0]->GetNumPoints();
                int       nquad1  = m_base[1]->GetNumPoints();
                int       nquad2  = m_base[2]->GetNumPoints();
                int       nqtot   = nquad0*nquad1*nquad2;
                int       nmodes0 = m_base[0]->GetNumModes();
                int       nmodes1 = m_base[1]->GetNumModes();
                int       nmodes2 = m_base[2]->GetNumModes();
                int       i,j;
                int wspsize = max(nquad0*nmodes2*(nmodes1+nquad1),
                                        nquad0*nquad1*(nquad2+nmodes0)
                                            + nmodes0*nmodes1*nquad2);

                NekDouble lambda  = mkey.GetConstant(0);

                const Array<OneD, const NekDouble>& base0  = m_base[0]->GetBdata();
                const Array<OneD, const NekDouble>& base1  = m_base[1]->GetBdata();
                const Array<OneD, const NekDouble>& base2  = m_base[2]->GetBdata();
                const Array<OneD, const NekDouble>& dbase0 = m_base[0]->GetDbdata();
                const Array<OneD, const NekDouble>& dbase1 = m_base[1]->GetDbdata();
                const Array<OneD, const NekDouble>& dbase2 = m_base[2]->GetDbdata();

                // Allocate temporary storage
                Array<OneD,NekDouble> alloc(14*nqtot,0.0);
                Array<OneD,NekDouble> wsp0(alloc);        // After BwdTrans
                Array<OneD,NekDouble> wsp1(alloc+  nqtot);// TensorDeriv 1
                Array<OneD,NekDouble> wsp2(alloc+2*nqtot);// TensorDeriv 2
                Array<OneD,NekDouble> wsp3(alloc+3*nqtot);// TensorDeriv 3
                Array<OneD,NekDouble> g0(alloc+4*nqtot);// g0
                Array<OneD,NekDouble> g1(alloc+5*nqtot);// g1
                Array<OneD,NekDouble> g2(alloc+6*nqtot);// g2
                Array<OneD,NekDouble> g3(alloc+7*nqtot);// g3
                Array<OneD,NekDouble> g4(alloc+8*nqtot);// g4
                Array<OneD,NekDouble> g5(alloc+9*nqtot);// g5
                Array<OneD,NekDouble> h0(alloc+10*nqtot);// h0
                Array<OneD,NekDouble> h1(alloc+11*nqtot);// h1
                Array<OneD,NekDouble> h2(alloc+12*nqtot);// h2
                Array<OneD,NekDouble> h3(alloc+13*nqtot);// h3

                // Reuse some of the storage as workspace
                Array<OneD,NekDouble> wsp4(alloc+5*nqtot);// wsp4 == g1
                Array<OneD,NekDouble> wsp5(alloc+6*nqtot);// wsp5 == g2
                Array<OneD,NekDouble> wsp6(alloc+9*nqtot);// wsp6 == g5
                Array<OneD,NekDouble> wsp7(alloc+10*nqtot);// wsp7 == h0
                Array<OneD,NekDouble> wsp8(alloc+11*nqtot);// wsp8 == h1
                Array<OneD,NekDouble> wsp9(alloc+12*nqtot);// wsp9 == h2

                Array<OneD,NekDouble> wsp(wspsize,0.0);

                // Step 1
                if(!(m_base[0]->Collocation() && m_base[1]->Collocation()
                        && m_base[2]->Collocation()))
                {
                    // MASS MATRIX OPERATION
                    // The following is being calculated:
                    // wsp0     = B   * u_hat = u
                    // wsp1     = W   * wsp0
                    // outarray = B^T * wsp1  = B^T * W * B * u_hat = M * u_hat
                    BwdTrans_SumFacKernel       (base0,base1,base2,inarray,wsp0,wsp,true,true,true);
                    MultiplyByQuadratureMetric  (wsp0,wsp2);
                    IProductWRTBase_SumFacKernel(base0,base1,base2,wsp2,outarray,wsp,true,true,true);

                    // LAPLACIAN MATRIX OPERATION
                    // wsp1 = du_dxi1 = D_xi1 * wsp0 = D_xi1 * u
                    // wsp2 = du_dxi2 = D_xi2 * wsp0 = D_xi2 * u
                    //StdTetExp::v_PhysDeriv(wsp0,wsp1,wsp2,wsp3);
                    StdExpansion3D::PhysTensorDeriv(wsp0,wsp1,wsp2,wsp3);
                }
                else
                {
                    // specialised implementation for the classical spectral element method
                    MultiplyByQuadratureMetric(inarray,outarray);

                    //StdTetExp::v_PhysDeriv(inarray,wsp1,wsp2,wsp3);
                    StdExpansion3D::PhysTensorDeriv(inarray,wsp1,wsp2,wsp3);
                }

                const Array<TwoD, const NekDouble>& gmat = m_metricinfo->GetGmat();
                const Array<OneD, const NekDouble>& z0 = m_base[0]->GetZ();
                const Array<OneD, const NekDouble>& z1 = m_base[1]->GetZ();
                const Array<OneD, const NekDouble>& z2 = m_base[2]->GetZ();

                // Step 2. Calculate the metric terms of the collapsed
                // coordinate transformation (Spencer's book P152)
                for(j = 0; j < nquad2; ++j)
                {
                    for(i = 0; i < nquad1; ++i)
                    {
                        Vmath::Fill(nquad0, 4.0/(1.0-z1[i])/(1.0-z2[j]), &h0[0]+i*nquad0 + j*nquad0*nquad1,1);
                        Vmath::Fill(nquad0, 2.0/(1.0-z1[i])/(1.0-z2[j]), &h1[0]+i*nquad0 + j*nquad0*nquad1,1);
                        Vmath::Fill(nquad0, 2.0/(1.0-z2[j]),             &h2[0]+i*nquad0 + j*nquad0*nquad1,1);
                        Vmath::Fill(nquad0, (1.0+z1[i])/(1.0-z2[j]),     &h3[0]+i*nquad0 + j*nquad0*nquad1,1);
                    }
                }
                for(i = 0; i < nquad0; i++)
                {
                    Blas::Dscal(nquad1*nquad2, 1+z0[i], &h1[0]+i, nquad0);
                }

                // Step 3. Construct combined metric terms for physical space to
                // collapsed coordinate system.
                // Order of construction optimised to minimise temporary storage
                if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                {
                    // wsp4
                    Vmath::Vadd(nqtot, &gmat[1][0], 1, &gmat[2][0], 1, &wsp4[0], 1);
                    Vmath::Vvtvvtp(nqtot, &gmat[0][0], 1, &h0[0], 1, &wsp4[0], 1, &h1[0], 1, &wsp4[0], 1);
                    // wsp5
                    Vmath::Vadd(nqtot, &gmat[4][0], 1, &gmat[5][0], 1, &wsp5[0], 1);
                    Vmath::Vvtvvtp(nqtot, &gmat[3][0], 1, &h0[0], 1, &wsp5[0], 1, &h1[0], 1, &wsp5[0], 1);
                    // wsp6
                    Vmath::Vadd(nqtot, &gmat[7][0], 1, &gmat[8][0], 1, &wsp6[0], 1);
                    Vmath::Vvtvvtp(nqtot, &gmat[6][0], 1, &h0[0], 1, &wsp6[0], 1, &h1[0], 1, &wsp6[0], 1);

                    // g0
                    Vmath::Vvtvvtp(nqtot, &wsp4[0], 1, &wsp4[0], 1, &wsp5[0], 1, &wsp5[0], 1, &g0[0], 1);
                    Vmath::Vvtvp  (nqtot, &wsp6[0], 1, &wsp6[0], 1, &g0[0],   1, &g0[0],   1);

                    // g4
                    Vmath::Vvtvvtp(nqtot, &gmat[2][0], 1, &wsp4[0], 1, &gmat[5][0], 1, &wsp5[0], 1, &g4[0], 1);
                    Vmath::Vvtvp  (nqtot, &gmat[8][0], 1, &wsp6[0], 1, &g4[0], 1, &g4[0], 1);

                    // overwrite h0, h1, h2
                    // wsp7 (h2f1 + h3f2)
                    Vmath::Vvtvvtp(nqtot, &gmat[1][0], 1, &h2[0], 1, &gmat[2][0], 1, &h3[0], 1, &wsp7[0], 1);
                    // wsp8 (h2f4 + h3f5)
                    Vmath::Vvtvvtp(nqtot, &gmat[4][0], 1, &h2[0], 1, &gmat[5][0], 1, &h3[0], 1, &wsp8[0], 1);
                    // wsp9 (h2f7 + h3f8)
                    Vmath::Vvtvvtp(nqtot, &gmat[7][0], 1, &h2[0], 1, &gmat[8][0], 1, &h3[0], 1, &wsp9[0], 1);

                    // g3
                    Vmath::Vvtvvtp(nqtot, &wsp4[0], 1, &wsp7[0], 1, &wsp5[0], 1, &wsp8[0], 1, &g3[0], 1);
                    Vmath::Vvtvp  (nqtot, &wsp6[0], 1, &wsp9[0], 1, &g3[0],   1, &g3[0],   1);

                    // overwrite wsp4, wsp5, wsp6
                    // g1
                    Vmath::Vvtvvtp(nqtot, &wsp7[0], 1, &wsp7[0], 1, &wsp8[0], 1, &wsp8[0], 1, &g1[0], 1);
                    Vmath::Vvtvp  (nqtot, &wsp9[0], 1, &wsp9[0], 1, &g1[0],   1, &g1[0],   1);

                    // g5
                    Vmath::Vvtvvtp(nqtot, &gmat[2][0], 1, &wsp7[0], 1, &gmat[5][0], 1, &wsp8[0], 1, &g5[0], 1);
                    Vmath::Vvtvp  (nqtot, &gmat[8][0], 1, &wsp9[0], 1, &g5[0], 1, &g5[0], 1);

                    // g2
                    Vmath::Vvtvvtp(nqtot, &gmat[2][0], 1, &gmat[2][0], 1, &gmat[5][0], 1, &gmat[5][0], 1, &g2[0], 1);
                    Vmath::Vvtvp  (nqtot, &gmat[8][0], 1, &gmat[8][0], 1, &g2[0],      1, &g2[0],      1);
                }
                else
                {
                    // wsp4
                    Vmath::Svtsvtp(nqtot, gmat[0][0], &h0[0], 1, gmat[1][0] + gmat[2][0], &h1[0], 1, &wsp4[0], 1);
                    // wsp5
                    Vmath::Svtsvtp(nqtot, gmat[3][0], &h0[0], 1, gmat[4][0] + gmat[5][0], &h1[0], 1, &wsp5[0], 1);
                    // wsp6
                    Vmath::Svtsvtp(nqtot, gmat[6][0], &h0[0], 1, gmat[7][0] + gmat[8][0], &h1[0], 1, &wsp6[0], 1);

                    // g0
                    Vmath::Vvtvvtp(nqtot, &wsp4[0], 1, &wsp4[0], 1, &wsp5[0], 1, &wsp5[0], 1, &g0[0], 1);
                    Vmath::Vvtvp  (nqtot, &wsp6[0], 1, &wsp6[0], 1, &g0[0],   1, &g0[0],   1);

                    // g4
                    Vmath::Svtsvtp(nqtot, gmat[2][0], &wsp4[0], 1, gmat[5][0], &wsp5[0], 1, &g4[0], 1);
                    Vmath::Svtvp  (nqtot, gmat[8][0], &wsp6[0], 1, &g4[0], 1, &g4[0], 1);

                    // overwrite h0, h1, h2
                    // wsp7 (h2f1 + h3f2)
                    Vmath::Svtsvtp(nqtot, gmat[1][0], &h2[0], 1, gmat[2][0], &h3[0], 1, &wsp7[0], 1);
                    // wsp8 (h2f4 + h3f5)
                    Vmath::Svtsvtp(nqtot, gmat[4][0], &h2[0], 1, gmat[5][0], &h3[0], 1, &wsp8[0], 1);
                    // wsp9 (h2f7 + h3f8)
                    Vmath::Svtsvtp(nqtot, gmat[7][0], &h2[0], 1, gmat[8][0], &h3[0], 1, &wsp9[0], 1);

                    // g3
                    Vmath::Vvtvvtp(nqtot, &wsp4[0], 1, &wsp7[0], 1, &wsp5[0], 1, &wsp8[0], 1, &g3[0], 1);
                    Vmath::Vvtvp  (nqtot, &wsp6[0], 1, &wsp9[0], 1, &g3[0],   1, &g3[0],   1);

                    // overwrite wsp4, wsp5, wsp6
                    // g1
                    Vmath::Vvtvvtp(nqtot, &wsp7[0], 1, &wsp7[0], 1, &wsp8[0], 1, &wsp8[0], 1, &g1[0], 1);
                    Vmath::Vvtvp  (nqtot, &wsp9[0], 1, &wsp9[0], 1, &g1[0],   1, &g1[0],   1);

                    // g5
                    Vmath::Svtsvtp(nqtot, gmat[2][0], &wsp7[0], 1, gmat[5][0], &wsp8[0], 1, &g5[0], 1);
                    Vmath::Svtvp  (nqtot, gmat[8][0], &wsp9[0], 1, &g5[0], 1, &g5[0], 1);

                    // g2
                    Vmath::Fill(nqtot, gmat[2][0]*gmat[2][0] + gmat[5][0]*gmat[5][0] + gmat[8][0]*gmat[8][0], &g2[0], 1);
                }

                // Compute component derivatives into wsp7, 8, 9
                Vmath::Vvtvvtp(nqtot,&g0[0],1,&wsp1[0],1,&g3[0],1,&wsp2[0],1,&wsp7[0],1);
                Vmath::Vvtvp  (nqtot,&g4[0],1,&wsp3[0],1,&wsp7[0],1,&wsp7[0],1);
                Vmath::Vvtvvtp(nqtot,&g1[0],1,&wsp2[0],1,&g3[0],1,&wsp1[0],1,&wsp8[0],1);
                Vmath::Vvtvp  (nqtot,&g5[0],1,&wsp3[0],1,&wsp8[0],1,&wsp8[0],1);
                Vmath::Vvtvvtp(nqtot,&g2[0],1,&wsp3[0],1,&g4[0],1,&wsp1[0],1,&wsp9[0],1);
                Vmath::Vvtvp  (nqtot,&g5[0],1,&wsp2[0],1,&wsp9[0],1,&wsp9[0],1);

                // Step 4.
                // Multiply by quadrature metric
                MultiplyByQuadratureMetric(wsp7,wsp7);
                MultiplyByQuadratureMetric(wsp8,wsp8);
                MultiplyByQuadratureMetric(wsp9,wsp9);

                IProductWRTBase_SumFacKernel(dbase0,base1,base2,wsp7,wsp1,wsp,false,true,true);
                IProductWRTBase_SumFacKernel(base0,dbase1,base2,wsp8,wsp2,wsp,true,false,true);
                IProductWRTBase_SumFacKernel(base0,base1,dbase2,wsp9,wsp3,wsp,true,true,false);

                // Step 5.
                // outarray = lambda * outarray + (wsp0 + wsp1)
                //          = (lambda * M + L ) * u_hat
                Vmath::Vadd(m_ncoeffs,wsp1.get(),1,wsp2.get(),1,wsp0.get(),1);
                Vmath::Vstvpp(m_ncoeffs,lambda,&outarray[0],1,&wsp0[0],1,&wsp3[0],1,&outarray[0],1);
            }
        }


        void TetExp::v_LaplacianMatrixOp(
                            const Array<OneD, const NekDouble> &inarray,
                            Array<OneD,NekDouble> &outarray,
                            const StdRegions::StdMatrixKey &mkey)
        {
            bool doMatOp = NekOptimize::ElementalOptimization<
                        StdRegions::eTetExp, NekOptimize::eLaplacianMatrixOp, 3>
                            ::DoMatOp(  m_base[0]->GetNumModes(),
                                        m_base[1]->GetNumModes(),
                                        m_base[2]->GetNumModes());

            if(doMatOp)
            {
                TetExp::GeneralMatrixOp_MatOp(inarray,outarray,mkey);
            }
            else
            {
                TetExp::v_LaplacianMatrixOp_MatFree(inarray,outarray,mkey);
            }

        }

        void TetExp::v_LaplacianMatrixOp(const int k1, const int k2,
                            const Array<OneD, const NekDouble> &inarray,
                            Array<OneD,NekDouble> &outarray,
                            const StdRegions::StdMatrixKey &mkey)
        {
            bool doMatOp = NekOptimize::ElementalOptimization<
                    StdRegions::eTetExp, NekOptimize::eLaplacianMatrixIJOp, 3>
                            ::DoMatOp(  m_base[0]->GetNumModes(),
                                        m_base[1]->GetNumModes(),
                                        m_base[2]->GetNumModes());

            if(doMatOp)
            {
                TetExp::GeneralMatrixOp_MatOp(inarray,outarray,mkey);
            }
            else
            {
                StdExpansion::LaplacianMatrixOp_MatFree(k1,k2,inarray,outarray,
                                                        mkey);
            }
        }

        void TetExp::v_LaplacianMatrixOp_MatFree(
                            const Array<OneD, const NekDouble> &inarray,
                            Array<OneD,NekDouble> &outarray,
                            const StdRegions::StdMatrixKey &mkey)
        {
            if(mkey.GetNvariableLaplacianCoefficients() == 0)
            {
                // This implementation is only valid when there are no coefficients
                // associated to the Laplacian operator
                if(m_metricinfo->IsUsingLaplMetrics())
                {
                    ASSERTL0(false,"Finish implementing HexExp for Lap metrics");
                    // Get this from HexExp
                }
                else
                {
                    int       nquad0  = m_base[0]->GetNumPoints();
                    int       nquad1  = m_base[1]->GetNumPoints();
                    int       nquad2  = m_base[2]->GetNumPoints();
                    int       nqtot   = nquad0*nquad1*nquad2;
                    int       nmodes0 = m_base[0]->GetNumModes();
                    int       nmodes1 = m_base[1]->GetNumModes();
                    int       nmodes2 = m_base[2]->GetNumModes();

                    int wspsize = max(nquad0*nmodes2*(nmodes1+nquad1),
                                        nquad0*nquad1*(nquad2+nmodes0)
                                            + nmodes0*nmodes1*nquad2);

                    const Array<OneD, const NekDouble>& base0  = m_base[0]->GetBdata();
                    const Array<OneD, const NekDouble>& base1  = m_base[1]->GetBdata();
                    const Array<OneD, const NekDouble>& base2  = m_base[2]->GetBdata();
                    const Array<OneD, const NekDouble>& dbase0 = m_base[0]->GetDbdata();
                    const Array<OneD, const NekDouble>& dbase1 = m_base[1]->GetDbdata();
                    const Array<OneD, const NekDouble>& dbase2 = m_base[2]->GetDbdata();

                    // Allocate temporary storage
                    Array<OneD,NekDouble> alloc(10*nqtot,0.0);
                    Array<OneD,NekDouble> wsp0(alloc);        // After BwdTrans
                    Array<OneD,NekDouble> wsp1(alloc+  nqtot);// TensorDeriv 1
                    Array<OneD,NekDouble> wsp2(alloc+2*nqtot);// TensorDeriv 2
                    Array<OneD,NekDouble> wsp3(alloc+3*nqtot);// TensorDeriv 3
                    Array<OneD,NekDouble> wsp4(alloc+4*nqtot);// g0
                    Array<OneD,NekDouble> wsp5(alloc+5*nqtot);// g1
                    Array<OneD,NekDouble> wsp6(alloc+6*nqtot);// g2
                    Array<OneD,NekDouble> wsp7(alloc+7*nqtot);// g3
                    Array<OneD,NekDouble> wsp8(alloc+8*nqtot);// g4
                    Array<OneD,NekDouble> wsp9(alloc+9*nqtot);// g5

                    Array<OneD,NekDouble> wsp(wspsize,0.0);

                    if(!(m_base[0]->Collocation() && m_base[1]->Collocation()
                            && m_base[2]->Collocation()))
                    {
                        // LAPLACIAN MATRIX OPERATION
                        // wsp0 = u       = B   * u_hat
                        // wsp1 = du_dxi1 = D_xi1 * wsp0 = D_xi1 * u
                        // wsp2 = du_dxi2 = D_xi2 * wsp0 = D_xi2 * u
                        BwdTrans_SumFacKernel(base0,base1,base2,inarray,wsp0,wsp,true,true,true);
                        StdExpansion3D::PhysTensorDeriv(wsp0,wsp1,wsp2,wsp3);
                    }
                    else
                    {
                        StdExpansion3D::PhysTensorDeriv(inarray,wsp1,wsp2,wsp3);
                    }

                    // wsp0 = k = g0 * wsp1 + g1 * wsp2 = g0 * du_dxi1 + g1 * du_dxi2
                    // wsp2 = l = g1 * wsp1 + g2 * wsp2 = g0 * du_dxi1 + g1 * du_dxi2
                    // where g0, g1 and g2 are the metric terms set up in the GeomFactors class
                    // especially for this purpose
                    const Array<TwoD, const NekDouble>& gmat = m_metricinfo->GetGmat();

                    if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                    {
                        // Compute geometric factor composites
                        // wsp3 = g0*g0 + g3*g3 + g6*g6
                        Vmath::Vvtvvtp(nqtot,&gmat[0][0],1,&gmat[0][0],1,&gmat[3][0],1,&gmat[3][0],1,&wsp4[0],1);
                        Vmath::Vvtvp  (nqtot,&gmat[6][0],1,&gmat[6][0],1,&wsp4[0],1,&wsp4[0],1);
                        // wsp4 = g1*g1 + g4*g4 + g7*g7;
                        Vmath::Vvtvvtp(nqtot,&gmat[1][0],1,&gmat[1][0],1,&gmat[4][0],1,&gmat[4][0],1,&wsp5[0],1);
                        Vmath::Vvtvp  (nqtot,&gmat[7][0],1,&gmat[7][0],1,&wsp5[0],1,&wsp5[0],1);
                        // wsp5 = g2*g2 + g5*g5 + g8*g8;
                        Vmath::Vvtvvtp(nqtot,&gmat[2][0],1,&gmat[2][0],1,&gmat[5][0],1,&gmat[5][0],1,&wsp6[0],1);
                        Vmath::Vvtvp  (nqtot,&gmat[8][0],1,&gmat[8][0],1,&wsp6[0],1,&wsp6[0],1);
                        // wsp6 = g0*g1 + g3*g4 + g6*g7
                        Vmath::Vvtvvtp(nqtot,&gmat[0][0],1,&gmat[1][0],1,&gmat[3][0],1,&gmat[4][0],1,&wsp7[0],1);
                        Vmath::Vvtvp  (nqtot,&gmat[6][0],1,&gmat[7][0],1,&wsp7[0],1,&wsp7[0],1);
                        // wsp7 = g0*g2 + g3*g5 + g6*g8
                        Vmath::Vvtvvtp(nqtot,&gmat[0][0],1,&gmat[2][0],1,&gmat[3][0],1,&gmat[5][0],1,&wsp8[0],1);
                        Vmath::Vvtvp  (nqtot,&gmat[6][0],1,&gmat[8][0],1,&wsp8[0],1,&wsp8[0],1);
                        // wsp8 = g1*g2 + g4*g5 + g7*g8
                        Vmath::Vvtvvtp(nqtot,&gmat[1][0],1,&gmat[2][0],1,&gmat[4][0],1,&gmat[5][0],1,&wsp9[0],1);
                        Vmath::Vvtvp  (nqtot,&gmat[7][0],1,&gmat[8][0],1,&wsp9[0],1,&wsp9[0],1);

                        // Multiply wsp1,2,3 by the appropriate factor composites
                        Vmath::Vvtvvtp(nqtot,&wsp4[0],1,&wsp1[0],1,&wsp7[0],1,&wsp2[0],1,&wsp4[0],1);
                        Vmath::Vvtvp  (nqtot,&wsp8[0],1,&wsp3[0],1,&wsp4[0],1,&wsp4[0],1);
                        Vmath::Vvtvvtp(nqtot,&wsp5[0],1,&wsp2[0],1,&wsp7[0],1,&wsp1[0],1,&wsp5[0],1);
                        Vmath::Vvtvp  (nqtot,&wsp9[0],1,&wsp3[0],1,&wsp5[0],1,&wsp5[0],1);
                        Vmath::Vvtvvtp(nqtot,&wsp6[0],1,&wsp3[0],1,&wsp8[0],1,&wsp1[0],1,&wsp6[0],1);
                        Vmath::Vvtvp  (nqtot,&wsp9[0],1,&wsp2[0],1,&wsp6[0],1,&wsp6[0],1);
                    }
                    else
                    {
                        NekDouble g0 = gmat[0][0]*gmat[0][0] + gmat[3][0]*gmat[3][0] + gmat[6][0]*gmat[6][0];
                        NekDouble g1 = gmat[1][0]*gmat[1][0] + gmat[4][0]*gmat[4][0] + gmat[7][0]*gmat[7][0];
                        NekDouble g2 = gmat[2][0]*gmat[2][0] + gmat[5][0]*gmat[5][0] + gmat[8][0]*gmat[8][0];
                        NekDouble g3 = gmat[0][0]*gmat[1][0] + gmat[3][0]*gmat[4][0] + gmat[6][0]*gmat[7][0];
                        NekDouble g4 = gmat[0][0]*gmat[2][0] + gmat[3][0]*gmat[5][0] + gmat[6][0]*gmat[8][0];
                        NekDouble g5 = gmat[1][0]*gmat[2][0] + gmat[4][0]*gmat[5][0] + gmat[7][0]*gmat[8][0];

                        Vmath::Svtsvtp(nqtot,g0,&wsp1[0],1,g3,&wsp2[0],1,&wsp4[0],1);
                        Vmath::Svtvp  (nqtot,g4,&wsp3[0],1,&wsp4[0],1,&wsp4[0],1);
                        Vmath::Svtsvtp(nqtot,g1,&wsp2[0],1,g3,&wsp1[0],1,&wsp5[0],1);
                        Vmath::Svtvp  (nqtot,g5,&wsp3[0],1,&wsp5[0],1,&wsp5[0],1);
                        Vmath::Svtsvtp(nqtot,g2,&wsp3[0],1,g4,&wsp1[0],1,&wsp6[0],1);
                        Vmath::Svtvp  (nqtot,g5,&wsp2[0],1,&wsp6[0],1,&wsp6[0],1);
                    }

                    MultiplyByQuadratureMetric(wsp4,wsp4);
                    MultiplyByQuadratureMetric(wsp5,wsp5);
                    MultiplyByQuadratureMetric(wsp6,wsp6);

                    // outarray = m = (D_xi1 * B)^T * k
                    // wsp1     = n = (D_xi2 * B)^T * l
                    IProductWRTBase_SumFacKernel(dbase0,base1,base2,wsp4,outarray,wsp,false,true,true);
                    IProductWRTBase_SumFacKernel(base0,dbase1,base2,wsp5,wsp1,    wsp,true,false,true);
                    IProductWRTBase_SumFacKernel(base0,base1,dbase2,wsp6,wsp2,    wsp,true,true,false);

                    // outarray = outarray + wsp1
                    //          = L * u_hat
                    Vmath::Vadd(m_ncoeffs,wsp1.get(),1,outarray.get(),1,outarray.get(),1);
                    Vmath::Vadd(m_ncoeffs,wsp2.get(),1,outarray.get(),1,outarray.get(),1);
                }
            }
            else
            {
                StdExpansion::LaplacianMatrixOp_MatFree_GenericImpl(inarray,outarray,mkey);
            }
        }


        DNekMatSharedPtr TetExp::GenMatrix(const StdRegions::StdMatrixKey &mkey)
        {
            DNekMatSharedPtr returnval;

            switch(mkey.GetMatrixType())
            {
            case StdRegions::eHybridDGHelmholtz:
            case StdRegions::eHybridDGLamToU:
            case StdRegions::eHybridDGLamToQ0:
            case StdRegions::eHybridDGLamToQ1:
            case StdRegions::eHybridDGLamToQ2:
            case StdRegions::eHybridDGHelmBndLam:
                returnval = Expansion3D::GenMatrix(mkey);
                break;
            default:
                returnval = StdTetExp::GenMatrix(mkey);
            }

            return returnval;
        }


        DNekScalMatSharedPtr TetExp::CreateMatrix(const MatrixKey &mkey)
        {
            DNekScalMatSharedPtr returnval;

            ASSERTL2(m_metricinfo->GetGtype() != SpatialDomains::eNoGeomType,"Geometric information is not set up");

            switch(mkey.GetMatrixType())
            {
            case StdRegions::eMass:
                {
                    if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                    {
                        NekDouble one = 1.0;
                        DNekMatSharedPtr mat = GenMatrix(*mkey.GetStdMatKey());
                        returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,mat);
                    }
                    else
                    {
                        NekDouble jac = (m_metricinfo->GetJac())[0];
                        DNekMatSharedPtr mat = GetStdMatrix(*mkey.GetStdMatKey());
                        returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(jac,mat);
                    }
                }
                break;
            case StdRegions::eInvMass:
                {
                    if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                    {
                        NekDouble one = 1.0;
                        StdRegions::StdMatrixKey masskey(StdRegions::eMass,DetExpansionType(),
                                                         *this);
                        DNekMatSharedPtr mat = GenMatrix(masskey);
                        mat->Invert();
                        returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,mat);
                    }
                    else
                    {
                        NekDouble fac = 1.0/(m_metricinfo->GetJac())[0];
                        DNekMatSharedPtr mat = GetStdMatrix(*mkey.GetStdMatKey());
                        returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(fac,mat);
                    }
                }
                break;
            case StdRegions::eLaplacian:
                {
                    if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                    {
                        NekDouble one = 1.0;
                        DNekMatSharedPtr mat = GenMatrix(*mkey.GetStdMatKey());

                        returnval = MemoryManager<DNekScalMat>
                                                ::AllocateSharedPtr(one,mat);
                    }
                    else
                    {
                        MatrixKey lap00key(StdRegions::eLaplacian00,
                                           mkey.GetExpansionType(), *this);
                        MatrixKey lap01key(StdRegions::eLaplacian01,
                                           mkey.GetExpansionType(), *this);
                        MatrixKey lap02key(StdRegions::eLaplacian02,
                                           mkey.GetExpansionType(), *this);
                        MatrixKey lap11key(StdRegions::eLaplacian11,
                                           mkey.GetExpansionType(), *this);
                        MatrixKey lap12key(StdRegions::eLaplacian12,
                                           mkey.GetExpansionType(), *this);
                        MatrixKey lap22key(StdRegions::eLaplacian22,
                                           mkey.GetExpansionType(), *this);

                        DNekMat &lap00
                                    = *GetStdMatrix(*lap00key.GetStdMatKey());
                        DNekMat &lap01
                                    = *GetStdMatrix(*lap01key.GetStdMatKey());
                        DNekMat &lap02
                                    = *GetStdMatrix(*lap02key.GetStdMatKey());
                        DNekMat &lap11
                                    = *GetStdMatrix(*lap11key.GetStdMatKey());
                        DNekMat &lap12
                                    = *GetStdMatrix(*lap12key.GetStdMatKey());
                        DNekMat &lap22
                                    = *GetStdMatrix(*lap22key.GetStdMatKey());

                        NekDouble jac = (m_metricinfo->GetJac())[0];
                        Array<TwoD, const NekDouble> gmat
                                                    = m_metricinfo->GetGmat();

                        int rows = lap00.GetRows();
                        int cols = lap00.GetColumns();

                        DNekMatSharedPtr lap = MemoryManager<DNekMat>
                                                ::AllocateSharedPtr(rows,cols);

                        (*lap) = (gmat[0][0]*gmat[0][0] + gmat[3][0]*gmat[3][0]
                                        + gmat[6][0]*gmat[6][0])*lap00
                               + (gmat[1][0]*gmat[1][0] + gmat[4][0]*gmat[4][0]
                                        + gmat[7][0]*gmat[7][0])*lap11
                               + (gmat[2][0]*gmat[2][0] + gmat[5][0]*gmat[5][0]
                                        + gmat[8][0]*gmat[8][0])*lap22
                               + (gmat[0][0]*gmat[1][0] + gmat[3][0]*gmat[4][0]
                                        + gmat[6][0]*gmat[7][0])
                                 *(lap01 + Transpose(lap01))
                               + (gmat[0][0]*gmat[2][0] + gmat[3][0]*gmat[5][0]
                                        + gmat[6][0]*gmat[8][0])
                                 *(lap02 + Transpose(lap02))
                               + (gmat[1][0]*gmat[2][0] + gmat[4][0]*gmat[5][0]
                                        + gmat[7][0]*gmat[8][0])
                                 *(lap12 + Transpose(lap12));

                        returnval = MemoryManager<DNekScalMat>
                                                ::AllocateSharedPtr(jac,lap);
                    }
                }
                break;
            case StdRegions::eHelmholtz:
                {
                    NekDouble factor = mkey.GetConstant(0);
                    MatrixKey masskey(StdRegions::eMass, mkey.GetExpansionType(), *this);
                    DNekScalMat &MassMat = *(this->m_matrixManager[masskey]);
                    MatrixKey lapkey(StdRegions::eLaplacian, mkey.GetExpansionType(), *this);
                    DNekScalMat &LapMat = *(this->m_matrixManager[lapkey]);

                    int rows = LapMat.GetRows();
                    int cols = LapMat.GetColumns();

                    DNekMatSharedPtr helm = MemoryManager<DNekMat>::AllocateSharedPtr(rows, cols);

                    NekDouble one = 1.0;
                    (*helm) = LapMat + factor*MassMat;

                    returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(one, helm);
                }
                break;
            case StdRegions::eIProductWRTBase:
                {
                    if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                    {
                        NekDouble one = 1.0;
                        DNekMatSharedPtr mat = GenMatrix(*mkey.GetStdMatKey());
                        returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,mat);
                    }
                    else
                    {
                        NekDouble jac = (m_metricinfo->GetJac())[0];
                        DNekMatSharedPtr mat = GetStdMatrix(*mkey.GetStdMatKey());
                        returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(jac,mat);
                    }
                }
                break;
            default:
				{
					//ASSERTL0(false, "Missing definition for " + (*StdRegions::MatrixTypeMap[mkey.GetMatrixType()]));
					NekDouble        one = 1.0;
					DNekMatSharedPtr mat = GenMatrix(*mkey.GetStdMatKey());

					returnval = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,mat);
				}
				break;
            }

            return returnval;
        }


        DNekScalBlkMatSharedPtr TetExp::CreateStaticCondMatrix(const MatrixKey &mkey)
        {
            DNekScalBlkMatSharedPtr returnval;

            ASSERTL2(m_metricinfo->GetGtype() != SpatialDomains::eNoGeomType,"Geometric information is not set up");

            // set up block matrix system
            int nbdry = NumBndryCoeffs();
            int nint = m_ncoeffs - nbdry;

            unsigned int exp_size[] = {nbdry, nint};
            int nblks = 2;
            returnval = MemoryManager<DNekScalBlkMat>::AllocateSharedPtr(nblks, nblks, exp_size, exp_size); //Really need a constructor which takes Arrays
            NekDouble factor = 1.0;

            switch(mkey.GetMatrixType())
            {
            case StdRegions::eLaplacian:
            case StdRegions::eHelmholtz: // special case since Helmholtz not defined in StdRegions

                // use Deformed case for both regular and deformed geometries
                factor = 1.0;
                goto UseLocRegionsMatrix;
                break;
            default:
                if(m_metricinfo->GetGtype() == SpatialDomains::eDeformed)
                {
                    factor = 1.0;
                    goto UseLocRegionsMatrix;
                }
                else
                {
                    DNekScalMatSharedPtr mat = GetLocMatrix(mkey);
                    factor = mat->Scale();
                    goto UseStdRegionsMatrix;
                }
                break;
            UseStdRegionsMatrix:
                {
                    NekDouble            invfactor = 1.0/factor;
                    NekDouble            one = 1.0;
                    DNekBlkMatSharedPtr  mat = GetStdStaticCondMatrix(*(mkey.GetStdMatKey()));
                    DNekScalMatSharedPtr Atmp;
                    DNekMatSharedPtr     Asubmat;

                    //TODO: check below
                    returnval->SetBlock(0,0,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(factor,Asubmat = mat->GetBlock(0,0)));
                    returnval->SetBlock(0,1,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,Asubmat = mat->GetBlock(0,1)));
                    returnval->SetBlock(1,0,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(factor,Asubmat = mat->GetBlock(1,0)));
                    returnval->SetBlock(1,1,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(invfactor,Asubmat = mat->GetBlock(1,1)));
                }
                break;
            UseLocRegionsMatrix:
                {
                    int i,j;
                    NekDouble            invfactor = 1.0/factor;
                    NekDouble            one = 1.0;
                    DNekScalMat &mat = *GetLocMatrix(mkey);
                    DNekMatSharedPtr A = MemoryManager<DNekMat>::AllocateSharedPtr(nbdry,nbdry);
                    DNekMatSharedPtr B = MemoryManager<DNekMat>::AllocateSharedPtr(nbdry,nint);
                    DNekMatSharedPtr C = MemoryManager<DNekMat>::AllocateSharedPtr(nint,nbdry);
                    DNekMatSharedPtr D = MemoryManager<DNekMat>::AllocateSharedPtr(nint,nint);

                    Array<OneD,unsigned int> bmap(nbdry);
                    Array<OneD,unsigned int> imap(nint);
                    GetBoundaryMap(bmap);
                    GetInteriorMap(imap);

                    for(i = 0; i < nbdry; ++i)
                    {
                        for(j = 0; j < nbdry; ++j)
                        {
                            (*A)(i,j) = mat(bmap[i],bmap[j]);
                        }

                        for(j = 0; j < nint; ++j)
                        {
                            (*B)(i,j) = mat(bmap[i],imap[j]);
                        }
                    }

                    for(i = 0; i < nint; ++i)
                    {
                        for(j = 0; j < nbdry; ++j)
                        {
                            (*C)(i,j) = mat(imap[i],bmap[j]);
                        }

                        for(j = 0; j < nint; ++j)
                        {
                            (*D)(i,j) = mat(imap[i],imap[j]);
                        }
                    }

                    // Calculate static condensed system
                    if(nint)
                    {
                        D->Invert();
                        (*B) = (*B)*(*D);
                        (*A) = (*A) - (*B)*(*C);
                    }

                    DNekScalMatSharedPtr     Atmp;

                    returnval->SetBlock(0,0,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(factor,A));
                    returnval->SetBlock(0,1,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,B));
                    returnval->SetBlock(1,0,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(factor,C));
                    returnval->SetBlock(1,1,Atmp = MemoryManager<DNekScalMat>::AllocateSharedPtr(invfactor,D));

                }
            }
            return returnval;
        }

        /// Return Shape of region, using  ShapeType enum list.
        StdRegions::ExpansionType TetExp::v_DetExpansionType() const
        {
            return StdRegions::eTetrahedron;
        }

        const SpatialDomains::GeomFactorsSharedPtr& TetExp::v_GetMetricInfo() const
        {
            return m_metricinfo;
        }

        int TetExp::v_GetCoordim()
        {
            return m_geom->GetCoordim();
        }

        NekDouble TetExp::v_Linf(const Array<OneD, const NekDouble> &sol)
        {
            return Linf(sol);
        }

        NekDouble TetExp::v_Linf()
        {
            return Linf();
        }

        NekDouble TetExp::v_L2(const Array<OneD, const NekDouble> &sol)
        {
            return StdExpansion::L2(sol);
        }

        NekDouble TetExp::v_L2()
        {
            return StdExpansion::L2();
        }

        DNekMatSharedPtr TetExp::v_CreateStdMatrix(const StdRegions::StdMatrixKey &mkey)
        {
            LibUtilities::BasisKey bkey0 = m_base[0]->GetBasisKey();
            LibUtilities::BasisKey bkey1 = m_base[1]->GetBasisKey();
            LibUtilities::BasisKey bkey2 = m_base[2]->GetBasisKey();
            StdRegions::StdTetExpSharedPtr tmp = MemoryManager<StdTetExp>::AllocateSharedPtr(bkey0, bkey1, bkey2);

            return tmp->GetStdMatrix(mkey);
        }

        DNekScalMatSharedPtr& TetExp::v_GetLocMatrix(const MatrixKey &mkey)
        {
            return m_matrixManager[mkey];
        }

        DNekScalBlkMatSharedPtr& TetExp::v_GetLocStaticCondMatrix(const MatrixKey &mkey)
        {
            return m_staticCondMatrixManager[mkey];
        }

    }//end of namespace
}//end of namespace

/**
 *    $Log: TetExp.cpp,v $
 *    Revision 1.24  2010/03/02 11:13:04  cantwell
 *    Minor updates to TetExp.
 *
 *    Revision 1.23  2010/02/26 13:52:45  cantwell
 *    Tested and fixed where necessary Hex/Tet projection and differentiation in
 *      StdRegions, and LocalRegions for regular and deformed (where applicable).
 *    Added SpatialData and SpatialParameters classes for managing spatiall-varying
 *      data.
 *    Added TimingGeneralMatrixOp3D for timing operations on 3D geometries along
 *      with some associated input meshes.
 *    Added 3D std and loc projection demos for tet and hex.
 *    Added 3D std and loc regression tests for tet and hex.
 *    Fixed bugs in regression tests in relation to reading OK files.
 *    Extended Elemental and Global optimisation parameters for 3D expansions.
 *    Added GNUPlot output format option.
 *    Updated ADR2DManifoldSolver to use spatially varying data.
 *    Added Barkley model to ADR2DManifoldSolver.
 *    Added 3D support to FldToVtk and XmlToVtk.
 *    Renamed History.{h,cpp} to HistoryPoints.{h,cpp}
 *
 *    Revision 1.22  2009/12/15 18:09:02  cantwell
 *    Split GeomFactors into 1D, 2D and 3D
 *    Added generation of tangential basis into GeomFactors
 *    Updated ADR2DManifold solver to use GeomFactors for tangents
 *    Added <GEOMINFO> XML session section support in MeshGraph
 *    Fixed const-correctness in VmathArray
 *    Cleaned up LocalRegions code to generate GeomFactors
 *    Removed GenSegExp
 *    Temporary fix to SubStructuredGraph
 *    Documentation for GlobalLinSys and GlobalMatrix classes
 *
 *    Revision 1.21  2009/10/30 14:00:07  pvos
 *    Multi-level static condensation updates
 *
 *    Revision 1.20  2009/07/08 17:19:48  sehunchun
 *    Deleting GetTanBasis
 *
 *    Revision 1.19  2009/04/27 21:34:07  sherwin
 *    Updated WriteToField
 *
 *    Revision 1.18  2009/01/21 16:59:57  pvos
 *    Added additional geometric factors to improve efficiency
 *
 *    Revision 1.17  2008/09/09 15:05:09  sherwin
 *    Updates related to cuved geometries. Normals have been removed from m_metricinfo and replaced with a direct evaluation call. Interp methods have been moved to LibUtilities
 *
 *    Revision 1.16  2008/08/14 22:12:57  sherwin
 *    Introduced Expansion classes and used them to define HDG routines, has required quite a number of virtual functions to be added
 *
 *    Revision 1.15  2008/07/09 11:44:49  sherwin
 *    Replaced GetScaleFactor call with GetConstant(0)
 *
 *    Revision 1.14  2008/07/04 10:19:05  pvos
 *    Some updates
 *
 *    Revision 1.13  2008/06/14 01:20:53  ehan
 *    Clean up the codes
 *
 *    Revision 1.12  2008/06/06 23:25:21  ehan
 *    Added doxygen documentation
 *
 *    Revision 1.11  2008/06/05 20:18:47  ehan
 *    Fixed undefined function GetGtype() in the ASSERTL2().
 *
 *    Revision 1.10  2008/06/02 23:35:26  ehan
 *    Fixed warning : no new line at end of file
 *
 *    Revision 1.9  2008/05/30 00:33:48  delisi
 *    Renamed StdRegions::ShapeType to StdRegions::ExpansionType.
 *
 *    Revision 1.8  2008/05/29 21:33:37  pvos
 *    Added WriteToFile routines for Gmsh output format + modification of BndCond implementation in MultiRegions
 *
 *    Revision 1.7  2008/05/29 01:02:13  bnelson
 *    Added precompiled header support.
 *
 *    Revision 1.6  2008/04/06 05:59:05  bnelson
 *    Changed ConstArray to Array<const>
 *
 *    Revision 1.5  2008/03/17 10:36:17  pvos
 *    Clean up of the code
 *
 *    Revision 1.4  2008/02/16 05:52:49  ehan
 *    Added PhysDeriv and virtual functions.
 *
 *    Revision 1.3  2008/02/05 00:40:57  ehan
 *    Added initial tetrahedral expansion.
 *
 *    Revision 1.2  2007/07/20 00:45:51  bnelson
 *    Replaced boost::shared_ptr with Nektar::ptr
 *
 *    Revision 1.1  2006/05/04 18:58:46  kirby
 *    *** empty log message ***
 *
 *    Revision 1.9  2006/03/12 07:43:32  sherwin
 *
 *    First revision to meet coding standard. Needs to be compiled
 *
 **/
