//--------------------------------------------------------------------
// $Id$
//
// Demo test of usage of the TNode class for implementing a simple
// direct coupling scheme between FMT and GEM in a single-GEM-call
// fashion, assuming that the chemical speciation and all dynamic
// parameter data are kept in the FMT part, which calls GEM IPM
// calculation once per node.
// TNode class implements a  simple C/C++ interface between GEMS3K
// and FMT codes. Works with DATACH and work DATABR structures
//
// Copyright (c) 2006-2012 S.Dmytriyeva, D.Kulik + (c) 2014 A.Yapparova
// <GEMS Development Team, mailto:gems2.support@psi.ch>
//
// This file is part of the GEMS3K code for thermodynamic modelling
// by Gibbs energy minimization <http://gems.web.psi.ch/GEMS3K/>
//
// GEMS3K is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.

// GEMS3K is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with GEMS3K code. If not, see <http://www.gnu.org/licenses/>.
//-------------------------------------------------------------------

#include "main.h"
#include <time.h>
time_t start,end;

//The case of data exchange in computer memory
int main( int argc, char* argv[] )
{
   char (*recipes)[fileNameLength] = 0;

   // Analyzing command line arguments ( Default arguments)
   char input_system_file_list_name[256] = "system-dat.lst";
   char input_recipes_file_list_name[256] = "more_recipes.lst";

   if (argc >= 2 )
       strncpy( input_system_file_list_name, argv[1], 256);
   // list of DCH, IPM and DBR input files for initializing GEMS3K
   if (argc >= 3 ) // list of DBR files for setting boundary conditions
       strncpy( input_recipes_file_list_name, argv[2], 256);

    // Creates TNode structure instance accessible trough the "node" pointer
    TNode* node  = new TNode();

    // (1) Initialization of GEMS3K internal data by reading  files
    //     whose names are given in the ipm_input_system_file_list_name
    if( node->GEM_init( input_system_file_list_name ) )
    {
          cout << "Error occured during reading the files" ;
          return 1;
    }

    // Getting direct access to work node DATABR structure which exchanges the
    // data with GEMS3K (already filled out by reading the DBR input file)
    DATABR* dBR = node->pCNode();

    // Getting direct access to DataCH structure in GEMS3K instance memory
    DATACH* dCH = node->pCSD();

    // Creating memory for mass transport nodes
    // 11 nodes, 99 time steps
    //TMyTransport mt( 11, 100, dCH->nICb, dCH->nDCb, dCH->nPHb, dCH->nPSb, 1 );
    // 101 nodes 500 time steps
    TMyTransport mt( 101, 500, dCH->nICb, dCH->nDCb, dCH->nPHb, dCH->nPSb, 1 );

    // Initialization of GEMS3K and chemical information for nodes kept in the MT part
    long int in;
    for(  in=0; in< mt.nNodes; in++ )
    {
        // Asking GEM IPM to run with automatic initial approximation
        dBR->NodeStatusCH = NEED_GEM_AIA;
        // (2) re-calculating equilibrium by calling GEMIPM2K, getting the status back
        mt.aNodeStatusCH[in] = node->GEM_run( false);
        if( !( mt.aNodeStatusCH[in] == OK_GEM_AIA || mt.aNodeStatusCH[in] == OK_GEM_SIA ) )
        {
              cout << "Error occured during re-calculating equilibrium" ;
              return 5;
        }

        // Extracting GEM IPM input data to mass-transport program arrays
        node->GEM_restore_MT( mt.aNodeHandle[in], mt.aNodeStatusCH[in], mt.aT[in], mt.aP[in],
            mt.aVs[in], mt.aMs[in], mt.abIC[in], mt.adul[in], mt.adll[in], mt.aaPH[in] );
          
        // Extracting GEM IPM output data to mass-transport program arrays
        node->GEM_to_MT( mt.aNodeHandle[in], mt.aNodeStatusCH[in], mt.aIterDone[in],
            mt.aVs[in], mt.aMs[in], mt.aGs[in], mt.aHs[in], mt.aIC[in], mt.apH[in], mt.ape[in],
            mt.aEh[in], mt.arMB[in], mt.auIC[in], mt.axDC[in], mt.agam[in], mt.axPH[in],
            mt.avPS[in], mt.amPS[in], mt.abPS[in], mt.axPA[in], mt.aaPh[in], mt.abSP[in] );

        // Here the setup of initial differences between node compositions,
        //    temperatures, etc. can be implemented
        //
        // Here the file output for the initial conditions can be implemented
    }

    if (argc >= 3 )  // Read DATABR structure from text file
    {
       char NextRecipeFileName[256];
       char NextRecipeOutFileName[300];
       char input_recipes_file_list_path[256-fileNameLength] = "";

       // Reading list of recipes names from file
       recipes = f_getfiles(  input_recipes_file_list_name,
                  input_recipes_file_list_path, mt.nRecipes, ',');
       // in this example, nRecipes = 1 (one additional input recipe)

       for(  in=0; in<min(mt.nRecipes, mt.nNodes); in++ )
       {
          // Trying to read the next DBR file name
          sprintf(NextRecipeFileName , "%s%s", input_recipes_file_list_path, recipes[in] );

          // (5) Reading the next DBR file (boundary condition on the left)
          node->GEM_read_dbr( NextRecipeFileName );
          // Asking GEM IPM to run with automatic initial approximation
          dBR->NodeStatusCH = NEED_GEM_AIA;
          // (2) Re-calculating chemical equilibrium by calling GEM
          mt.aNodeStatusCH[in] = node->GEM_run( false );
          if( !( mt.aNodeStatusCH[in] == OK_GEM_AIA || mt.aNodeStatusCH[in] == OK_GEM_SIA ) )
          {
              cout << "Error occured during re-calculating chemical equilibrium" ;
              return 5;
          }

          // (6) Extracting GEMIPM input data to mass-transport program arrays
          node->GEM_restore_MT( mt.aNodeHandle[in], mt.aNodeStatusCH[in], mt.aT[in], mt.aP[in],
              mt.aVs[in], mt.aMs[in], mt.abIC[in], mt.adul[in], mt.adll[in], mt.aaPH[in] );
          
          // (7) Extracting GEMIPM output data to mass-transport program arrays
          node->GEM_to_MT( mt.aNodeHandle[in], mt.aNodeStatusCH[in], mt.aIterDone[in],
              mt.aVs[in], mt.aMs[in], mt.aGs[in], mt.aHs[in], mt.aIC[in], mt.apH[in], mt.ape[in],
              mt.aEh[in], mt.arMB[in], mt.auIC[in], mt.axDC[in], mt.agam[in], mt.axPH[in],
              mt.avPS[in], mt.amPS[in], mt.abPS[in], mt.axPA[in], mt.aaPh[in], mt.abSP[in] );

          // Here the setup of initial differences between node compositions,
          //    temperatures, etc. can be implemented
          //
          // Here the file output for the initial conditions can be implemented
       }  // end loop on in
    }

    // Main loop - iterations over nTimes time steps
    int xCalcite = node->Ph_name_to_xDB("Calcite");
    int xDolomite = node->Ph_name_to_xDB("Dolomite-dis");
    int xAq_gen = node->Ph_name_to_xDB("aq_gen");
    long int ICndx[5];
    ICndx[0] = node->IC_name_to_xDB("Ca");
    ICndx[1] = node->IC_name_to_xDB("C");
    ICndx[2] = node->IC_name_to_xDB("O");
    ICndx[3] = node->IC_name_to_xDB("Mg");
    ICndx[4] = node->IC_name_to_xDB("Cl");
    // Checking indexes
    cout << "xCa= " << ICndx[0] << " xC=" << ICndx[1] << " xO=" << ICndx[2] << " xMg="
         << ICndx[3] << " xCl=" << ICndx[4] << endl << " xCalcite=" << xCalcite
         << " xDolomite=" << xDolomite << " xAq_gen=" << xAq_gen << endl << endl;

    time (&start);

    long int it;
    for( it=0; it< mt.nTimes; it++ )
    {
       cout << "Time step  " << it;
       // Mass transport loop over nodes (not a real transport model)
       mt.OneTimeStepRun( ICndx, 5 );

       cout << "Node\tAq\tpH\tCalcite\tDolomite\tCa\tMg\tCl" << endl;

       // Chemical equilibration loop over nodes
       for( in=0; in< mt.nNodes; in++ )
       {
          mt.aNodeHandle[in] = in;
          mt.aNodeStatusCH[in] = NEED_GEM_SIA;
          // mt.aNodeStatusCH[in] = NEED_GEM_AIA;
          // (8) Setting input data for GEM IPM to use available node speciation as
          // initial approximation
          node->GEM_from_MT( mt.aNodeHandle[in], mt.aNodeStatusCH[in],
                  mt.aT[in], mt.aP[in], mt.aVs[in], mt.aMs[in],
                  mt.abIC[in], mt.adul[in], mt.adll[in], mt.aaPH[in], mt.axDC[in], mt.agam[in] );
          // (9)   Passing current FMT iteration information into the work DATABR structure
          node->GEM_set_MT( (double)it, 1. );

          // Calling GEMIPM calculation
          mt.aNodeStatusCH[in] = node->GEM_run( true );

          if( ( mt.aNodeStatusCH[in] == ERR_GEM_AIA || mt.aNodeStatusCH[in] == ERR_GEM_SIA ||
                        mt.aNodeStatusCH[in] ==  T_ERROR_GEM ) )
          {
              cout << "Error: GEM calculation results are not retrieved. Time step"
                 << it << " node " << in << endl;
          }
          else
          {
            if( ( mt.aNodeStatusCH[in] == BAD_GEM_AIA || mt.aNodeStatusCH[in] == BAD_GEM_SIA  ) )
            {
               cout << "Insufficient quality of GEM solution, but GEM results are retrieved"
               << it << " node " << in << endl;
            }              
            else // (7) Extracting GEMIPM output data to FMT part
              node->GEM_to_MT( mt.aNodeHandle[in], mt.aNodeStatusCH[in], mt.aIterDone[in],
                mt.aVs[in], mt.aMs[in], mt.aGs[in], mt.aHs[in], mt.aIC[in], mt.apH[in], mt.ape[in],
                mt.aEh[in],mt.arMB[in], mt.auIC[in], mt.axDC[in], mt.agam[in], mt.axPH[in],
                mt.avPS[in], mt.amPS[in], mt.abPS[in], mt.axPA[in], mt.aaPh[in], mt.abSP[in] );
          }
          // Here, the output upon completion of the time step is usually implemented
          //  to monitor the coupled simulation or collect results
          cout << in <<"\t"<< mt.axPH[in][xAq_gen] << "\t" << mt.apH[in] <<
                  "\t" << mt.axPH[in][xCalcite] <<
                  "\t" << mt.axPH[in][xDolomite] <<
                  "\t" << mt.abPS[in][ICndx[0]] <<
                  "\t" << mt.abPS[in][ICndx[3]] <<
                  "\t" << mt.abPS[in][ICndx[4]] <<
                  endl;
      }
   }
   // Calculations finished - end time reached
    time (&end);
    double dif = difftime (end,start);
    printf ("Elasped time is %.2lf seconds.", dif );

   // Final output e.g. of total simulation time or of the final distribution of
   //  components and phases in all nodes can be implemented here

   // deleting GEM IPM and data exchange memory structures
   delete node;
   // end of example
   return 0;
}


