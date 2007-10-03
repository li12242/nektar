///////////////////////////////////////////////////////////////////////////////
//
// File: ArithmeticTraits.hpp
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

#ifndef NEKTAR_LIB_UTILITIES_EXPRESSION_TEMPLATES_ARITHMETIC_TRAITS_HPP
#define NEKTAR_LIB_UTILITIES_EXPRESSION_TEMPLATES_ARITHMETIC_TRAITS_HPP

#include <boost/typeof/typeof.hpp>

namespace Nektar
{
    // The following classes are workarounds for visual studio.
    // Theoretically, typename BOOST_TYPEOF_TPL(NekAdd(LhsType, RhsType)) would be sufficient,
    // but it seems to confuse visual studio.
    template<typename LhsType, typename RhsType>
    class AdditionTraits
    {
        public:
            BOOST_TYPEOF_NESTED_TYPEDEF_TPL(nested, NekAdd(LhsType(), RhsType()));
            typedef typename nested::type ResultType;
    };
    
    template<typename LhsType, typename RhsType>
    class SubtractionTraits
    {
        public:
            BOOST_TYPEOF_NESTED_TYPEDEF_TPL(nested, NekSubtract(LhsType(), RhsType()));
            typedef typename nested::type ResultType;
    };
    
    template<typename LhsType, typename RhsType>
    class MultiplicationTraits
    {
        public:
            BOOST_TYPEOF_NESTED_TYPEDEF_TPL(nested, NekMultiply(LhsType(), RhsType()));
            typedef typename nested::type ResultType;

            //static void Multiply(ResultType& result, 
            //                     typename boost::call_traits<LhsType>::const_reference lhs,
            //                     typename boost::call_traits<RhsType>::const_reference rhs)
            //{
            //    NekMultiply(result, lhs, rhs);
            //}
    };
    
    template<typename LhsType, typename RhsType>
    class DivisionTraits
    {
        public:
            BOOST_TYPEOF_NESTED_TYPEDEF_TPL(nested, NekDivide(LhsType(), RhsType()));
            typedef typename nested::type ResultType;
    };

}

#endif //NEKTAR_LIB_UTILITIES_EXPRESSION_TEMPLATES_ARITHMETIC_TRAITS_HPP
