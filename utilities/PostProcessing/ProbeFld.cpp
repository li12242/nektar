#include <cstdio>
#include <cstdlib>

#include <MultiRegions/ExpList.h>
#include <MultiRegions/ExpList1D.h>
#include <MultiRegions/ExpList2D.h>
#include <SpatialDomains/Geometry2D.h>
using namespace Nektar;

int main(int argc, char *argv[])
{
    int i,j;

    if(argc != 10)
    {
        fprintf(stderr,
                "Usage: ProbeFld meshfile fieldfile N x0 y0 z0 dx dy dz\n");
        fprintf(stderr,
                "  Probes N points along the line from (x0,y0,z0) to "
                "(x0+dx, y0+dy, z0+dz)\n");
        exit(1);
    }

    //----------------------------------------------
    // Read in mesh from input file
    string meshfile(argv[1]);
    SpatialDomains::MeshGraph graph;
    SpatialDomains::MeshGraphSharedPtr graphShPt = graph.Read(meshfile);
    //----------------------------------------------

    //----------------------------------------------
    // Import field file.
    string fieldfile(argv[2]);
    vector<SpatialDomains::FieldDefinitionsSharedPtr> fielddef;
    vector<vector<NekDouble> > fielddata;
    graphShPt->Import(fieldfile,fielddef,fielddata);
    //----------------------------------------------

    //----------------------------------------------
    // Set up Expansion information
    vector< vector<LibUtilities::PointsType> > pointstype;
    for(i = 0; i < fielddef.size(); ++i)
    {
        vector<LibUtilities::PointsType> ptype;
        for(j = 0; j < 2; ++j)
        {
            ptype.push_back(LibUtilities::ePolyEvenlySpaced);
        }
        pointstype.push_back(ptype);
    }
    graphShPt->SetExpansions(fielddef,pointstype);
    //----------------------------------------------


    //----------------------------------------------
    // Define Expansion
    int expdim  = graphShPt->GetMeshDimension();
    int nfields = fielddef[0]->m_Fields.size();
    Array<OneD, MultiRegions::ExpListSharedPtr> Exp(nfields);

    switch(expdim)
    {
    case 1:
        {
            SpatialDomains::MeshGraph1DSharedPtr mesh;

            if(!(mesh = boost::dynamic_pointer_cast<
                                    SpatialDomains::MeshGraph1D>(graphShPt)))
            {
                ASSERTL0(false,"Dynamic cast failed");
            }

            MultiRegions::ExpList1DSharedPtr Exp1D;
            Exp1D = MemoryManager<MultiRegions::ExpList1D>
                                                    ::AllocateSharedPtr(*mesh);
            Exp[0] = Exp1D;
            for(i = 1; i < nfields; ++i)
            {
                Exp[i] = MemoryManager<MultiRegions::ExpList1D>
                                                    ::AllocateSharedPtr(*Exp1D);
            }
        }
        break;
    case 2:
        {
            SpatialDomains::MeshGraph2DSharedPtr mesh;

            if(!(mesh = boost::dynamic_pointer_cast<
                                    SpatialDomains::MeshGraph2D>(graphShPt)))
            {
                ASSERTL0(false,"Dynamic cast failed");
            }

            MultiRegions::ExpList2DSharedPtr Exp2D;
            Exp2D = MemoryManager<MultiRegions::ExpList2D>
                                                    ::AllocateSharedPtr(*mesh);
            Exp[0] =  Exp2D;

            for(i = 1; i < nfields; ++i)
            {
                Exp[i] = MemoryManager<MultiRegions::ExpList2D>
                                                    ::AllocateSharedPtr(*Exp2D);
            }
        }
        break;
    case 3:
        ASSERTL0(false,"3D not set up");
        break;
    default:
        ASSERTL0(false,"Expansion dimension not recognised");
        break;
    }
    //----------------------------------------------

    //----------------------------------------------
    // Copy data from field file
    for(j = 0; j < nfields; ++j)
    {
        for(int i = 0; i < fielddata.size(); ++i)
        {
            Exp[j]->ExtractDataToCoeffs(fielddef [i],
                                        fielddata[i],
                                        fielddef [i]->m_Fields[j]);
        }
        Exp[j]->BwdTrans(Exp[j]->GetCoeffs(),Exp[j]->UpdatePhys());
        Exp[j]->PutPhysInToElmtExp();
    }
    //----------------------------------------------

    //----------------------------------------------
    // Probe data fields
    NekDouble N     = atoi(argv[3]);
    NekDouble x0    = atof(argv[4]);
    NekDouble y0    = atof(argv[5]);
    NekDouble z0    = atof(argv[6]);
    NekDouble dx    = atof(argv[7])/(N-1);
    NekDouble dy    = atof(argv[8])/(N-1);
    NekDouble dz    = atof(argv[9])/(N-1);
    NekDouble u     = 0.0;

    Array<OneD, NekDouble> gloCoord(3,0.0);

    for (int i = 0; i < N; ++i)
    {
        gloCoord[0] = x0 + i*dx;
        gloCoord[1] = y0 + i*dy;
        gloCoord[2] = z0 + i*dz;
        cout << gloCoord[0] << "   " << gloCoord[1] << "   " << gloCoord[2];
        for (int j = 0; j < nfields; ++j)
        {
            cout << "   " << Exp[j]->GetExp(gloCoord)->PhysEvaluate(gloCoord)
                 << endl;
        }
    }
    //----------------------------------------------
    return 0;
}
