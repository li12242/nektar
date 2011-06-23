///////////////////////////////////////////////////////////////////////////////
//
// File GlobalLinSys.cpp
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
// Description: GlobalLinSys definition
//
///////////////////////////////////////////////////////////////////////////////

#include <MultiRegions/GlobalLinSys.h>
#include <MultiRegions/LocalToGlobalC0ContMap.h>

namespace Nektar
{
    namespace MultiRegions
    {
        /**
         * @class GlobalLinSys
         *
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
         */


        GlobalLinSys::GlobalLinSys()
        {

        }


        /**
         * Given a block matrix, construct a global matrix system according to
         * a local to global mapping. #m_linSys is constructed by
         * AssembleFullMatrix().
         * @param   mkey        Associated linear system key.
         * @param   Mat         Block matrix.
         * @param   locToGloMap Local to global mapping.
         */
        GlobalLinSys::GlobalLinSys(const GlobalLinSysKey &pKey,
                const boost::shared_ptr<ExpList> &pExpList,
                const boost::shared_ptr<LocalToGlobalBaseMap>
                                   &pLocToGloMap):
            m_linSysKey(pKey),
            m_expList(pExpList)
        {

        }


        /**
         *
         */
        GlobalLinSysFactory& GetGlobalLinSysFactory()
        {
            typedef Loki::SingletonHolder<GlobalLinSysFactory,
                Loki::CreateUsingNew,
                Loki::NoDestroy > Type;
            return Type::Instance();
        }


        /**
         * Retrieves a the block matrix from n'th expansion using the matrix
         * key provided by the #m_linSysKey.
         * @param   n           Number of the expansion.
         * @returns             Block matrix for the specified expansion.
         */
        DNekScalMatSharedPtr GlobalLinSys::GetBlock(unsigned int n)
        {
            int cnt = 0;
            int nel = m_expList->GetOffset_Elmt_Id(n);
            DNekScalMatSharedPtr loc_mat;

            StdRegions::StdExpansionSharedPtr vExp = m_expList->GetExp(nel);
            const boost::shared_ptr<GlobalMatrixKey> vMatrixKey
                                            = m_linSysKey.GetGlobalMatrixKey();

            // need to be initialised with zero size for non variable
            // coefficient case
            Array<OneD, Array<OneD,const NekDouble> > vVarcoeffs;
            int vNVCoeffs = vMatrixKey->GetNvariableCoefficients();

            // retrieve variable coefficients
            if(vNVCoeffs > 0)
            {
                cnt = m_expList->GetPhys_Offset(n);
                vVarcoeffs
                        = Array<OneD, Array<OneD, const NekDouble> >(vNVCoeffs);
                for(int j = 0; j < vNVCoeffs; j++)
                {
                    vVarcoeffs[j] = vMatrixKey->GetVariableCoefficient(j) + cnt;
                }
            }

            // if requesting the HybridDGHelmBndLam matrix, retrieve the two
            // scalar constants
            if (m_linSysKey.GetMatrixType() == StdRegions::eHybridDGHelmBndLam)
            {
                NekDouble vFactor1, vFactor2;
                int Nconstants = vMatrixKey->GetNconstants();

                if(Nconstants>2)
                {
                    vFactor1 = m_linSysKey.GetConstant(nel);
                    vFactor2 = m_linSysKey.GetConstant(Nconstants-1);
                }

                else
                {
                    vFactor1 = m_linSysKey.GetConstant(0);
                    vFactor2 = m_linSysKey.GetConstant(1);
                }

                LocalRegions::MatrixKey matkey(vMatrixKey->GetMatrixType(),
                                               vExp->DetExpansionType(),
                                               *vExp, vFactor1, vFactor2,
                                               vVarcoeffs);
                loc_mat = vExp->GetLocMatrix(matkey);
            }
            else
            {
                LocalRegions::MatrixKey matkey(vMatrixKey->GetMatrixType(),
                                           vExp->DetExpansionType(),
                                           *vExp, vMatrixKey->GetConstants(),
                                           vVarcoeffs);
                loc_mat = vExp->GetLocMatrix(matkey);
            }

            // retrieve robin boundary condition information and apply robin
            // boundary conditions to the matrix.
            const map<int, RobinBCInfoSharedPtr> vRobinBCInfo
                                                = m_expList->GetRobinBCInfo();
            if(vRobinBCInfo.count(nel) != 0) // add robin mass matrix
            {
                RobinBCInfoSharedPtr rBC;

                // declare local matrix from scaled matrix.
                int rows = loc_mat->GetRows();
                int cols = loc_mat->GetColumns();
                const NekDouble *dat = loc_mat->GetRawPtr();
                DNekMatSharedPtr new_mat = MemoryManager<DNekMat>::AllocateSharedPtr(rows,cols,dat);
                Blas::Dscal(rows*cols,loc_mat->Scale(),new_mat->GetRawPtr(),1);

                // add local matrix contribution
                for(rBC = vRobinBCInfo.find(nel)->second;rBC; rBC = rBC->next)
                {
                    vExp->AddRobinMassMatrix(rBC->m_robinID,rBC->m_robinPrimitiveCoeffs,new_mat);
                }

                NekDouble one = 1.0;
                // redeclare loc_mat to point to new_mat plus the scalar.
                loc_mat = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,new_mat);
            }