TMyTransport::TMyTransport( long int p_nNod, long int p_nTim, long int p_nIC, long int p_nDC,
              long int p_nPH, long int p_nPS, long int p_nRcps )
{

    nNodes = p_nNod;
    nTimes = p_nTim;
    nIC = p_nIC;
    nDC = p_nDC;
    nPH = p_nPH;
    nPS = p_nPS;
    nRecipes = p_nRcps;

    aNodeHandle = new long int [nNodes];
    aNodeStatusCH = new long int [nNodes];
    aIterDone = new long int [nNodes];

    aT = new double [nNodes];
    aP = new double [nNodes];
    aVs = new double [nNodes];
    aMs = new double [nNodes];
    aGs = new double [nNodes];
    aHs = new double [nNodes];
    aIC = new double [nNodes];
    apH = new double [nNodes];
    ape = new double [nNodes];
    aEh = new double [nNodes];

    axDC = new double *[nNodes];
    agam = new double *[nNodes];
    axPH = new double *[nNodes];
    aaPH = new double *[nNodes];
    avPS = new double *[nNodes];
    amPS = new double *[nNodes];
    abPS = new double *[nNodes];
    axPA = new double *[nNodes];
    aaPh = new double *[nNodes];
    adul = new double *[nNodes];
    adll = new double *[nNodes];
    abIC = new double *[nNodes];
    arMB = new double *[nNodes];
    auIC = new double *[nNodes];
    abSP = new double *[nNodes];

    for (long int in=0; in<nNodes; in++)
    {
         abIC[in] = new double [nIC];
         arMB[in] = new double [nIC];
         auIC[in] = new double [nIC];
         axDC[in] = new double [nDC];
         agam[in] = new double [nDC];
         adul[in] = new double [nDC];
         adll[in] = new double [nDC];
         aaPH[in] = new double [nPH];
         axPH[in] = new double [nPH];
         avPS[in] = new double [nPS];
         amPS[in] = new double [nPS];
         axPA[in] = new double [nPS];
         aaPh[in] = new double [nPH];
         abPS[in] = new double [nIC*nPS];
         abSP[in] = new double [nIC];
    }
}

