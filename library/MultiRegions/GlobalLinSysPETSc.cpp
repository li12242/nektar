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

#include <MultiRegions/GlobalLinSysPETSc.h>

#include "petscis.h"
#include "petscversion.h"

namespace Nektar
{
    namespace MultiRegions
    {
        /**
         * @class GlobalLinSysPETSc
         *
         * Solves a linear system using direct methods.
         */


        /// Constructor for full direct matrix solve.
        GlobalLinSysPETSc::GlobalLinSysPETSc(
            const GlobalLinSysKey                &pKey,
            const boost::weak_ptr<ExpList>       &pExp,
            const boost::shared_ptr<AssemblyMap> &pLocToGloMap)
            : GlobalLinSys(pKey, pExp, pLocToGloMap)
        {
            // Initialise PETSc
            PetscInitialize(0, NULL, NULL, NULL);

            // Create matrix
            MatCreate(PETSC_COMM_WORLD, &m_matrix);
        }

        GlobalLinSysPETSc::~GlobalLinSysPETSc()
        {
        }

        void GlobalLinSysPETSc::v_SolveLinearSystem(
            const int                          pNumRows,
            const Array<OneD,const NekDouble> &pInput,
                  Array<OneD,      NekDouble> &pOutput,
            const AssemblyMapSharedPtr        &locToGloMap,
            const int                          pNumDir)
        {
            const int nHomDofs = pNumRows - pNumDir;

            // Populate RHS vector from input
            VecSetValues(m_b, nHomDofs, &m_reorderedMap[0], &pInput[pNumDir], INSERT_VALUES);

            // Assemble RHS vector
            VecAssemblyBegin(m_b);
            VecAssemblyEnd  (m_b);

            // Do system solve
            PetscErrorCode ierr = KSPSolve(m_ksp, m_b, m_x);

            // Grab number of iterations taken
            PetscInt its;
            KSPGetIterationNumber(m_ksp, &its);
            cout << "iterations = " << its << endl;

            // Scatter results to local vector
            VecScatterBegin(m_ctx, m_x, m_locVec, INSERT_VALUES, SCATTER_FORWARD);
            VecScatterEnd  (m_ctx, m_x, m_locVec, INSERT_VALUES, SCATTER_FORWARD);

            // Copy results into output vector
            PetscScalar *avec;
            VecGetArray    (m_locVec, &avec);
            Vmath::Vcopy   (nHomDofs, avec, 1, &pOutput[pNumDir], 1);
            VecRestoreArray(m_locVec, &avec);
        }

        void GlobalLinSysPETSc::SetUpScatter()
        {
            const int nHomDofs = m_reorderedMap.size();

            // Create local and global numbering systems for vector
            IS isGlobal, isLocal;
            ISCreateGeneral(PETSC_COMM_SELF, nHomDofs, &m_reorderedMap[0],
                            PETSC_COPY_VALUES, &isGlobal);
            ISCreateStride (PETSC_COMM_SELF, nHomDofs, 0, 1, &isLocal);

            // Create local vector for output
            VecCreate        (PETSC_COMM_SELF, &m_locVec);
            VecSetSizes      (m_locVec, m_reorderedMap.size(), PETSC_DECIDE);
            VecSetFromOptions(m_locVec);

            // Create scatter context
            VecScatterCreate (m_x, isGlobal, m_locVec, isLocal, &m_ctx);

            // Clean up
            ISDestroy(&isGlobal);
            ISDestroy(&isLocal);
        }

        void GlobalLinSysPETSc::CalculateReordering(
            const Array<OneD, const int> &glo2uniMap,
            const Array<OneD, const int> &glo2unique,
            const AssemblyMapSharedPtr   &pLocToGloMap)
        {
            LibUtilities::CommSharedPtr vComm
                = m_expList.lock()->GetSession()->GetComm();

            const int nDirDofs = pLocToGloMap->GetNumGlobalDirBndCoeffs();
            const int nHomDofs = glo2uniMap.num_elements() - nDirDofs;
            const int nProc    = vComm->GetSize();
            const int rank     = vComm->GetRank();

            int n, cnt;

            m_nLocal = Vmath::Vsum(nHomDofs, glo2unique + nDirDofs, 1);

            m_reorderedMap.resize(nHomDofs);

            Array<OneD, int> localCounts(nProc, 0), localOffset(nProc, 0);
            localCounts[rank] = nHomDofs;
            vComm->AllReduce(localCounts, LibUtilities::ReduceSum);

            for (n = 1; n < nProc; ++n)
            {
                localOffset[n] = localOffset[n-1] + localCounts[n-1];
            }

            int totHomDofs = Vmath::Vsum(nProc, localCounts, 1);
            vector<unsigned int> allUniIds(totHomDofs, 0);

            for (n = 0; n < nHomDofs; ++n)
            {
                int gid = n + nDirDofs;
                allUniIds[n + localOffset[rank]] = glo2uniMap[gid];
            }

            vComm->AllReduce(allUniIds, LibUtilities::ReduceSum);
            std::sort(allUniIds.begin(), allUniIds.end());
            map<int,int> uniIdReorder;

            for (cnt = n = 0; n < allUniIds.size(); ++n)
            {
                if (uniIdReorder.count(allUniIds[n]) > 0)
                {
                    continue;
                }

                uniIdReorder[allUniIds[n]] = cnt++;
            }

            for (n = 0; n < nHomDofs; ++n)
            {
                int gid = n + nDirDofs;
                int uniId = glo2uniMap[gid];
                ASSERTL0(uniIdReorder.count(uniId) > 0, "Error in ordering");
                m_reorderedMap[n] = uniIdReorder[uniId];
            }
        }

        void GlobalLinSysPETSc::SetUpMatVec()
        {
            // CREATE VECTORS
            VecCreate        (PETSC_COMM_WORLD, &m_x);
            VecSetSizes      (m_x, m_nLocal, PETSC_DECIDE);
            VecSetFromOptions(m_x);
            VecDuplicate     (m_x, &m_b);

            // CREATE MATRICES
            MatCreate        (PETSC_COMM_WORLD, &m_matrix);
            MatSetType       (m_matrix, MATMPIAIJ);
            MatSetSizes      (m_matrix, m_nLocal, m_nLocal,
                              PETSC_DETERMINE, PETSC_DETERMINE);
            MatSetFromOptions(m_matrix);
            MatSetUp         (m_matrix);
        }

        void GlobalLinSysPETSc::SetUpSolver(NekDouble tolerance)
        {
            KSPCreate(PETSC_COMM_WORLD, &m_ksp);
            KSPSetTolerances(
                m_ksp, tolerance, PETSC_DEFAULT, PETSC_DEFAULT, PETSC_DEFAULT);
            KSPSetFromOptions(m_ksp);
#if PETSC_VERSION_GE(3,5,0)
            KSPSetOperators(m_ksp, m_matrix, m_matrix);
#else
            KSPSetOperators(m_ksp, m_matrix, m_matrix, SAME_NONZERO_PATTERN);
#endif
        }
    }
}