            // finally return the matrix.
            return loc_mat;
        }


        /**
         * Retrieves a the static condensation block matrices from n'th
         * expansion using the matrix key provided by the #m_linSysKey.
         * @param   n           Number of the expansion
         * @returns             2x2 Block matrix holding the static condensation
         *                      matrices for the n'th expansion.
         */
        DNekScalBlkMatSharedPtr GlobalLinSys::GetStaticCondBlock(unsigned int n)
        {
            int cnt = 0;
            int nel = m_expList->GetOffset_Elmt_Id(n);
            DNekScalBlkMatSharedPtr loc_mat;
            DNekScalMatSharedPtr    tmp_mat;

            StdRegions::StdExpansionSharedPtr vExp = m_expList->GetExp(nel);
            const boost::shared_ptr<GlobalMatrixKey> vMatrixKey
                    = m_linSysKey.GetGlobalMatrixKey();

            // need to be initialised with zero size for non variable coefficient case
            Array<OneD, Array<OneD,const NekDouble> > vVarcoeffs;
            int vNVCoeffs = m_linSysKey.GetNvariableCoefficients();

            if(vNVCoeffs>0)
            {
                cnt = m_expList->GetPhys_Offset(n);
                vVarcoeffs
                        = Array<OneD, Array<OneD, const NekDouble> >(vNVCoeffs);
                for(int j = 0; j < vNVCoeffs; j++)
                {
                    vVarcoeffs[j] = vMatrixKey->GetVariableCoefficient(j) + cnt;
                }
            }

            LocalRegions::MatrixKey matkey( vMatrixKey->GetMatrixType(),
                                            vExp->DetExpansionType(),
                                            *vExp,
                                            vMatrixKey->GetConstants(),
                                            vVarcoeffs);

            loc_mat = vExp->GetLocStaticCondMatrix(matkey);

            const map<int, RobinBCInfoSharedPtr> vRobinBCInfo
                    = m_expList->GetRobinBCInfo();
            if(vRobinBCInfo.count(nel) != 0) // add robin mass matrix
            {
                RobinBCInfoSharedPtr rBC;

                tmp_mat = loc_mat->GetBlock(0,0);

                // declare local matrix from scaled matrix.
                int rows = tmp_mat->GetRows();
                int cols = tmp_mat->GetColumns();
                const NekDouble *dat = tmp_mat->GetRawPtr();
                DNekMatSharedPtr new_mat = MemoryManager<DNekMat>::AllocateSharedPtr(rows,cols,dat);
                Blas::Dscal(rows*cols,tmp_mat->Scale(),new_mat->GetRawPtr(),1);

                // add local matrix contribution
                for(rBC = vRobinBCInfo.find(nel)->second;rBC; rBC = rBC->next)
                {
                    vExp->AddRobinMassMatrix(rBC->m_robinID,rBC->m_robinPrimitiveCoeffs,new_mat);
                }

                NekDouble one = 1.0;
                // redeclare loc_mat to point to new_mat plus the scalar.
                tmp_mat = MemoryManager<DNekScalMat>::AllocateSharedPtr(one,new_mat);
                loc_mat->SetBlock(0,0,tmp_mat);
            }

            return loc_mat;
        }
    } //end of namespace
} //end of namespace