TMyTransport::~TMyTransport()
{
    // Deleting chemical data arrays for nodes
    for (long int in=0; in<nNodes; in++)
    {
        delete[]abIC[in];
        delete[]arMB[in];
        delete[]auIC[in];
        delete[]axDC[in];
        delete[]agam[in];

        delete[]adul[in];
        delete[]adll[in];

        delete[]aaPH[in];
        delete[]axPH[in];
        delete[]avPS[in];
        delete[]amPS[in];
        delete[]axPA[in];
        delete[]aaPh[in];
        delete[]abPS[in];
        delete[]abSP[in];
    }
// return;
    delete[]axDC;
    delete[]agam;
    delete[]axPH;
    delete[]aaPH;
    delete[]avPS;
    delete[]amPS;
    delete[]abPS;
    delete[]axPA;
    delete[]aaPh;
    delete[]adul;
    delete[]adll;
    delete[]abIC;
    delete[]arMB;
    delete[]auIC;
    delete[]abSP;

    delete[]aNodeHandle;
    delete[]aNodeStatusCH;
    delete[]aIterDone;
    delete[]aT;
    delete[]aP;
    delete[]aVs;
    delete[]aMs;
    delete[]aGs;
    delete[]aHs;
    delete[]aIC;
    delete[]apH;
    delete[]ape;
    delete[]aEh;
}

