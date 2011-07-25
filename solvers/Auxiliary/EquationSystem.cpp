///////////////////////////////////////////////////////////////////////////////
//
// File EquationSystem.cpp
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
// Description: Main wrapper class for Advection Diffusion Reaction Solver
//
///////////////////////////////////////////////////////////////////////////////

#include <string>
using std::string;

#include <LibUtilities/BasicUtils/Equation.h>
#include <SpatialDomains/MeshPartition.h>

#include <MultiRegions/ContField1D.h>
#include <MultiRegions/ContField2D.h>
#include <MultiRegions/ContField3D.h>
#include <MultiRegions/ContField3DHomogeneous1D.h>
#include <MultiRegions/ContField3DHomogeneous2D.h>

#include <Auxiliary/EquationSystem.h>

namespace Nektar
{
    /**
     * @class EquationSystem
     *
     * This class is a base class for all solver implementations. It provides
     * the underlying generic functionality and interface for solving equations.
     *
     * To solve a steady-state equation, create a derived class from this class
     * and reimplement the virtual functions to provide custom implementation
     * for the problem.
     *
     * To solve unsteady problems, derive from the UnsteadySystem class instead
     * which provides general time integration.
     */

    EquationSystemFactory& GetEquationSystemFactory()
    {
        typedef Loki::SingletonHolder<EquationSystemFactory,
            Loki::CreateUsingNew,
            Loki::NoDestroy > Type;
        return Type::Instance();
    }


    /**
     * This constructor is protected as the objects of this class are never
     * instantiated directly.
     * @param   pSession        The session reader holding problem parameters.
     */
    EquationSystem::EquationSystem( LibUtilities::CommSharedPtr& pComm,
                                    LibUtilities::SessionReaderSharedPtr& pSession)
        : m_comm (pComm),
          m_session (pSession)
    {
    }


