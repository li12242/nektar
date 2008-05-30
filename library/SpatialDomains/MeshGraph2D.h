////////////////////////////////////////////////////////////////////////////////
//
//  File:  $Source: /usr/sci/projects/Nektar/cvs/Nektar++/library/SpatialDomains/MeshGraph2D.h,v $
//
//  For more information, please see: http://www.nektar.info/
//
//  The MIT License
//
//  Copyright (c) 2006 Division of Applied Mathematics, Brown University (USA),
//  Department of Aeronautics, Imperial College London (UK), and Scientific
//  Computing and Imaging Institute, University of Utah (USA).
//
//  License for the specific language governing rights and limitations under
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//
//  Description:
//
//
////////////////////////////////////////////////////////////////////////////////
#ifndef NEKTAR_SPATIALDOMAINS_MESHGRAPH2D_H
#define NEKTAR_SPATIALDOMAINS_MESHGRAPH2D_H

#include <SpatialDomains/MeshGraph.h>
#include <SpatialDomains/SegGeom.h>
#include <SpatialDomains/TriGeom.h>
#include <SpatialDomains/QuadGeom.h>

class TiXmlDocument;

namespace Nektar
{
    namespace SpatialDomains
    {
        typedef boost::shared_ptr<QuadGeom> SharedQuadGeomPtr;
        typedef std::vector< SharedQuadGeomPtr >      QuadGeomVector;

        class MeshGraph2D: 
            public MeshGraph
        {

        public:

            MeshGraph2D();
            virtual ~MeshGraph2D();

            void ReadGeometry(std::string &infilename);
            void ReadGeometry(TiXmlDocument &doc);

            SegGeomSharedPtr GetSegGeom(int eID);

            inline const int GetCoordim(void){
                return GetSpaceDimension();
            }

            inline const TriGeomVector &GetTrigeoms(void) const
            {
                return m_trigeoms;
            }

            inline const QuadGeomVector &GetQuadgeoms(void) const
            {
                return m_quadgeoms;
            }

#ifdef OLD
            inline bool GetGeofac_defined(void)
            {
                return m_geofac_defined;
            }
#endif

            void GenXGeoFac();

            inline const int GetNseggeoms() const 
            {
                return int(m_seggeoms.size());
            }

            inline const int GetVidFromElmt(StdRegions::ExpansionType expansion, 
                const int vert, const int elmt) const 
            {
                if(expansion == StdRegions::eTriangle)
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_trigeoms.size()),
                        "eid is out of range");

                    return m_trigeoms[elmt]->GetVid(vert);
                }
                else
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_quadgeoms.size()),
                        "eid is out of range");

                    return m_quadgeoms[elmt]->GetVid(vert);
                }
            }

            inline const int GetEidFromElmt(StdRegions::ExpansionType expansion, 
                const int edge, const int elmt) const
            {
                if(expansion == StdRegions::eTriangle)
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_trigeoms.size()),
                        "eid is out of range");

                    return m_trigeoms[elmt]->GetEid(edge);
                }
                else
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_quadgeoms.size()),
                        "eid is out of range");

                    return m_quadgeoms[elmt]->GetEid(edge);
                }
            }

            inline const StdRegions::EdgeOrientation GetEorientFromElmt(StdRegions::ExpansionType expansion,const int edge, const int elmt) const 
            {
                if(expansion == StdRegions::eTriangle)
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_trigeoms.size()),
                        "eid is out of range");

                    return m_trigeoms[elmt]->GetEorient(edge);
                }
                else
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_quadgeoms.size()),
                        "eid is out of range");

                    return m_quadgeoms[elmt]->GetEorient(edge);
                }
            }


            inline const StdRegions::EdgeOrientation GetCartesianEorientFromElmt(StdRegions::ExpansionType expansion,const int edge, const int elmt) const
            {
                StdRegions::EdgeOrientation returnval;

                if(expansion == StdRegions::eTriangle)
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_trigeoms.size()),
                        "eid is out of range");

                    returnval = m_trigeoms[elmt]->GetEorient(edge);
                }
                else
                {
                    ASSERTL2((elmt >=0)&&(elmt < m_quadgeoms.size()),
                        "eid is out of range");

                    returnval =  m_quadgeoms[elmt]->GetEorient(edge);
                }

                // swap orientation if on edge 2 & 3 (if quad)
                if(edge >= 2)
                {
                    if(returnval == StdRegions::eForwards)
                    {
                        returnval = StdRegions::eBackwards;
                    }
                    else
                    {
                        returnval = StdRegions::eForwards; 
                    }
                }
                return returnval;
            }

            int GetNumComposites(void)
            {
                return int(m_MeshCompositeVector.size());
            }

            int GetNumCompositeItems(int whichComposite)
            {
                int returnval = -1;

                try
                {
                    returnval = int(m_MeshCompositeVector[whichComposite]->size());
                }
                catch(...)
                {
                    std::ostringstream errStream;
                    errStream << "Unable to access composite item [" << whichComposite << "].";
                    NEKERROR(ErrorUtil::efatal, errStream.str());
                }

                return returnval;
            }

            /// \brief Return the elements (shared ptrs) that have this edge.
            ElementEdgeVectorSharedPtr GetElementsFromEdge(SegGeomSharedPtr edge);

            /// \brief Return the BasisKey corresponding to an edge of an element
            LibUtilities::BasisKey GetEdgeBasisKey(SegGeomSharedPtr edge);

        protected:
            void ReadEdges    (TiXmlDocument &doc);
            void ReadElements (TiXmlDocument &doc);
            void ReadComposites(TiXmlDocument &doc);
            void ResolveGeomRef(const std::string &prevToken, const std::string &token);