// A very simple example of finite difference transport algorithm
// Contributed on 22.08.2014 by Alina Yapparova, Chair of Reservoir Engineering,
// Montanuniveritaet Leoben, Austria
//
void TMyTransport::OneTimeStepRun( long int *ICndx, long int nICndx )
{
    double column_length  = 0.5; // [m]
    double dx = column_length/(nNodes-1);

    //constant velocity field
    double v = 1.e-5; // velocity [m/s]
    // stability requirement: dt<=dx/velocity, so we can choose any coefficient k<1
    double k = 0.1; // k = dt/dx*v
    // calculate dt
    double dt = k*dx/v;
    // and print dt into the output file
    cout<<"\tdt = " << dt << "[s]" << endl;

    //Finite difference approximation for the equation dc/dt + v*dc/dx = 0
    // explicit time, left spatial derivative
    // (c^{n+1}_{i} - c^{n}_{i})/dt + v*(c^{n}_{i} - c^{n}_{i-1})/dx = 0
    // c^{n+1}_{i} = (1 - k)*c^{n}_{i} + k*c^{n}_{i-1}
    long int in;
    for(  in=1; in< nNodes; in++ )
    {
        for (int i=0; i<nICndx; i++)
        {
            abIC[in][ICndx[i]] = (1 - k)*abPS[in][ICndx[i]] + k*abPS[in-1][ICndx[i]] + abSP[in][ICndx[i]];
            // where abPS is the total amount of an independent component ICndx[i] in the aqueous phase (after GEMS computation)
            // abSP is the total amount of an independent component ICndx[i] in ALL solid phases
            // abIC is the total amount of ICndx[i] that will serve as an input constraint for GEMS computation at the next time level
            // NB: more precisely, one should write abPS[in][n*nIC + ICndx[i]], where 0=<n<=nPS, and transport each phase-solution separately
            //but as far as an aqueous phase is the first in the list (n=0), this simplified indexing will work for transport of aq phase
        }
    }
}

//---------------------------------------------------------------------------
// end of main.cpp for the node-gem (TNode-level usage) coupled-code example