    /**
     *
     */
    void EquationSystem::v_InitObject()
    {
        m_filename = m_session->GetFilename();
        m_solnType = MultiRegions::eDirectMultiLevelStaticCond;
        // Save the basename of input file name for output details.
        m_sessionName = m_filename;
        m_sessionName = m_sessionName.substr(0,
                                m_sessionName.find_last_of("."));

        // Create communicator
        if (m_session->DefinesSolverInfo("GlobalSysSolve"))
        {
            for (unsigned int i = 0; i < MultiRegions::eSIZE_GlobalSysSolnType; ++i)
            {
                if (m_session->GetSolverInfo("GlobalSysSolve") == MultiRegions::GlobalSysSolnTypeMap[i])
                {
                    m_solnType = (MultiRegions::GlobalSysSolnType)i;
                    break;
                }
                if (i == MultiRegions::eSIZE_GlobalSysSolnType - 1)
                {
                    ASSERTL0(false, "Unknown GlobalSysSolve type in session file.");
                }
            }
        }

        if (m_comm->GetSize() > 1)
        {
            if (m_comm->GetRank() == 0)
            {
                SpatialDomains::MeshPartitionSharedPtr vPartitioner = MemoryManager<SpatialDomains::MeshPartition>::AllocateSharedPtr(m_session);
                vPartitioner->PartitionMesh(m_comm->GetSize());
                vPartitioner->WritePartitions(m_session, m_filename);
            }

            m_comm->Block();

            m_filename = m_filename + "." + boost::lexical_cast<std::string>(m_comm->GetRank());
            m_solnType = MultiRegions::eIterativeFull;
            m_sessionName = m_sessionName.substr(0,m_sessionName.find_last_of("."));
        }

        SpatialDomains::MeshGraph graph;

        // Read the geometry and the expansion information
        m_graph = graph.Read(m_filename);

        m_UseContCoeff = false;

        // Also read and store the boundary conditions
        SpatialDomains::MeshGraph *meshptr = m_graph.get();
        m_boundaryConditions = MemoryManager<SpatialDomains::BoundaryConditions>
            ::AllocateSharedPtr(meshptr);
        m_boundaryConditions->Read(m_filename);

        // Read and store history point data
        m_historyPoints = MemoryManager<SpatialDomains::History>
                                        ::AllocateSharedPtr(meshptr);
        m_historyPoints->Read(m_filename);

        // Set space dimension for use in class
        m_spacedim = m_graph->GetSpaceDimension();

        // Setting parameteres for homogenous problems
        m_HomoDirec       = 0;
        m_useFFT          = false;
        m_HomogeneousType = eNotHomogeneous;

        if(m_boundaryConditions->SolverInfoExists("HOMOGENEOUS"))
        {
            std::string HomoStr = m_boundaryConditions->GetSolverInfo("HOMOGENEOUS");
            m_spacedim          = 3;

            if((HomoStr == "HOMOGENEOUS1D")||(HomoStr == "Homogeneous1D")||
               (HomoStr == "1D")||(HomoStr == "Homo1D"))
            {
                m_HomogeneousType = eHomogeneous1D;
                m_npointsZ        = m_boundaryConditions->GetParameter("HomModesZ");
                m_LhomZ           = m_boundaryConditions->GetParameter("LZ");
                m_HomoDirec       = 1;
            }

            if((HomoStr == "HOMOGENEOUS2D")||(HomoStr == "Homogeneous2D")||
               (HomoStr == "2D")||(HomoStr == "Homo2D"))
            {
                m_HomogeneousType = eHomogeneous2D;
                m_npointsY        = m_boundaryConditions->GetParameter("HomModesY");
                m_LhomY           = m_boundaryConditions->GetParameter("LY");
                m_npointsZ        = m_boundaryConditions->GetParameter("HomModesZ");
                m_LhomZ           = m_boundaryConditions->GetParameter("LZ");
                m_HomoDirec       = 2;
            }

            if((HomoStr == "HOMOGENEOUS3D")||(HomoStr == "Homogeneous3D")||
               (HomoStr == "3D")||(HomoStr == "Homo3D"))
            {
                m_HomogeneousType = eHomogeneous3D;
                m_npointsX        = m_boundaryConditions->GetParameter("HomModesX");
                m_LhomX           = m_boundaryConditions->GetParameter("LX");
                m_npointsY        = m_boundaryConditions->GetParameter("HomModesY");
                m_LhomY           = m_boundaryConditions->GetParameter("LY");
                m_npointsZ        = m_boundaryConditions->GetParameter("HomModesZ");
                m_LhomZ           = m_boundaryConditions->GetParameter("LZ");
                m_HomoDirec       = 3;
            }

            if(m_boundaryConditions->SolverInfoExists("USEFFT"))
            {
                m_useFFT = true;
            }
        }
        else
        {
            m_npointsZ = 1; // set to default value so can use to identify 2d or 3D (homogeneous) expansions
        }

        // Options to determine type of projection from file or
        // directly from constructor
        if(m_boundaryConditions->SolverInfoExists("PROJECTION"))
        {
            std::string ProjectStr
                    = m_boundaryConditions->GetSolverInfo("PROJECTION");

            if((ProjectStr == "Continuous")||(ProjectStr == "Galerkin")||
               (ProjectStr == "CONTINUOUS")||(ProjectStr == "GALERKIN"))
            {
                m_projectionType = MultiRegions::eGalerkin;
            }
            else if(ProjectStr == "DisContinuous")
            {
                m_projectionType = MultiRegions::eDiscontinuousGalerkin;
            }
            else
            {
                ASSERTL0(false,"PROJECTION value not recognised");
            }
        }
        else
        {
            cerr << "Projection type not specified in SOLVERINFO,"
                    "defaulting to continuous Galerkin" << endl;
            m_projectionType = MultiRegions::eGalerkin;
        }

        // Enforce singularity check for some problems
        m_checkIfSystemSingular = v_GetSystemSingularChecks();

        int i;
        int nvariables = m_boundaryConditions->GetNumVariables();
        bool DeclareCoeffPhysArrays = true;

        m_fields   = Array<OneD, MultiRegions::ExpListSharedPtr>(nvariables);
        m_spacedim = m_graph->GetSpaceDimension()+m_HomoDirec;
        m_expdim   = m_graph->GetMeshDimension();

        // Continuous Galerkin projection
        if(m_projectionType == MultiRegions::eGalerkin)
        {
            switch(m_expdim)
            {
                case 1:
                {
                    if(m_HomogeneousType == eHomogeneous2D)
                    {
                        SpatialDomains::MeshGraph1DSharedPtr mesh1D;

                        if(!(mesh1D = boost::dynamic_pointer_cast<
                             SpatialDomains::MeshGraph1D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        const LibUtilities::PointsKey PkeyY(m_npointsY,LibUtilities::eFourierEvenlySpaced);
                        const LibUtilities::BasisKey  BkeyY(LibUtilities::eFourier,m_npointsY,PkeyY);
                        const LibUtilities::PointsKey PkeyZ(m_npointsZ,LibUtilities::eFourierEvenlySpaced);
                        const LibUtilities::BasisKey  BkeyZ(LibUtilities::eFourier,m_npointsZ,PkeyZ);

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions::ContField3DHomogeneous2D>
                                ::AllocateSharedPtr(m_comm,BkeyY,BkeyZ,m_LhomY,m_LhomZ,m_useFFT,*mesh1D,*m_boundaryConditions,i,m_solnType);
                        }
                    }
                    else
                    {
                        SpatialDomains::MeshGraph1DSharedPtr mesh1D;

                        if( !(mesh1D = boost::dynamic_pointer_cast<
                              SpatialDomains::MeshGraph1D>(m_graph)) )
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions::ContField1D>
                                ::AllocateSharedPtr(m_comm,*mesh1D,
                                                    *m_boundaryConditions,i,m_solnType);
                        }

                    }

                    break;
                }
            case 2:
                {
                    if(m_HomogeneousType == eHomogeneous1D)
                    {
                        SpatialDomains::MeshGraph2DSharedPtr mesh2D;

                        if(!(mesh2D = boost::dynamic_pointer_cast<
                             SpatialDomains::MeshGraph2D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        const LibUtilities::PointsKey PkeyZ(m_npointsZ,LibUtilities::eFourierEvenlySpaced);
                        const LibUtilities::BasisKey  BkeyZ(LibUtilities::eFourier,m_npointsZ,PkeyZ);

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions::ContField3DHomogeneous1D>::AllocateSharedPtr(m_comm,BkeyZ,m_LhomZ,m_useFFT,*mesh2D,*m_boundaryConditions,i,m_solnType);
                        }
                    }
                    else
                    {
                        SpatialDomains::MeshGraph2DSharedPtr mesh2D;

                        if(!(mesh2D = boost::dynamic_pointer_cast<
                             SpatialDomains::MeshGraph2D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        i = 0;
                        MultiRegions::ContField2DSharedPtr firstfield =
                            MemoryManager<MultiRegions::ContField2D>
                            ::AllocateSharedPtr(m_comm,*mesh2D,
                                                *m_boundaryConditions,i,m_solnType,
                                                DeclareCoeffPhysArrays,
                                                m_checkIfSystemSingular[0]);

                        firstfield->ReadGlobalOptimizationParameters(m_filename);

                        m_fields[0] = firstfield;
                        for(i = 1 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions::ContField2D>
                                ::AllocateSharedPtr(*firstfield,
                                                    *mesh2D,*m_boundaryConditions,i,
                                                    DeclareCoeffPhysArrays,
                                                    m_checkIfSystemSingular[i]);
                        }
                    }

                    break;
                }
                case 3:
                    {
                        if(m_HomogeneousType == eHomogeneous3D)
                        {
                            ASSERTL0(false,"3D fully periodic problems not implemented yet");
                        }
                        else
                        {
                            SpatialDomains::MeshGraph3DSharedPtr mesh3D;

                            if(!(mesh3D = boost::dynamic_pointer_cast<
                                 SpatialDomains::MeshGraph3D>(m_graph)))
                            {
                                ASSERTL0(false,"Dynamics cast failed");
                            }

                            i = 0;
                            MultiRegions::ContField3DSharedPtr firstfield =
                                MemoryManager<MultiRegions::ContField3D>
                                ::AllocateSharedPtr(m_comm,*mesh3D,
                                                    *m_boundaryConditions,i,m_solnType);

                            firstfield->ReadGlobalOptimizationParameters(m_filename);

                            m_fields[0] = firstfield;
                            for(i = 1 ; i < m_fields.num_elements(); i++)
                            {
                                m_fields[i] = MemoryManager<MultiRegions::ContField3D>
                                    ::AllocateSharedPtr(*firstfield,
                                                        *mesh3D,*m_boundaryConditions,i);
                        }
                        }
                        break;
                    }
            default:
                ASSERTL0(false,"Expansion dimension not recognised");
                break;
            }
        }
        else // Discontinuous Field
        {
            switch(m_expdim)
            {
                case 1:
                {
                    if(m_HomogeneousType == eHomogeneous2D)
                    {
                        SpatialDomains::MeshGraph1DSharedPtr mesh1D;

                        if(!(mesh1D = boost::dynamic_pointer_cast<
                             SpatialDomains::MeshGraph1D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        const LibUtilities::PointsKey PkeyY(m_npointsY,LibUtilities::eFourierEvenlySpaced);
                        const LibUtilities::BasisKey  BkeyY(LibUtilities::eFourier,m_npointsY,PkeyY);
                        const LibUtilities::PointsKey PkeyZ(m_npointsZ,LibUtilities::eFourierEvenlySpaced);
                        const LibUtilities::BasisKey  BkeyZ(LibUtilities::eFourier,m_npointsZ,PkeyZ);

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions::DisContField3DHomogeneous2D>
                                ::AllocateSharedPtr(m_comm,BkeyY,BkeyZ,m_LhomY,m_LhomZ,m_useFFT,*mesh1D,*m_boundaryConditions,i,m_solnType);
                        }
                    }
                    else
                    {
                        SpatialDomains::MeshGraph1DSharedPtr mesh1D;

                        if(!(mesh1D = boost::dynamic_pointer_cast<SpatialDomains
                             ::MeshGraph1D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions
                                ::DisContField1D>::AllocateSharedPtr(m_comm,*mesh1D,
                                                                     *m_boundaryConditions,i,m_solnType);
                        }
                    }

                    break;
                }
            case 2:
                {
                    if(m_HomogeneousType == eHomogeneous1D)
                    {
                        SpatialDomains::MeshGraph2DSharedPtr mesh2D;

                        if(!(mesh2D = boost::dynamic_pointer_cast<
                             SpatialDomains::MeshGraph2D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        const LibUtilities::PointsKey PkeyZ(m_npointsZ,LibUtilities::eFourierEvenlySpaced);
                        const LibUtilities::BasisKey  BkeyZ(LibUtilities::eFourier,m_npointsZ,PkeyZ);

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions::DisContField3DHomogeneous1D>
                                ::AllocateSharedPtr(m_comm,BkeyZ,m_LhomZ,m_useFFT,*mesh2D,*m_boundaryConditions,i,m_solnType);
                        }
                    }
                    else
                    {
                        SpatialDomains::MeshGraph2DSharedPtr mesh2D;

                        if(!(mesh2D = boost::dynamic_pointer_cast<SpatialDomains
                             ::MeshGraph2D>(m_graph)))
                        {
                            ASSERTL0(false,"Dynamics cast failed");
                        }

                        for(i = 0 ; i < m_fields.num_elements(); i++)
                        {
                            m_fields[i] = MemoryManager<MultiRegions
                                ::DisContField2D>::AllocateSharedPtr(m_comm,*mesh2D,
                                                                     *m_boundaryConditions,i,m_solnType);
                        }
                    }

                    break;
                }
            case 3:
                {
                    if(m_HomogeneousType == eHomogeneous3D)
                    {
                        ASSERTL0(false,"3D fully periodic problems not implemented yet");
                    }
                    else
                    {
                        ASSERTL0(false,"3 D not set up");
                    }
                    break;
                }
            default:
                ASSERTL0(false,"Expansion dimension not recognised");
                break;
            }

            // Set up Normals.
            switch(m_expdim)
            {
            case 1:
                    // no need??...
                break;
            case 2:
                {
                    m_traceNormals
                        = Array<OneD, Array<OneD, NekDouble> >(m_spacedim);

                    for(i = 0; i < m_spacedim; ++i)
                    {
                        m_traceNormals[i] = Array<OneD, NekDouble> (m_fields[0]
                                                                    ->GetTrace()->GetNpoints());
                    }

                    m_fields[0]->GetTrace()->GetNormals(m_traceNormals);
                    break;
                }
            case 3:
                ASSERTL0(false,"3 D not set up");
                    break;
            default:
                    ASSERTL0(false,"Expansion dimension not recognised");
                    break;
            }
        }

        // Set Default Parameter
        m_session->LoadParameter("Time", m_time, 0.0);
        m_session->LoadParameter("TimeStep", m_timestep, 0.01);
        m_session->LoadParameter("NumSteps", m_steps, 0);
        m_session->LoadParameter("IO_CheckSteps", m_checksteps, m_steps);
        m_session->LoadParameter("FinTime", m_fintime, 0);
        m_session->LoadParameter("NumQuadPointsError", m_NumQuadPointsError, 0);

        // Read in spatial data
        int nq = m_fields[0]->GetNpoints();
        m_spatialParameters = MemoryManager<SpatialDomains::SpatialParameters>
          ::AllocateSharedPtr(nq);
        m_spatialParameters->Read(m_filename);

        Array<OneD, NekDouble> x(nq), y(nq), z(nq);
        m_fields[0]->GetCoords(x,y,z);
        m_spatialParameters->EvaluateParameters(x,y,z);

        ScanForHistoryPoints();

        if (m_session->DefinesFunction("BodyForce"))
        {
            m_forces  = Array<OneD, MultiRegions::ExpListSharedPtr>(v_GetForceDimension());
            int nq = m_fields[0]->GetNpoints();
            switch(m_expdim)
            {
                case 1:
                    m_forces[0] = MemoryManager<MultiRegions
                            ::DisContField1D>::AllocateSharedPtr
                                 (*boost::static_pointer_cast<MultiRegions::DisContField1D>(m_fields[0]));
                    Vmath::Zero(nq,(m_forces[0]->UpdatePhys()),1);
                    break;
                case 2:
                    if(m_HomogeneousType == eHomogeneous1D)
                    {
                        bool DeclarePlaneSetCoeffsPhys = true;
                        for(int i = 0 ; i < m_forces.num_elements(); i++)
                        {
                            m_forces[i]= MemoryManager<MultiRegions::ExpList3DHomogeneous1D>::AllocateSharedPtr(*boost::static_pointer_cast<MultiRegions::ExpList3DHomogeneous1D>(m_fields[i]),DeclarePlaneSetCoeffsPhys);
                        }
                    }
                    else
                    {
                        for(int i = 0 ; i < m_forces.num_elements(); i++)
                        {
                            m_forces[i] = MemoryManager<MultiRegions
                                    ::ExpList2D>::AllocateSharedPtr
                                     (*boost::static_pointer_cast<MultiRegions::ExpList2D>(m_fields[i]));
                            Vmath::Zero(nq,(m_forces[i]->UpdatePhys()),1);
                        }
                    }
                    break;
                case 3:
                    ASSERTL0(false, "Force function not implemented for 3D.");
            }

            EvaluateFunction(m_forces, "BodyForce");
        }

        // If a tangent vector policy is defined then the local tangent vectors
        // on each element need to be generated.
        if (m_session->DefinesGeometricInfo("TANGENTDIR"))
        {
            m_fields[0]->SetUpTangents();
        }

        // Zero all physical fields initially.
        ZeroPhysFields();
    }

    /**
     *
     */
    EquationSystem::~EquationSystem()
    {

    }


    /**
     *
     */
    void EquationSystem::LoadParameter(std::string name, int &var, int def)
    {
        if(m_boundaryConditions->CheckForParameter(name) == true)
        {
            var = m_boundaryConditions->GetParameter(name);
        }
        else
        {
            var  = def;
        }
    }


    /**
     *
     */
    void EquationSystem::LoadParameter(std::string name, NekDouble &var, NekDouble def)
    {
        if(m_boundaryConditions->CheckForParameter(name) == true)
        {
            var = m_boundaryConditions->GetParameter(name);
        }
        else
        {
            var  = def;
        }
    }


    /**
     * Evaluates a physical function at each quadrature point in the domain.
     *
     * @param   pArray          The array into which to write the values.
     * @param   pEqn            The equation to evaluate.
     */
    void EquationSystem::EvaluateFunction(Array<OneD, NekDouble>& pArray,
                              SpatialDomains::ConstUserDefinedEqnShPtr pEqn,
                              const NekDouble pTime)
    {
        int nq = m_fields[0]->GetNpoints();

        Array<OneD,NekDouble> x0(nq);
        Array<OneD,NekDouble> x1(nq);
        Array<OneD,NekDouble> x2(nq);

        // get the coordinates (assuming all fields have the same
        // discretisation)
        m_fields[0]->GetCoords(x0,x1,x2);

        if (pArray.num_elements() != nq)
        {
            pArray = Array<OneD, NekDouble>(nq);
        }
        for(int i = 0; i < nq; i++)
        {
            pArray[i] = pEqn->Evaluate(x0[i],x1[i],x2[i],pTime);
        }

    }


    /**
     * Evaluates a physical function at each quadrature point in the domain.
     *
     * @param   pArray          The array into which to write the values.
     * @param   pEqn            The equation to evaluate.
     */
    void EquationSystem::EvaluateFunction(
            Array<OneD, Array<OneD, NekDouble> >& pArray,
            std::string pFunctionName,
            const NekDouble pTime)
    {
        int nq = m_fields[0]->GetNpoints();

        Array<OneD,NekDouble> x0(nq);
        Array<OneD,NekDouble> x1(nq);
        Array<OneD,NekDouble> x2(nq);

        // get the coordinates (assuming all fields have the same
        // discretisation)
        m_fields[0]->GetCoords(x0,x1,x2);

        for(int k = 0; k < m_boundaryConditions->GetNumVariables(); ++k)
        {
            ASSERTL0(pArray[k].num_elements() == nq,
                     "Array size does not match field size.");
            std::string vVar = m_boundaryConditions->GetVariable(k);
            ASSERTL0(m_session->DefinesFunction(pFunctionName, vVar),
                     "Variable '" + vVar + "' is not defined for function '"
                     + pFunctionName + "'.");

            LibUtilities::EquationSharedPtr vEqn
                                = m_session->GetFunction(pFunctionName,vVar);
            for(int i = 0; i < nq; i++)
            {
                pArray[k][i] = vEqn->Evaluate(x0[i],x1[i],x2[i],pTime);
            }
        }
    }


    /**
     * Populates a forcing function for each of the dependent variables using
     * the expression provided by the BoundaryConditions object.
     * @param   force           Array of fields to assign forcing.
     */
    void EquationSystem::EvaluateFunction(
                        Array<OneD, MultiRegions::ExpListSharedPtr> &pFields,
                        const std::string& pFunctionName)
    {
        LibUtilities::FunctionType vType = m_session->GetFunctionType(pFunctionName);
        if (vType == LibUtilities::eFunctionTypeFile)
        {
            std::string filename = m_session->GetFunctionFilename(pFunctionName);
            cout << pFunctionName << " from file: " << filename << endl;
            ImportFld(filename, pFields);
        }
        else if (vType == LibUtilities::eFunctionTypeExpression)
        {
            int nq = m_fields[0]->GetNpoints();
            Array<OneD,NekDouble> x0(nq);
            Array<OneD,NekDouble> x1(nq);
            Array<OneD,NekDouble> x2(nq);
            // get the coordinates (assuming all fields have the same
            // discretisation)
            pFields[0]->GetCoords(x0,x1,x2);
            for(int i = 0 ; i < pFields.num_elements(); i++)
            {
                LibUtilities::EquationSharedPtr ffunc
                        = m_session->GetFunction(pFunctionName, i);
                for(int j = 0; j < nq; j++)
                {
                    (pFields[i]->UpdatePhys())[j]
                            = ffunc->Evaluate(x0[j],x1[j],x2[j]);
                }
                pFields[i]->SetPhysState(true);
            }
        }
    }


    /**
     * If boundary conditions are time-dependent, they will be evaluated at the
     * time specified.
     *
     * @param   time            The time at which to evaluate the BCs
     */
    void EquationSystem::SetBoundaryConditions(NekDouble time)
    {
        int nvariables = m_fields.num_elements();
        for (int i = 0; i < nvariables; ++i)
        {
            m_fields[i]->EvaluateBoundaryConditions(time);
        }
    }


     /**
      * Compute the error in the L2-norm
      * @param   field           The field to compare.
      * @param   exactsoln       The exact solution to compare with.
      * @param   Normalised      Normalise L2-error.
      * @returns                 Error in the L2-norm.
      */
     NekDouble EquationSystem::L2Error(int field,
                                const Array<OneD, NekDouble> &exactsoln,
                                bool Normalised)
     {
         NekDouble L2error = -1.0;

         if(m_NumQuadPointsError == 0)
         {
             if(m_fields[field]->GetPhysState() == false)
             {
                 m_fields[field]->BwdTrans(m_fields[field]->GetCoeffs(),
                                           m_fields[field]->UpdatePhys());
             }

             if(exactsoln.num_elements())
             {
                 L2error = m_fields[field]->L2(exactsoln);
             }
             else if (m_session->DefinesFunction("ExactSolution"))
             {
                 Array<OneD, NekDouble> exactsoln(m_fields[field]->GetNpoints());

                 LibUtilities::EquationSharedPtr vEqu
                             = m_session->GetFunction("ExactSolution",field);
                 EvaluateFunction(exactsoln,vEqu,m_time);

                 L2error = m_fields[field]->L2(exactsoln);
             }
             else
             {
                 L2error = m_fields[field]->L2();
             }

             if(Normalised == true)
             {
                 Array<OneD, NekDouble> one(m_fields[field]->GetNpoints(),1.0);

                 NekDouble Vol = m_fields[field]->PhysIntegral(one);
             m_comm->AllReduce(Vol, LibUtilities::ReduceSum);

                 L2error = sqrt(L2error*L2error/Vol);
             }
         }
         else
         {
             Array<OneD,NekDouble> L2INF(2);
             L2INF = ErrorExtraPoints(field);
             L2error = L2INF[0];

         }
         return L2error;
     }


     /**
      * Compute the error in the L_inf-norm
      * @param   field           The field to compare.
      * @param   exactsoln       The exact solution to compare with.
      * @returns                 Error in the L_inft-norm.
      */
     NekDouble EquationSystem::LinfError(int field,
                                  const Array<OneD, NekDouble> &exactsoln)
     {
         NekDouble Linferror = -1.0;

         if(m_NumQuadPointsError == 0)
         {
             if(m_fields[field]->GetPhysState() == false)
             {
                 m_fields[field]->BwdTrans(m_fields[field]->GetCoeffs(),
                                       m_fields[field]->UpdatePhys());
             }

             if(exactsoln.num_elements())
             {
                 Linferror = m_fields[field]->Linf(exactsoln);
             }
             else if (m_session->DefinesFunction("ExactSolution"))
             {
                 Array<OneD, NekDouble> exactsoln(m_fields[field]->GetNpoints());

                 LibUtilities::EquationSharedPtr vEqu
                         = m_session->GetFunction("ExactSolution", field);
                 EvaluateFunction(exactsoln,vEqu,m_time);

                 Linferror = m_fields[field]->Linf(exactsoln);
             }
             else
             {
                 Linferror = 0.0;
             }
         }
         else
         {
             Array<OneD,NekDouble> L2INF(2);
             L2INF = ErrorExtraPoints(field);
             Linferror = L2INF[1];
         }

         return Linferror;
     }

     /**
      * Compute the error in the L2-norm, L-inf for a larger number of Quadrature Points
      * @param   field              The field to compare.
      * @returns                    Error in the L2-norm and L-inf norm.
      */
     Array<OneD,NekDouble> EquationSystem::ErrorExtraPoints(int field)
     {
         SpatialDomains::MeshGraph2DSharedPtr mesh2D;
         mesh2D = boost::dynamic_pointer_cast<SpatialDomains::MeshGraph2D>(m_graph);

         int NumModes = GetNumExpModes();

         Array<OneD,NekDouble> L2INF(2);

         const LibUtilities::PointsKey PkeyT1(m_NumQuadPointsError,LibUtilities::eGaussLobattoLegendre);
         const LibUtilities::PointsKey PkeyT2(m_NumQuadPointsError,LibUtilities::eGaussRadauMAlpha1Beta0);
         const LibUtilities::PointsKey PkeyQ1(m_NumQuadPointsError,LibUtilities::eGaussLobattoLegendre);
         const LibUtilities::PointsKey PkeyQ2(m_NumQuadPointsError,LibUtilities::eGaussLobattoLegendre);
         const LibUtilities::BasisKey  BkeyT1(LibUtilities::eModified_A,NumModes,PkeyT1);
         const LibUtilities::BasisKey  BkeyT2(LibUtilities::eModified_B,NumModes,PkeyT2);
         const LibUtilities::BasisKey  BkeyQ1(LibUtilities::eModified_A,NumModes,PkeyQ1);
         const LibUtilities::BasisKey  BkeyQ2(LibUtilities::eModified_A,NumModes,PkeyQ2);

         MultiRegions::ExpList2DSharedPtr ErrorExp =
         MemoryManager<MultiRegions::ExpList2D>::AllocateSharedPtr(m_comm,BkeyT1,BkeyT2,BkeyQ1,BkeyQ2,*mesh2D);

         int ErrorCoordim = ErrorExp->GetCoordim(0);
         int ErrorNq      = ErrorExp->GetTotPoints();

         Array<OneD,NekDouble> ErrorXc0(ErrorNq,0.0);
         Array<OneD,NekDouble> ErrorXc1(ErrorNq,0.0);
         Array<OneD,NekDouble> ErrorXc2(ErrorNq,0.0);

         switch(ErrorCoordim)
         {
             case 1:
                 ErrorExp->GetCoords(ErrorXc0);
                 break;
             case 2:
                 ErrorExp->GetCoords(ErrorXc0,ErrorXc1);
                 break;
             case 3:
                 ErrorExp->GetCoords(ErrorXc0,ErrorXc1,ErrorXc2);
                 break;
         }
         SpatialDomains::ConstExactSolutionShPtr exSol = m_boundaryConditions->GetExactSolution(field);
         // evaluate exact solution
         Array<OneD,NekDouble> ErrorSol(ErrorNq);
         for(int i = 0; i < ErrorNq; ++i)
         {
             ErrorSol[i] = exSol->Evaluate(ErrorXc0[i],ErrorXc1[i],ErrorXc2[i],m_time);
         }

         // calcualte spectral/hp approximation on the quad points of this new
         // expansion basis
         ErrorExp->BwdTrans_IterPerExp(m_fields[field]->GetCoeffs(),ErrorExp->UpdatePhys());

         L2INF[0]    = ErrorExp->L2  (ErrorSol);
         L2INF[1]    = ErrorExp->Linf(ErrorSol);

         return L2INF;
     }


    /**
     * Set the physical fields based on a restart file, or a function
     * describing the initial condition given in the session.
     * @param   initialtime             Time at which to evaluate the function.
     * @param   dumpInitialConditions   Write the initial condition to file?
     */
    void EquationSystem::v_SetInitialConditions(NekDouble initialtime,
                                         bool dumpInitialConditions)
    {
       cout << "Initial Conditions:" << endl;
       if (m_session->DefinesFunction("InitialConditions"))
       {
           // Check for restart file.
           if (m_session->GetFunctionType("InitialConditions")
                   == LibUtilities::eFunctionTypeFile)
           {
               std::string restartfile
                           = m_session->GetFunctionFilename("InitialConditions");
               cout << "\tRestart file: "<< restartfile << endl;
               ImportFld(restartfile, m_fields);
           }
           else
           {
               int nq = m_fields[0]->GetNpoints();

               Array<OneD,NekDouble> x0(nq);
               Array<OneD,NekDouble> x1(nq);
               Array<OneD,NekDouble> x2(nq);

               // get the coordinates (assuming all fields have the same
               // discretisation)
               m_fields[0]->GetCoords(x0,x1,x2);

               for(int i = 0 ; i < m_fields.num_elements(); i++)
               {
                   LibUtilities::EquationSharedPtr ifunc
                           = m_session->GetFunction("InitialConditions", i);
                   for(int j = 0; j < nq; j++)
                   {
                       (m_fields[i]->UpdatePhys())[j]
                               = ifunc->Evaluate(x0[j],x1[j],x2[j],initialtime);
                   }
                   m_fields[i]->SetPhysState(true);
                   m_fields[i]->FwdTrans_IterPerExp(m_fields[i]->GetPhys(),
                                                    m_fields[i]->UpdateCoeffs());
                   cout << "\tField "<< m_session->GetVariable(i)
                        <<": " << ifunc->GetEquation() << endl;
               }
           }
       }
       else
       {
           int nq = m_fields[0]->GetNpoints();
           for(int i = 0 ; i < m_fields.num_elements(); i++)
           {
               Vmath::Zero(nq, m_fields[i]->UpdatePhys(), 1);
               m_fields[i]->SetPhysState(true);
               m_fields[i]->FwdTrans_IterPerExp(m_fields[i]->GetPhys(),
                                                m_fields[i]->UpdateCoeffs());
               cout << "\tField "<< m_session->GetVariable(i)
                    <<": 0 (default)" << endl;
           }
       }
       if(dumpInitialConditions)
       {
           std::string outname = m_sessionName +"_initial.chk";
           if (m_comm->GetSize() > 1)
           {
               char procout[16] = "";
               sprintf(procout, "%d", m_comm->GetRank());
               outname = outname + "." + procout;
           }

           // dump initial conditions to file
           WriteFld(outname);
       }
   }


    void EquationSystem::v_EvaluateExactSolution(int field,
                    Array<OneD, NekDouble> &outfield,
                    const NekDouble time)
    {
        ASSERTL0 (m_session->DefinesFunction("ExactSolution"),
                    "No ExactSolution provided in session file.");
        ASSERTL0 (outfield.num_elements() == m_fields[field]->GetNpoints(),
                    "ExactSolution array size mismatch.");

        LibUtilities::EquationSharedPtr vEqu
                    = m_session->GetFunction("ExactSolution",field);
        EvaluateFunction(outfield,vEqu,m_time);
    }


    /**
     * By default, nothing needs initialising at the EquationSystem level.
     */
    void EquationSystem::v_DoInitialise()
    {

    }
    
    
    void EquationSystem::InitialiseBaseFlow(Array<OneD, Array<OneD, NekDouble> > &base)
    {
    	base = Array<OneD, Array<OneD, NekDouble> >(m_spacedim);
    	int nq = m_fields[0]->GetNpoints();
        std::string velStr[3] = {"Vx","Vy","Vz"};        
        if(m_session->DefinesSolverInfo("BaseFlowFile"))
        {
        	std::string baseyn= m_session->GetSolverInfo("BaseFlowFile");
        	//capitalise the string baseyn
        	std::transform(baseyn.begin(), baseyn.end(), baseyn.begin()
        		, ::toupper);
        	if(baseyn=="YES")
        	{
        		SetUpBaseFields(m_graph);
        		string basename = m_session->GetFilename();
        		basename= (m_session->GetFilename()).substr(0
        		, basename.find_last_of("."));
        		ImportFldBase(basename + "-Base.fld",m_graph);
        		cout<<"Base flow from file:  "<<basename<<"-Base.fld"<<endl;
        		for(int i=0; i<m_spacedim; ++i)
        		{
        			base[i] = Array<OneD, NekDouble> (nq,0.0);
        			Vmath::Vcopy(nq,m_base[i]->GetPhys(),1,base[i],1);	
        		}
        	}
        	else
                {
                	for(int i = 0; i < m_spacedim; ++i)
                	{	
        		   base[i] = Array<OneD, NekDouble> (nq,0.0);
        		   SpatialDomains::ConstUserDefinedEqnShPtr ifunc
        		   = m_boundaryConditions->GetUserDefinedEqn(velStr[i]);
        		   EvaluateFunction(base[i],ifunc);
        		}
        	}
	  }        
	  else
	  {
        	for(int i = 0; i < m_spacedim; ++i)
        	{	
        		base[i] = Array<OneD, NekDouble> (nq,0.0);
        		SpatialDomains::ConstUserDefinedEqnShPtr ifunc
        		= m_boundaryConditions->GetUserDefinedEqn(velStr[i]);
               		EvaluateFunction(base[i],ifunc);
              	}
           }
    }    	    
    
    void
    EquationSystem::SetUpBaseFields(SpatialDomains::MeshGraphSharedPtr
    	    &mesh)
    {
    	   int i;
       	   //NUM VARIABLES CAN BE DIFFERENT FROM THE DIMENSION OF THE BASE FLOW
    	   m_base =Array<OneD, MultiRegions::ExpListSharedPtr> (m_spacedim);
    	   if (m_projectionType = MultiRegions::eGalerkin)
	   {
	        switch (m_expdim)
	        {
	           case 1:
	           {
                       SpatialDomains::MeshGraph1DSharedPtr mesh1D;
                       if( !(mesh1D = boost::dynamic_pointer_cast<
                        SpatialDomains::MeshGraph1D>(mesh)) )
                       {
                           ASSERTL0(false,"Dynamics cast failed");
                       }
                       for(i = 0 ; i < m_base.num_elements(); i++)
                       {
                           m_base[i] = MemoryManager<MultiRegions::ContField1D>
                                ::AllocateSharedPtr(m_comm,*mesh1D,
                                                    *m_boundaryConditions,i);
                       }
	           }
                   break;
                   case 2:
	           {
                       SpatialDomains::MeshGraph2DSharedPtr mesh2D;
                       if(!(mesh2D = boost::dynamic_pointer_cast<
                         SpatialDomains::MeshGraph2D>(mesh)))
                       {
                           ASSERTL0(false,"Dynamics cast failed");
                       }
                       i = 0;
                       MultiRegions::ContField2DSharedPtr firstbase =
                        MemoryManager<MultiRegions::ContField2D>
                                ::AllocateSharedPtr(m_comm,*mesh2D,
                                                    *m_boundaryConditions,i);
                       m_base[0]=firstbase;
                       for(i = 1 ; i < m_base.num_elements(); i++)
                       {
                            m_base[i] = MemoryManager<MultiRegions::ContField2D>
                                ::AllocateSharedPtr(*firstbase,*mesh2D,
                                                    *m_boundaryConditions,i);
                       }
	           }
                   break;
	           case 3:
	           {
	               SpatialDomains::MeshGraph3DSharedPtr mesh3D;
                       if(!(mesh3D = boost::dynamic_pointer_cast<
                          SpatialDomains::MeshGraph3D>(mesh)))
                       {
                            ASSERTL0(false,"Dynamics cast failed");
                       }
                       MultiRegions::ContField3DSharedPtr firstbase =
                            MemoryManager<MultiRegions::ContField3D>
                                ::AllocateSharedPtr(m_comm,*mesh3D,
                                                    *m_boundaryConditions,i);
                       m_base[0] = firstbase;
                       for(i = 1 ; i < m_base.num_elements(); i++)
                       {
                           m_base[i] = MemoryManager<MultiRegions::ContField3D>
                           ::AllocateSharedPtr(*firstbase,
                                        *mesh3D,*m_boundaryConditions,i);
                       }
	           }
                   break;
                   default:
                     ASSERTL0(false,"Expansion dimension not recognised");
                   break;
	       }
        }
        else
	{
               switch(m_expdim)
               {
                   case 1:
                   {
                       SpatialDomains::MeshGraph1DSharedPtr mesh1D;
                       if(!(mesh1D = boost::dynamic_pointer_cast<SpatialDomains
                         ::MeshGraph1D>(mesh)))
                       {
                           ASSERTL0(false,"Dynamics cast failed");
                       }
                       for(i = 0 ; i < m_base.num_elements(); i++)
                       {
                           m_base[i] = MemoryManager<MultiRegions
                            ::DisContField1D>::AllocateSharedPtr(m_comm,*mesh1D,
                                                             *m_boundaryConditions,i);
                       }
                       break;
                   }
                   case 2:
                   {
                         SpatialDomains::MeshGraph2DSharedPtr mesh2D;
                         if(!(mesh2D = boost::dynamic_pointer_cast<SpatialDomains
                         ::MeshGraph2D>(mesh)))
                         {
                               ASSERTL0(false,"Dynamics cast failed");
                         }
                         for(i = 0 ; i < m_base.num_elements(); i++)
                         {
                                 m_base[i] = MemoryManager<MultiRegions
                                 ::DisContField2D>::AllocateSharedPtr(m_comm,*mesh2D,
                                                             *m_boundaryConditions,i);
                         }
                         break;
                    }
                    case 3:
                         ASSERTL0(false,"3 D not set up");
                    default:
                          ASSERTL0(false,"Expansion dimension not recognised");
                    break;
                    }
	  }
       }    	    
 	
    //Import base flow from file and load in m_base    	
    void EquationSystem::ImportFldBase(std::string pInfile, 
    	    SpatialDomains::MeshGraphSharedPtr pGraph)
    {
    	    std::vector<SpatialDomains::FieldDefinitionsSharedPtr> FieldDef;
    	    std::vector<std::vector<NekDouble>   > FieldData;
    	    pGraph->Import(pInfile, FieldDef,FieldData);
       	    int nvar= m_spacedim;
      	    //copy data to m_velocity
    	    for(int j=0; j <nvar; ++j)
    	    {
    	       for(int i=0; i<FieldDef.size(); ++i)
    	       {
    	    	  bool flag = FieldDef[i]->m_fields[j]
    	             ==m_boundaryConditions->GetVariable(j);
    	             ASSERTL1(flag, (std::string("Order of ") +pInfile
    	             	     + std::string("  data and that defined in "
    	             	     	     "m_boundaryconditions differs")).c_str());   
    	          m_base[j]->ExtractDataToCoeffs(FieldDef[i]
    	            	    , FieldData[i], FieldDef[i]->m_fields[j]);
    	       }
    	    	m_base[j]->BwdTrans(m_base[j]->GetCoeffs(), m_base[j]
    	    		->UpdatePhys());    
   	    }	    
     }	      	    

    /**
     *
     */
    void EquationSystem::v_DoSolve()
    {

    }


    /**
     * By default, there are no further parameters to display.
     */
    void EquationSystem::v_PrintSummary(std::ostream &out)
    {

    }


    /**
     * Write the field data to file. The file is named according to the session
     * name with the extension .fld appended.
     */
    void EquationSystem::v_Output(void)
    {
        std::string outname = m_sessionName + ".fld";
        WriteFld(outname);
    }


    /**
     * Zero the physical fields.
     */
    void EquationSystem::ZeroPhysFields(void)
    {
        for(int i = 0; i < m_fields.num_elements(); i++)
        {
            Vmath::Zero(m_fields[i]->GetNpoints(),m_fields[i]->UpdatePhys(),1);
        }
    }

    /**
     * FwdTrans the m_fields members
     */
    void EquationSystem::FwdTransFields(void)
    {
        for(int i = 0; i < m_fields.num_elements(); i++)
        {
            m_fields[i]->FwdTrans(m_fields[i]->GetPhys(),m_fields[i]->UpdateCoeffs());
            m_fields[i]->SetPhysState(false);
        }
    }

//   void EquationSystem::SetInitialForce(NekDouble initialtime)
//   {
//   }



     /**
      * @todo implement 3D case
      * @todo
      */
//    void EquationSystem::InitialiseForcingFunctions( Array<OneD, MultiRegions::ExpListSharedPtr> &force)
//    {
//    }

    /**
     * Computes the weak Green form of advection terms (without boundary
     * integral), i.e. \f$ (\nabla \phi \cdot F) \f$ where for example
     * \f$ F=uV \f$.
     * @param   F           Fields.
     * @param   outarray    Storage for result.
     *
     * \note Assuming all fields are of the same expansion and order so that we
     * can use the parameters of m_fields[0].
     */
    void EquationSystem::WeakAdvectionGreensDivergenceForm(
                const Array<OneD, Array<OneD, NekDouble> > &F,
                Array<OneD, NekDouble> &outarray)
    {
        // use dimension of Velocity vector to dictate dimension of operation
        int ndim    = F.num_elements();
        int nCoeffs = m_fields[0]->GetNcoeffs();

        Array<OneD, NekDouble> iprod(nCoeffs);
        Vmath::Zero(nCoeffs, outarray, 1);

        for (int i = 0; i < ndim; ++i)
        {
            m_fields[0]->IProductWRTDerivBase(i, F[i], iprod);
            Vmath::Vadd(nCoeffs, iprod, 1, outarray, 1, outarray, 1);
        }
    }


    /**
     * Calculate Inner product of the divergence advection form
     * \f$(\phi, \nabla \cdot F)\f$, where for example \f$ F = uV \f$.
     * @param   F           Fields.
     * @param   outarray    Storage for result.
     */
    void EquationSystem::WeakAdvectionDivergenceForm(
                const Array<OneD, Array<OneD, NekDouble> > &F,
                Array<OneD, NekDouble> &outarray)
    {
        // use dimension of Velocity vector to dictate dimension of operation
        int ndim       = F.num_elements();
        int nPointsTot = m_fields[0]->GetNpoints();
        Array<OneD, NekDouble> tmp(nPointsTot);
        Array<OneD, NekDouble> div(nPointsTot, 0.0);

        // Evaluate the divergence
        for(int i = 0; i < ndim; ++i)
        {
            //m_fields[0]->PhysDeriv(i,F[i],tmp);
            m_fields[0]->PhysDeriv(MultiRegions::DirCartesianMap[i],F[i],tmp);
            Vmath::Vadd(nPointsTot, tmp, 1, div, 1, div, 1);
        }

        m_fields[0]->IProductWRTBase(div, outarray);
    }


    /**
     * Calculate Inner product of the divergence advection form
     * \f$ (\phi, V\cdot \nabla u) \f$
     * @param   V           Fields.
     * @param   u           Fields.
     * @param   outarray    Storage for result.
     */
    void EquationSystem::WeakAdvectionNonConservativeForm(
                const Array<OneD, Array<OneD, NekDouble> > &V,
                const Array<OneD, const NekDouble> &u,
                Array<OneD, NekDouble> &outarray)
    {
        // use dimension of Velocity vector to dictate dimension of operation
        int ndim       = V.num_elements();

        int nPointsTot = m_fields[0]->GetNpoints();
        Array<OneD, NekDouble> tmp(nPointsTot);
        Array<OneD, NekDouble> wk(ndim * nPointsTot, 0.0);

        AdvectionNonConservativeForm(V, u, tmp, wk);

        m_fields[0]->IProductWRTBase_IterPerExp(tmp, outarray);
    }


    /**
     * Calculate the inner product \f$ V\cdot \nabla u \f$
     * @param   V           Fields.
     * @param   u           Fields.
     * @param   outarray    Storage for result.
     * @param   wk          Workspace.
     */
    void EquationSystem::AdvectionNonConservativeForm(
                const Array<OneD, Array<OneD, NekDouble> > &V,
                const Array<OneD, const NekDouble> &u,
                Array<OneD, NekDouble> &outarray,
                Array<OneD, NekDouble> &wk)
    {
        // use dimension of Velocity vector to dictate dimension of operation
        int ndim       = V.num_elements();
        //int ndim = m_expdim;

        // ToDo: here we should add a check that V has right dimension

        int nPointsTot = m_fields[0]->GetNpoints();
        Array<OneD, NekDouble> grad0,grad1,grad2;

        // check to see if wk space is defined
        if (wk.num_elements())
        {
            grad0 = wk;
        }
        else
        {
            grad0 = Array<OneD, NekDouble> (nPointsTot);
        }

        // Evaluate V\cdot Grad(u)
        switch(ndim)
        {
            case 1:
                m_fields[0]->PhysDeriv(u,grad0);
                Vmath::Vmul(nPointsTot,grad0,1,V[0],1,outarray,1);
                break;
            case 2:
                grad1 = Array<OneD, NekDouble> (nPointsTot);
                m_fields[0]->PhysDeriv(u,grad0,grad1);
                Vmath::Vmul (nPointsTot,grad0,1,V[0],1,outarray,1);
                Vmath::Vvtvp(nPointsTot,grad1,1,V[1],1,outarray,1,outarray,1);
                break;
            case 3:
                grad1 = Array<OneD, NekDouble> (nPointsTot);
                grad2 = Array<OneD, NekDouble> (nPointsTot);
                m_fields[0]->PhysDeriv(u,grad0,grad1,grad2,m_UseContCoeff);
                Vmath::Vmul (nPointsTot,grad0,1,V[0],1,outarray,1);
                Vmath::Vvtvp(nPointsTot,grad1,1,V[1],1,outarray,1,outarray,1);
                Vmath::Vvtvp(nPointsTot,grad2,1,V[2],1,outarray,1,outarray,1);
                break;
            default:
                ASSERTL0(false,"dimension unknown");
        }
    }

    /*
     * Calculate weak DG advection in the form
     * \f$ \langle\phi, \hat{F}\cdot n\rangle - (\nabla \phi \cdot F) \f$
     * @param   InField         Fields.
     * @param   OutField        Storage for result.
     * @param   NumericalFluxIncludesNormal     Default: true.
     * @param   InFieldIsPhysSpace              Default: false.
     * @param   nvariables      Number of fields.
     */
    void EquationSystem::WeakDGAdvection(
                const Array<OneD, Array<OneD, NekDouble> >& InField,
                Array<OneD, Array<OneD, NekDouble> >& OutField,
                bool NumericalFluxIncludesNormal,
                bool InFieldIsInPhysSpace,
                int nvariables)
    {
        int i;
        int nVelDim         = m_spacedim;
        int nPointsTot      = GetNpoints();
        int ncoeffs         = GetNcoeffs();
        int nTracePointsTot = GetTraceNpoints();

        if (!nvariables)
        {
            nvariables      = m_fields.num_elements();
        }

        Array<OneD, Array<OneD, NekDouble> > fluxvector(nVelDim);
        Array<OneD, Array<OneD, NekDouble> > physfield (nvariables);

        for(i = 0; i < nVelDim; ++i)
        {
            fluxvector[i]    = Array<OneD, NekDouble>(nPointsTot);
        }

        // Get the variables in physical space
        // already in physical space
        if(InFieldIsInPhysSpace == true)
        {
            for(i = 0; i < nvariables; ++i)
            {
                physfield[i] = InField[i];
            }
        }
        // otherwise do a backward transformation
        else
        {
            for(i = 0; i < nvariables; ++i)
            {
                // Could make this point to m_fields[i]->UpdatePhys();
                physfield[i] = Array<OneD, NekDouble>(nPointsTot);
                m_fields[i]->BwdTrans(InField[i],physfield[i]);
            }
        }

        // Get the advection part (without numerical flux)
        for(i = 0; i < nvariables; ++i)
        {
            // Get the ith component of the  flux vector in (physical space)
            GetFluxVector(i, physfield, fluxvector);

            // Calculate the i^th value of (\grad_i \phi, F)
            WeakAdvectionGreensDivergenceForm(fluxvector,OutField[i]);
        }

        // Get the numerical flux and add to the modal coeffs
        // if the NumericalFluxs function already includes the
        // normal in the output
        if (NumericalFluxIncludesNormal == true)
        {
            Array<OneD, Array<OneD, NekDouble> > numflux   (nvariables);

            for(i = 0; i < nvariables; ++i)
            {
                numflux[i]   = Array<OneD, NekDouble>(nTracePointsTot);
            }

            // Evaluate numerical flux in physical space which may in
            // general couple all component of vectors
            NumericalFlux(physfield, numflux);

            // Evaulate  <\phi, \hat{F}\cdot n> - OutField[i]
            for(i = 0; i < nvariables; ++i)
            {
                Vmath::Neg(ncoeffs,OutField[i],1);
                m_fields[i]->AddTraceIntegral(numflux[i],OutField[i]);
                m_fields[i]->SetPhysState(false);
            }
        }
        // if the NumericalFlux function does not include the
        // normal in the output
        else
        {
            Array<OneD, Array<OneD, NekDouble> > numfluxX   (nvariables);
            Array<OneD, Array<OneD, NekDouble> > numfluxY   (nvariables);

            for(i = 0; i < nvariables; ++i)
            {
                numfluxX[i]   = Array<OneD, NekDouble>(nTracePointsTot);
                numfluxY[i]   = Array<OneD, NekDouble>(nTracePointsTot);
            }

            // Evaluate numerical flux in physical space which may in
            // general couple all component of vectors
            NumericalFlux(physfield, numfluxX, numfluxY);

            // Evaulate  <\phi, \hat{F}\cdot n> - OutField[i]
            for(i = 0; i < nvariables; ++i)
            {
                Vmath::Neg(ncoeffs,OutField[i],1);
                m_fields[i]->AddTraceIntegral(numfluxX[i], numfluxY[i],
                                              OutField[i]);
                m_fields[i]->SetPhysState(false);
            }
        }
    }


    /**
     * Calculate weak DG Diffusion in the LDG form
     * \f$ \langle\psi, \hat{u}\cdot n\rangle
     * - \langle\nabla\psi \cdot u\rangle
     *  \langle\phi, \hat{q}\cdot n\rangle - (\nabla \phi \cdot q) \rangle \f$
     */
    void EquationSystem::WeakDGDiffusion(
            const Array<OneD, Array<OneD, NekDouble> >& InField,
            Array<OneD, Array<OneD, NekDouble> >& OutField,
            bool NumericalFluxIncludesNormal,
            bool InFieldIsInPhysSpace)
    {
        int i,j,k;
        int nPointsTot      = GetNpoints();
        int ncoeffs         = GetNcoeffs();
        int nTracePointsTot = GetTraceNpoints();
        int nvariables = m_fields.num_elements();
        int nqvar = 2;

        Array<OneD, NekDouble>  qcoeffs (ncoeffs);
        Array<OneD, NekDouble>  temp (ncoeffs);

        Array<OneD, Array<OneD, NekDouble> > fluxvector (m_spacedim);
        Array<OneD, Array<OneD, NekDouble> > ufield (nvariables);

        Array<OneD, Array<OneD, Array<OneD, NekDouble> > >  flux   (nqvar);
        Array<OneD, Array<OneD, Array<OneD, NekDouble> > >  qfield  (nqvar);

        for(j = 0; j < nqvar; ++j)
        {
            qfield[j] = Array<OneD, Array<OneD, NekDouble> >(nqvar);
            flux[j] = Array<OneD, Array<OneD, NekDouble> >(nqvar);

            for(i = 0; i< nvariables; ++i)
            {
                ufield[i] = Array<OneD, NekDouble>(nPointsTot,0.0);

                qfield[j][i]  = Array<OneD, NekDouble>(nPointsTot,0.0);
                flux[j][i] = Array<OneD, NekDouble>(nTracePointsTot,0.0);
            }
        }

        for(k = 0; k < m_spacedim; ++k)
        {
            fluxvector[k] = Array<OneD, NekDouble>(nPointsTot,0.0);
        }

        // Get the variables in physical space
        // already in physical space
        if(InFieldIsInPhysSpace == true)
        {
            for(i = 0; i < nvariables; ++i)
            {
                ufield[i] = InField[i];
            }
        }
        // otherwise do a backward transformation
        else
        {
            for(i = 0; i < nvariables; ++i)
            {
                // Could make this point to m_fields[i]->UpdatePhys();
                ufield[i] = Array<OneD, NekDouble>(nPointsTot);
                m_fields[i]->BwdTrans(InField[i],ufield[i]);
            }
        }

        // ##########################################################
        // Compute q_{\eta} and q_{\xi} from su
        // Obtain Numerical Fluxes
        // ##########################################################
        NumFluxforScalar(ufield, flux);

        for(j = 0; j < nqvar; ++j)
        {
            for(i = 0; i < nvariables; ++i)
            {
                // Get the ith component of the  flux vector in (physical space)
                // fluxvector = m_tanbasis * u, where m_tanbasis = 2 by
                // m_spacedim by nPointsTot
                if(m_tanbasis.num_elements())
                {
                    for (k = 0; k < m_spacedim; ++k)
                    {
                        Vmath::Vmul(nPointsTot, m_tanbasis[j][k], 1, ufield[i],
                                    1, fluxvector[k], 1);
                    }
                }
                else
                {
                    GetFluxVector(i, j, ufield, fluxvector);
                }

                // Calculate the i^th value of (\grad_i \phi, F)
                WeakAdvectionGreensDivergenceForm(fluxvector, qcoeffs);

                Vmath::Neg(ncoeffs,qcoeffs,1);
                m_fields[i]->AddTraceIntegral(flux[j][i], qcoeffs);
                m_fields[i]->SetPhysState(false);

                // Add weighted mass matrix = M ( \nabla \cdot Tanbasis )
                if(m_gradtan.num_elements())
                {
                    MultiRegions::GlobalMatrixKey key(StdRegions::eMass,
                                                        m_gradtan[j]);
                    m_fields[i]->MultiRegions::ExpList::GeneralMatrixOp(key,
                                                        InField[i], temp);
                    Vmath::Svtvp(ncoeffs, -1.0, temp, 1, qcoeffs, 1,
                                                        qcoeffs, 1);
                }

                 //Multiply by the inverse of mass matrix
                m_fields[i]->MultiplyByElmtInvMass(qcoeffs, qcoeffs);

                // Back to physical space
                m_fields[i]->BwdTrans(qcoeffs, qfield[j][i]);
            }
        }


        // ##########################################################
        //   Compute u from q_{\eta} and q_{\xi}
        // ##########################################################

        // Obtain Numerical Fluxes
        NumFluxforVector(ufield, qfield, flux[0]);

        for (i = 0; i < nvariables; ++i)
        {
            // L = L(tan_eta) q_eta + L(tan_xi) q_xi
            OutField[i] = Array<OneD, NekDouble>(ncoeffs, 0.0);
            temp = Array<OneD, NekDouble>(ncoeffs, 0.0);

            if(m_tanbasis.num_elements())
            {
                for(j = 0; j < nqvar; ++j)
                {
                    for (k = 0; k < m_spacedim; ++k)
                    {
                        Vmath::Vmul(nPointsTot, m_tanbasis[j][k], 1,
                                    qfield[j][i], 1, fluxvector[k], 1);
                    }

                    WeakAdvectionGreensDivergenceForm(fluxvector, temp);
                    Vmath::Vadd(ncoeffs, temp, 1, OutField[i], 1,
                                                    OutField[i], 1);
                }
            }
            else
            {
                for (k = 0; k < m_spacedim; ++k)
                {
                    Vmath::Vcopy(nPointsTot, qfield[k][i], 1, fluxvector[k], 1);
                }

                WeakAdvectionGreensDivergenceForm(fluxvector, OutField[i]);
            }

            // Evaulate  <\phi, \hat{F}\cdot n> - OutField[i]
            Vmath::Neg(ncoeffs,OutField[i],1);
            m_fields[i]->AddTraceIntegral(flux[0][i], OutField[i]);
            m_fields[i]->SetPhysState(false);
        }
    }


    /**
     * Write the n-th checkpoint file.
     * @param   n           The index of the checkpoint file.
     */
    void EquationSystem::Checkpoint_Output(const int n)
    {
        char chkout[16] = "";
        char procout[16] = "";

        sprintf(chkout, "%d", n);
        std::string outname = m_sessionName +"_" + chkout + ".chk";

        if (m_comm->GetSize() > 1)
        {
            sprintf(procout, "%d", m_comm->GetRank());
            outname = outname + "." + procout;
        }
        WriteFld(outname);
    }

    /**
     * Write the n-th checkpoint file.
     * @param   n           The index of the checkpoint file.
     */
    void EquationSystem::Checkpoint_Output(const int n, MultiRegions::ExpListSharedPtr &field, Array< OneD, Array<OneD, NekDouble> > &fieldcoeffs, Array<OneD, std::string> &variables)
    {
        char chkout[16] = "";
        sprintf(chkout, "%d", n);
        std::string outname = m_sessionName +"_" + chkout + ".chk";
        WriteFld(outname, field, fieldcoeffs, variables);
    }


    /**
     * Writes the field data to a file with the given filename.
     * @param   outname     Filename to write to.
     */
    void EquationSystem::WriteFld(std::string &outname)
    {
        Array<OneD, Array<OneD, NekDouble> > fieldcoeffs(m_fields.num_elements());
        Array<OneD, std::string>  variables(m_fields.num_elements());

        for(int i = 0; i < m_fields.num_elements(); ++i)
        {
            if (m_fields[i]->GetPhysState()==true)
            {
                m_fields[i]->FwdTrans_IterPerExp(m_fields[i]->GetPhys(),m_fields[i]->UpdateCoeffs());
            }
            fieldcoeffs[i] = m_fields[i]->UpdateCoeffs();
            variables[i] = m_boundaryConditions->GetVariable(i);
        }

        WriteFld(outname, m_fields[0], fieldcoeffs, variables);

    }


    /**
     * Writes the field data to a file with the given filename.
     * @param   outname     Filename to write to.
     * @param   field       ExpList on which data is based
     * @param fieldcoeffs   An array of array of expansion coefficients
     * @param  variables    An array of variable names
     */
    void EquationSystem::WriteFld(std::string &outname, MultiRegions::ExpListSharedPtr &field, Array<OneD, Array<OneD, NekDouble> > &fieldcoeffs, Array<OneD, std::string> &variables)
    {

        std::vector<SpatialDomains::FieldDefinitionsSharedPtr> FieldDef
            = field->GetFieldDefinitions();
        std::vector<std::vector<NekDouble> > FieldData(FieldDef.size());

        // copy Data into FieldData and set variable
        for(int j = 0; j < fieldcoeffs.num_elements(); ++j)
        {
            for(int i = 0; i < FieldDef.size(); ++i)
            {
                // Could do a search here to find correct variable
                FieldDef[i]->m_fields.push_back(variables[j]);
                field->AppendFieldData(FieldDef[i], FieldData[i], fieldcoeffs[j]);
            }
        }

        m_graph->Write(outname,FieldDef,FieldData);

    }


    /**
     * Import field from infile and load into \a m_fields. This routine will
     * also perform a \a BwdTrans to ensure data is in both the physical and
     * coefficient storage.
     * @param   infile          Filename to read.
     */
    void EquationSystem::ImportFld(std::string &infile, Array<OneD, MultiRegions::ExpListSharedPtr> &pFields)
    {
        std::vector<SpatialDomains::FieldDefinitionsSharedPtr> FieldDef;
        std::vector<std::vector<NekDouble> > FieldData;

        m_graph->Import(infile,FieldDef,FieldData);

        // copy FieldData into m_fields
        for(int j = 0; j < pFields.num_elements(); ++j)
        {
            for(int i = 0; i < FieldDef.size(); ++i)
            {
                ASSERTL1(FieldDef[i]->m_fields[j] == m_session->GetVariable(j),
                         std::string("Order of ") + infile
                         + std::string(" data and that defined in "
                                       "m_boundaryconditions differs"));

                pFields[j]->ExtractDataToCoeffs(FieldDef[i], FieldData[i],
                                                 FieldDef[i]->m_fields[j]);
            }
            pFields[j]->BwdTrans(pFields[j]->GetCoeffs(),
                                 pFields[j]->UpdatePhys());
        }
    }


    /**
     * Write data to file in Tecplot format?
     * @param   n           Checkpoint index.
     * @param   name        Additional name (appended to session name).
     * @param   inarray     Field data to write out.
     * @param   IsInPhysicalSpace   Indicates if field data is in phys space.
     */
    void EquationSystem::Array_Output(const int n, std::string name,
                               const Array<OneD, const NekDouble>&inarray,
                               bool IsInPhysicalSpace)
    {
        int nq = m_fields[0]->GetTotPoints();

        Array<OneD, NekDouble> tmp(nq);

        // save values
        Vmath::Vcopy(nq, m_fields[0]->GetPhys(), 1, tmp, 1);

        // put inarray in m_phys
        if (IsInPhysicalSpace == false)
        {
            m_fields[0]->BwdTrans(inarray,(m_fields[0]->UpdatePhys()));
        }
        else
        {
            Vmath::Vcopy(nq,inarray,1,(m_fields[0]->UpdatePhys()),1);
        }

        char chkout[16] = "";
        sprintf(chkout, "%d", n);
        std::string outname = m_sessionName +"_" + name + "_" + chkout + ".chk";
        ofstream outfile(outname.c_str());
        m_fields[0]->WriteToFile(outfile,eTecplot);

        // copy back the original values
        Vmath::Vcopy(nq,tmp,1,m_fields[0]->UpdatePhys(),1);
    }

    /**
     * Write data to file in Tecplot format
     * @param   n                   Checkpoint index.
     * @param   name                Additional name (appended to session name).
     * @param   IsInPhysicalSpace   Indicates if field data is in phys space.
     */
    void EquationSystem::WriteTecplotFile(const int n, std::string name, bool IsInPhysicalSpace)
    {
        int nq = m_fields[0]->GetTotPoints();

        std::string var = "";
        for(int j = 0; j < m_fields.num_elements(); ++j)
        {
            var = var + ", " + m_boundaryConditions->GetVariable(j);
        }

        char chkout[16] = "";
        sprintf(chkout, "%d", n);
        std::string outname = m_sessionName + "_" + name + "_" + chkout + ".dat";
        ofstream outfile(outname.c_str());

        // put inarray in m_phys
        if (IsInPhysicalSpace == false)
        {
            for(int i = 0; i < m_fields.num_elements(); ++i)
            {
                m_fields[i]->BwdTrans(m_fields[i]->GetCoeffs(),m_fields[i]->UpdatePhys());
            }
        }

        m_fields[0]->WriteTecplotHeader(outfile,var);

        for(int i = 0; i < m_fields[0]->GetExpSize(); ++i)
        {
            m_fields[0]->WriteTecplotZone(outfile,i);
            for(int j = 0; j < m_fields.num_elements(); ++j)
            {
                m_fields[j]->WriteTecplotField(outfile,i);
            }
        }
    }

    /**
     *
     */
    void EquationSystem::ScanForHistoryPoints()
    {
        m_historyList.clear();
        Array<OneD, NekDouble> gloCoord(3,0.0);
        for (int i = 0; i < m_historyPoints->GetNumHistoryPoints(); ++i) {
            SpatialDomains::VertexComponentSharedPtr vtx = m_historyPoints->GetHistoryPoint(i);
            vtx->GetCoords(gloCoord[0], gloCoord[1], gloCoord[2]);

            int eId = m_fields[0]->GetExpIndex(gloCoord);

            m_historyList.push_back(
                std::pair<SpatialDomains::VertexComponentSharedPtr, int>(vtx, eId));
        }
    }

    /**
     * @todo Efficiency improvement required. PutPhysInToElmtExp needs to be
     * called only once per field.
     */
    void EquationSystem::WriteHistoryData (std::ostream &out)
    {
        int numPoints = m_historyList.size();
        int numFields = m_fields.num_elements();

        vector<NekDouble> data(numPoints*numFields);
        int k;
        Array<OneD, NekDouble> gloCoord(3,0.0);
        std::list<pair<SpatialDomains::VertexComponentSharedPtr, int> >::iterator x;

        // Pull out data values field by field
        for (int j = 0; j < m_fields.num_elements(); ++j)
        {
            m_fields[j]->BwdTrans(m_fields[j]->GetCoeffs(),m_fields[j]->UpdatePhys());
            m_fields[j]->PutPhysInToElmtExp();
            for (k = 0, x = m_historyList.begin(); x != m_historyList.end(); ++x, ++k)
            {
                (*x).first->GetCoords(gloCoord[0], gloCoord[1], gloCoord[2]);
                data[k*numFields+j] = m_fields[j]->GetExp((*x).second)->PhysEvaluate(gloCoord);
            }
        }

        // Write data values point by point
        for (k = 0, x = m_historyList.begin(); x != m_historyList.end(); ++x, ++k)
        {
            (*x).first->GetCoords(gloCoord[0], gloCoord[1], gloCoord[2]);
            out.width(8);
            out << m_time;
            out.width(8);
            out << gloCoord[0];
            out.width(8);
            out << gloCoord[1];
            out.width(8);
            out << gloCoord[2];
            for (int j = 0; j < numFields; ++j)
            {
                //m_fields[j]->PutPhysInToElmtExp();
                out.width(14);
                //out << m_fields[j]->GetExp((*x).second)->PhysEvaluate(gloCoord);
                out << data[k*numFields+j];
            }
            out << endl;
        }
    }


    /**
     * Write out a summary of the session and timestepping to the given output
     * stream.
     * @param   out         Output stream to write data to.
     */
    void EquationSystem::Summary(std::ostream &out)
    {
        SessionSummary(out);
        TimeParamSummary(out);
    }


    /**
     * Write out a summary of the session data.
     * @param   out         Output stream to write data to.
     */
    void EquationSystem::SessionSummary(std::ostream &out)
    {

        if(m_HomogeneousType == eHomogeneous1D)
        {
            out << "\tQuasi-3D        : " << "Homogeneous in z-direction" << endl;
            out << "\tSession Name    : " << m_sessionName << endl;
            out << "\tExpansion Dim.  : " << m_expdim+1 << endl;
            out << "\tSpatial   Dim.  : " << m_spacedim << endl;
            out << "\t2D Exp. Order   : " << m_fields[0]->EvalBasisNumModesMax()<< endl;
            out << "\tN.Hom. Modes    : " << m_npointsZ << endl;
            out << "\tHom. length (LZ): " << m_LhomZ << endl;
            if(m_useFFT)
            {
              out << "\tUsing FFTW " << endl;
            }
            else
            {
              out << "\tUsing MVM "  << endl;
            }

        }
        else if(m_HomogeneousType == eHomogeneous2D)
        {
            out << "\tQuasi-3D        : " << "Homogeneous in yz-plane" << endl;
            out << "\tSession Name    : " << m_sessionName << endl;
            out << "\tExpansion Dim.  : " << m_expdim+2 << endl;
            out << "\tSpatial   Dim.  : " << m_spacedim << endl;
            out << "\t1D Exp. Order   : " << m_fields[0]->EvalBasisNumModesMax()<< endl;
            out << "\tN.Hom. Modes (y): " << m_npointsY << endl;
            out << "\tN.Hom. Modes (z): " << m_npointsZ << endl;
            out << "\tHom. length (LY): " << m_LhomY << endl;
            out << "\tHom. length (LZ): " << m_LhomZ << endl;
            if(m_useFFT)
            {
                out << "\tUsing FFTW " << endl;
            }
            else
            {
                out << "\tUsing MVM "  << endl;
            }

        }
        else
        {
            out << "\tSession Name    : " << m_sessionName << endl;
            out << "\tExpansion Dim.  : " << m_expdim << endl;
            out << "\tSpatial   Dim.  : " << m_spacedim << endl;
            out << "\tMax Exp. Order  : " << m_fields[0]->EvalBasisNumModesMax()<< endl;
        }
        if(m_projectionType == MultiRegions::eGalerkin)
        {
            out << "\tProjection Type : Galerkin" <<endl;
        }
        else
        {
            out << "\tProjection Type : Discontinuous Galerkin" <<endl;
        }
    }

    /**
     * Write out a summary of the time parameters.
     * @param   out         Output stream to write to.
     */
    void EquationSystem::TimeParamSummary(std::ostream &out)
    {
        out << "\tTime Step       : " << m_timestep << endl;
        out << "\tNo. of Steps    : " << m_steps << endl;
        out << "\tCheckpoints     : " << m_checksteps << " steps" << endl;
//        out << "\tInformation     : " << m_infosteps << " steps" << endl;
    }


    /**
     * Performs a case-insensitive string comparison (from web).
     * @param   s1          First string to compare.
     * @param   s2          Second string to compare.
     * @returns             0 if the strings match.
     */
    int EquationSystem::NoCaseStringCompare(const string & s1, const string& s2)
    {
        //if (s1.size() < s2.size()) return -1;
        //if (s1.size() > s2.size()) return 1;

        string::const_iterator it1=s1.begin();
        string::const_iterator it2=s2.begin();

        //stop when either string's end has been reached
        while ( (it1!=s1.end()) && (it2!=s2.end()) )
        {
            if(::toupper(*it1) != ::toupper(*it2)) //letters differ?
            {
                // return -1 to indicate smaller than, 1 otherwise
                return (::toupper(*it1)  < ::toupper(*it2)) ? -1 : 1;
            }

            //proceed to the next character in each string
            ++it1;
            ++it2;
        }

        size_t size1=s1.size();
        size_t size2=s2.size();// cache lengths

        //return -1,0 or 1 according to strings' lengths
        if (size1==size2)
        {
            return 0;
        }

        return (size1 < size2) ? -1 : 1;
    }

    Array<OneD, bool> EquationSystem::v_GetSystemSingularChecks()
    {
        return Array<OneD, bool>(m_boundaryConditions->GetNumVariables(), false);
    }

    int EquationSystem::v_GetForceDimension()
    {
        return 0;
    }

    void EquationSystem::v_GetFluxVector(const int i, Array<OneD,
                        Array<OneD, NekDouble> >&physfield,
                        Array<OneD, Array<OneD, NekDouble> >&flux)
    {
        ASSERTL0(false, "v_GetFluxVector: This function is not valid "
                        "for the Base class");
    }

    void EquationSystem::v_GetFluxVector(const int i, const int j,
                        Array<OneD, Array<OneD, NekDouble> >&physfield,
                        Array<OneD, Array<OneD, NekDouble> >&flux)
    {
        ASSERTL0(false, "v_GetqFluxVector: This function is not valid "
                        "for the Base class");
    }

    void EquationSystem::v_GetFluxVector(const int i, Array<OneD,
                        Array<OneD, NekDouble> >&physfield,
                        Array<OneD, Array<OneD, NekDouble> >&fluxX,
                        Array<OneD, Array<OneD, NekDouble> > &fluxY)
    {
        ASSERTL0(false, "v_GetFluxVector: This function is not valid "
                        "for the Base class");
    }

    void EquationSystem::v_NumericalFlux(
                        Array<OneD, Array<OneD, NekDouble> > &physfield,
                        Array<OneD, Array<OneD, NekDouble> > &numflux)
    {
        ASSERTL0(false, "v_NumericalFlux: This function is not valid "
                        "for the Base class");
    }

    void EquationSystem::v_NumericalFlux(
                        Array<OneD, Array<OneD, NekDouble> > &physfield,
                        Array<OneD, Array<OneD, NekDouble> > &numfluxX,
                        Array<OneD, Array<OneD, NekDouble> > &numfluxY )
    {
        ASSERTL0(false, "v_NumericalFlux: This function is not valid "
                        "for the Base class");
    }

    void EquationSystem::v_NumFluxforScalar(
                Array<OneD, Array<OneD, NekDouble> > &ufield,
                Array<OneD, Array<OneD, Array<OneD, NekDouble> > > &uflux)
    {
        ASSERTL0(false, "v_NumFluxforScalar: This function is not valid "
                        "for the Base class");
    }

    void EquationSystem::v_NumFluxforVector(
                Array<OneD, Array<OneD, NekDouble> > &ufield,
                Array<OneD, Array<OneD, Array<OneD, NekDouble> > >  &qfield,
                Array<OneD, Array<OneD, NekDouble > >  &qflux)
    {
        ASSERTL0(false, "v_NumFluxforVector: This function is not valid "
                        "for the Base class");
    }

}