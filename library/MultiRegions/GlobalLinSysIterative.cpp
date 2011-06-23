///////////////////////////////////////////////////////////////////////////////
//
// File GlobalLinSysIterative.cpp
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
// Description: GlobalLinSysIterative definition
//
///////////////////////////////////////////////////////////////////////////////

#include <LibUtilities/BasicUtils/VDmathArray.hpp>
#include <MultiRegions/GlobalLinSysIterative.h>
#include <MultiRegions/LocalToGlobalC0ContMap.h>

namespace Nektar
{
    namespace MultiRegions
    {
        /**
         * @class GlobalLinSysIterative
         *
         * Solves a linear system using direct methods.
         */

        /// Constructor for full direct matrix solve.
        GlobalLinSysIterative::GlobalLinSysIterative(
                const GlobalLinSysKey &pKey,
                const boost::shared_ptr<ExpList> &pExpList,
                const boost::shared_ptr<LocalToGlobalBaseMap>
                                                       &pLocToGloMap)
                : GlobalLinSys(pKey, pExpList, pLocToGloMap)
        {
        }

        GlobalLinSysIterative::~GlobalLinSysIterative()
        {
        }

        /**
         * Solve a global linear system using the conjugate gradient method.
         * We solve only for the non-Dirichlet modes. The operator is evaluated
         * using the local-matrix representation. Distributed math routines are
         * used to support parallel execution of the solver.
         * @param       pInput      Input vector of non-Dirichlet DOFs.
         * @param       pOutput     Solution vector of non-Dirichlet DOFs.
         */
        void GlobalLinSysIterative::v_SolveLinearSystem(
                    const int nGlobal,
                    const Array<OneD,const NekDouble> &pInput,
                          Array<OneD,      NekDouble> &pOutput,
                    const int nDir)
        {
            // Check if preconditioner has been computed and compute if needed.
            if (!m_preconditioner)
            {
                v_ComputePreconditioner();
            }

            // Get the communicator for performing data exchanges
            LibUtilities::CommSharedPtr vComm = m_expList->GetComm();

            // Get vector sizes
            int nNonDir = nGlobal - nDir;

            // Allocate array storage
            Array<OneD, NekDouble> d_A    (nGlobal, 0.0);
            Array<OneD, NekDouble> p_A    (nGlobal, 0.0);
            Array<OneD, NekDouble> z_A    (nNonDir, 0.0);
            Array<OneD, NekDouble> z_new_A(nNonDir, 0.0);
            Array<OneD, NekDouble> r_A    (nNonDir, 0.0);
            Array<OneD, NekDouble> r_new_A(nNonDir, 0.0);

            // Create NekVector wrappers for linear algebra operations
            NekVector<NekDouble> in(nNonDir,pInput,eWrapper);
            NekVector<NekDouble> out(nNonDir,pOutput,eWrapper);
            NekVector<NekDouble> r(nNonDir,r_A,eWrapper);
            NekVector<NekDouble> r_new(nNonDir,r_new_A,eWrapper);
            NekVector<NekDouble> z(nNonDir,z_A,eWrapper);
            NekVector<NekDouble> z_new(nNonDir,z_new_A,eWrapper);
            NekVector<NekDouble> d(nNonDir,d_A + nDir, eWrapper);
            NekVector<NekDouble> p(nNonDir,p_A + nDir,eWrapper);

            int k;
            NekDouble alpha, beta, normsq;
            Array<OneD, NekDouble> vExchange(3);

            // INVERSE of preconditioner matrix.
            const DNekMat &M = (*m_preconditioner);

            // Initialise with zero as the initial guess.
            r = in;
            z = M * r;
            d = z;
            k = 0;

            // If input vector is zero, set zero output and skip solve.
            vExchange[0] = VDmath::Ddot2(vComm, nNonDir, r_A, r_A, m_map + nDir);
            if (vExchange[0] < NekConstants::kNekZeroTol)
            {
                Vmath::Zero(nNonDir, pOutput, 1);
                return;
            }

            // Continue until convergence
            while (true)
            {
                // Perform the method-specific matrix-vector multiply operation.
                v_DoMatrixMultiply(d_A, p_A);

                // compute step length alpha
                // alpha denominator
                vExchange[0] = Vmath::Dot2(nNonDir,
                                        d_A + nDir,
                                        p_A + nDir,
                                        m_map + nDir);
                // alpha numerator
                vExchange[1] = Vmath::Dot2(nNonDir,
                                        z_A,
                                        r_A,
                                        m_map + nDir);
                // beta denominator
                vExchange[2] = Vmath::Dot2(nNonDir,
                                        r_A,
                                        z_A,
                                        m_map + nDir);
                // perform exchange
                vComm->AllReduce(vExchange, Nektar::LibUtilities::ReduceSum);

                // compute alpha
                alpha = vExchange[1]/vExchange[0];

                // approximate solution
                out   = out + alpha*d;

                // compute residual
                r_new = r   - alpha*p;

                // Apply preconditioner to new residual
                z_new = M * r_new;

                // beta
                vExchange[0] = Vmath::Dot2(nNonDir,
                                        r_new_A,
                                        z_new_A,
                                        m_map + nDir) / vExchange[2];

                // residual
                vExchange[1] = Vmath::Dot2(nNonDir,
                                        r_new_A,
                                        r_new_A,
                                        m_map + nDir);

                // perform exchange
                vComm->AllReduce(vExchange, Nektar::LibUtilities::ReduceSum);

                // extract values for beta and norm
                beta = vExchange[0];
                normsq = vExchange[1];

                // test if norm is within tolerance
                if (sqrt(normsq) < NekConstants::kNekIterativeTol)
                {
                    break;
                }

                // Compute new search direction
                d = z_new + beta*d;

                // Next step
                r = r_new;
                z = z_new;
                k++;

                ASSERTL1(k < 20000,
                         "Exceeded maximum number of iterations (20000)");
            }
        }
    }
}
