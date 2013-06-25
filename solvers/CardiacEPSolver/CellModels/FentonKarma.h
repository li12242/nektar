///////////////////////////////////////////////////////////////////////////////
//
// File CourtemancheRamirezNattel.h
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
// Description: Courtemanche-Ramirez-Nattel cell model
//
///////////////////////////////////////////////////////////////////////////////

#ifndef NEKTAR_SOLVERS_ADRSOLVER_EQUATIONSYSTEMS_FENTONKARMA_H
#define NEKTAR_SOLVERS_ADRSOLVER_EQUATIONSYSTEMS_FENTONKARMA_H

#include <CardiacEPSolver/CellModels/CellModel.h>

namespace Nektar
{
    class FentonKarma: public CellModel
    {
    public:
        /// Creates an instance of this class
        static CellModelSharedPtr create(
                const LibUtilities::SessionReaderSharedPtr& pSession,
                const MultiRegions::ExpListSharedPtr& pField)
        {
            return MemoryManager<FentonKarma>
            ::AllocateSharedPtr(pSession, pField);
        }

        /// Name of class
        static std::string className;

        /// Constructor
        FentonKarma(
                const LibUtilities::SessionReaderSharedPtr& pSession,
                const MultiRegions::ExpListSharedPtr& pField);

        /// Destructor
        virtual ~FentonKarma();

    protected:
        /// Computes the reaction terms $f(u,v)$ and $g(u,v)$.
        virtual void v_Update(
                const Array<OneD, const Array<OneD, NekDouble> >&inarray,
                      Array<OneD, Array<OneD, NekDouble> >&outarray,
                const NekDouble time);

        /// Prints a summary of the model parameters.
        virtual void v_PrintSummary(std::ostream &out);

        virtual void v_SetInitialConditions();

        virtual std::string v_GetCellVarName(unsigned int idx);

    private:
        NekDouble C_m;
        NekDouble V_0;
        NekDouble u_fi;
        NekDouble u_c;
        NekDouble u_v;
        NekDouble u_r;
        NekDouble g_fi_max;
        NekDouble tau_d;
        NekDouble tau_v1_minus;
        NekDouble tau_v2_minus;
        NekDouble tau_v_plus;
        NekDouble tau_0;
        NekDouble tau_r;
        NekDouble tau_si;
        NekDouble tau_y_plus;
        NekDouble tau_y_minus;
        NekDouble u_csi;
        NekDouble k1;
        NekDouble k2;
        NekDouble tau_w_minus;
        NekDouble tau_w_plus;

        bool isCF3;

        enum Variants {
            eBR,
            eMBR,
            eMLR1,
            eGP,
            eCF1,
            eCF2a,
            eCF2b,
            eCF2c,
            eCF3a,
            eCF3b
        };
        enum Variants model_variant;

        static std::string lookupIds[];
        static std::string def;
    };

}

#endif