#ifdef OLD
            bool   m_geofac_defined;
#endif

        private:
            SegGeomVector  m_seggeoms;
            TriGeomVector  m_trigeoms;
            QuadGeomVector m_quadgeoms;
        };
    };
};

#endif //NEKTAR_SPATIALDOMAINS_MESHGRAPH2D_H

//
// $Log: MeshGraph2D.h,v $
// Revision 1.20  2008/05/29 19:07:39  delisi
// Removed the Write(..) methods, so it is only in the base MeshGraph class. Also, added a line to set the global ID of the geometry object for every element read in.
//
// Revision 1.19  2008/03/18 14:14:49  pvos
// Update for nodal triangular helmholtz solver
//
// Revision 1.18  2008/01/21 19:58:14  sherwin
// Updated so that QuadGeom and TriGeom have SegGeoms instead of EdgeComponents
//
// Revision 1.17  2007/12/11 21:51:53  jfrazier
// Updated 2d components so elements could be retrieved from edges.
//
// Revision 1.16  2007/12/04 03:02:27  jfrazier
// Changed to stringstream.
//
// Revision 1.15  2007/09/20 22:25:06  jfrazier
// Added expansion information to meshgraph class.
//
// Revision 1.14  2007/07/22 23:04:24  bnelson
// Backed out Nektar::ptr.
//
// Revision 1.13  2007/07/20 02:15:08  bnelson
// Replaced boost::shared_ptr with Nektar::ptr
//
// Revision 1.12  2007/06/07 15:54:19  pvos
// Modificications to make Demos/MultiRegions/ProjectCont2D work correctly.
// Also made corrections to various ASSERTL2 calls
//
// Revision 1.11  2007/06/05 16:36:55  pvos
// Updated Explist2D ContExpList2D and corresponding demo-codes
//
// Revision 1.10  2007/05/28 21:48:42  sherwin
// Update for 2D functionality
//
// Revision 1.9  2006/10/17 22:26:01  jfrazier
// Added capability to specify ranges in composite definition.
//
// Revision 1.8  2006/09/26 23:41:53  jfrazier
// Updated to account for highest level NEKTAR tag and changed the geometry tag to GEOMETRY.
//
// Revision 1.7  2006/08/24 18:50:01  jfrazier
// Completed error checking on permissable composite item combinations.
//
// Revision 1.6  2006/08/18 19:45:29  jfrazier
// Completed composites.
//
// Revision 1.5  2006/08/17 22:55:00  jfrazier
// Continued adding code to process composites in the mesh2d.
//
// Revision 1.4  2006/08/16 23:34:42  jfrazier
// *** empty log message ***
//
// Revision 1.3  2006/07/02 17:16:17  sherwin
//
// Modifications to make MultiRegions work for a connected domain in 2D (Tris)
//
// Revision 1.2  2006/06/01 14:15:30  sherwin
// Added typdef of boost wrappers and made GeoFac a boost shared pointer.
//
// Revision 1.1  2006/05/04 18:59:01  kirby
// *** empty log message ***
//
// Revision 1.11  2006/04/11 23:18:11  jfrazier
// Completed MeshGraph2D for tri's and quads.  Not thoroughly tested.
//
// Revision 1.10  2006/04/04 23:12:38  jfrazier
// More updates to readers.  Still work to do on MeshGraph2D to store tris and quads.
//
// Revision 1.9  2006/03/25 00:58:29  jfrazier
// Many changes dealing with fundamental structure and reading/writing.
//
// Revision 1.8  2006/03/12 14:20:43  sherwin
//
// First compiling version of SpatialDomains and associated modifications
//
// Revision 1.7  2006/03/12 07:42:03  sherwin
//
// Updated member names and StdRegions call. Still has not been compiled
//
// Revision 1.6  2006/03/04 20:26:05  bnelson
// Added comments after #endif.
//
// Revision 1.5  2006/02/19 01:37:34  jfrazier
// Initial attempt at bringing into conformance with the coding standard.  Still more work to be done.  Has not been compiled.
//
//
