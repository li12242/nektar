#ifndef NEKTAR_SOLVERS_ADRSOLVER_EQUATIONSYSTEMS_UNSTEADYDIFFUSION_H
#define NEKTAR_SOLVERS_ADRSOLVER_EQUATIONSYSTEMS_UNSTEADYDIFFUSION_H

#include <ADRSolver/EquationSystems/UnsteadySystem.h>

namespace Nektar
{
    class UnsteadyDiffusion : public UnsteadySystem
    {
    public:
        /// Creates an instance of this class
        static EquationSystemSharedPtr create(SessionReaderSharedPtr& pSession) {
            return MemoryManager<UnsteadyDiffusion>::AllocateSharedPtr(pSession);
        }
        /// Name of class
        static std::string className;

        UnsteadyDiffusion(SessionReaderSharedPtr& pSession);

        virtual ~UnsteadyDiffusion();

    protected:
        void DoOdeRhs(const Array<OneD, const  Array<OneD, NekDouble> >&inarray,
                          Array<OneD,        Array<OneD, NekDouble> >&outarray,
                    const NekDouble time);


        void DoOdeProjection(const Array<OneD,  const  Array<OneD, NekDouble> > &inarray,
                          Array<OneD,  Array<OneD, NekDouble> > &outarray,
                          const NekDouble time);

        virtual void DoImplicitSolve(const Array<OneD, const Array<OneD,      NekDouble> >&inarray,
                      Array<OneD, Array<OneD, NekDouble> >&outarray,
                      NekDouble time,
                      NekDouble lambda);


    private:
        NekDouble m_waveFreq;
        NekDouble m_epsilon;
    };
}

#endif