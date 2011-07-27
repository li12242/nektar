///////////////////////////////////////////////////////////////////////////////
//
// File DriverStandard.cpp
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
// Description: Incompressible Navier Stokes solver
//
///////////////////////////////////////////////////////////////////////////////

#include <Auxiliary/DriverStandard.h>

namespace Nektar

{
    string DriverStandard::className = GetDriverFactory().RegisterCreatorFunction("Standard", DriverStandard::create);

    /**
	 *
     */
    DriverStandard::DriverStandard(LibUtilities::CommSharedPtr                pComm,
                                   LibUtilities::SessionReaderSharedPtr       pSession)
        : Driver(pComm,pSession)
    {
    }
    
    
    /**
     *
     */
    DriverStandard:: ~DriverStandard()
    {
    }
    
    
    /**
     *
     */
    void DriverStandard::v_InitObject()
    {
        try
        {
            ASSERTL0(m_session->DefinesSolverInfo("EqType"),
                     "EqType SolverInfo tag must be defined.");
            std::string vEquation = m_session->GetSolverInfo("EqType");
            if (m_session->DefinesSolverInfo("SolverType"))
            {
                vEquation = m_session->GetSolverInfo("SolverType");
            }
            ASSERTL0(GetEquationSystemFactory().ModuleExists(vEquation),
                     "Solver module '" + vEquation + "' is not defined.\n"
                     "Ensure equation name is correct and module is compiled.\n");

            m_equ = Array<OneD, EquationSystemSharedPtr>(1);
            m_equ[0] = GetEquationSystemFactory().CreateInstance(vEquation, m_comm, m_session);
        }
        catch (int e)
        {
            ASSERTL0(e == -1, "No such class class defined.");
            cout << "An error occured during driver initialisation." << endl;
        }
    }
    
    
    void DriverStandard::v_Execute()
        
    {
        m_equ[0]->DoInitialise();
        m_equ[0]->PrintSummary(cout);
        m_equ[0]->DoSolve();
        m_equ[0]->Output();
        
        // Evaluate and output computation time and solution accuracy.
        // The specific format of the error output is essential for the
        // regression tests to work.
        // Evaluate L2 Error
        for(int i = 0; i < m_equ[0]->GetNvariables(); ++i)
        {
            NekDouble vL2Error = m_equ[0]->L2Error(i,false);
            NekDouble vLinfError = m_equ[0]->LinfError(i);
            if (m_comm->GetRank() == 0)
            {
                cout << "L 2 error (variable " << m_equ[0]->GetVariable(i) << ") : " << vL2Error << endl;
                cout << "L inf error (variable " << m_equ[0]->GetVariable(i) << ") : " << vLinfError << endl;
            }
        }
    }

}



/**
 * $Log $
**/
