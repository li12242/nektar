///////////////////////////////////////////////////////////////////////////////
//
// File: MatrixOperations.cpp
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
// Description: Defines the global functions needed for matrix operations.
//
///////////////////////////////////////////////////////////////////////////////

#include <LibUtilities/LinearAlgebra/MatrixOperations.hpp>

namespace Nektar
{     
    void NekMultiplyEqual(NekMatrix<double, StandardMatrixTag>& result,
                          const NekMatrix<const double, StandardMatrixTag>& rhs)
    {
        ASSERTL0(result.GetType() == eFULL && rhs.GetType() == eFULL, "Only full matrices supported.");
        unsigned int M = result.GetRows();
        unsigned int N = rhs.GetColumns();
        unsigned int K = result.GetColumns();

        unsigned int LDA = M;
        if( result.GetTransposeFlag() == 'T' )
        {
            LDA = K;
        }

        unsigned int LDB = K;
        if( rhs.GetTransposeFlag() == 'T' )
        {
            LDB = N;
        }

        Array<OneD, double>& buf = result.GetTempSpace();
        Blas::Dgemm(result.GetTransposeFlag(), rhs.GetTransposeFlag(), M, N, K,
            1.0, result.GetRawPtr(), LDA, rhs.GetRawPtr(), LDB, 0.0,
            buf.data(), result.GetRows());
        result.SetSize(result.GetRows(), rhs.GetColumns());
        result.SwapTempAndDataBuffers();
    }
}


