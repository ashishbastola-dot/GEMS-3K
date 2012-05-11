//-------------------------------------------------------------------
// $Id: ipm_main.cpp 705 2006-04-28 19:39:01Z gems $
//
// Copyright (C) 1992,2010  D.Kulik, S.Dmitrieva, K.Chudnenko, I.Karpov
//
// Implementation of parts of the Interior Points Method (IPM) module
// for convex programming Gibbs energy minimization, described in:
// (Karpov, Chudnenko, Kulik (1997): American Journal of Science
//  v.297 p. 767-806)
//
// Uses: GEM-Vizor GUI DBMS library, gems/lib/gemvizor.lib
// Uses: JAMA/C++ Linear Algebra Package based on the Template
// Numerical Toolkit (TNT) - an interface for scientific computing in C++,
// (c) Roldan Pozo, NIST (USA), http://math.nist.gov/tnt/download.html
//
// This file is part of a GEM-Selektor (GEMS) v.3.x.x program
// environment for thermodynamic modeling in geochemistry and of the
// standalone GEMIPM2K code.
// This file may be distributed under the terms of the GEMS-PSI
// QA Licence (GEMSPSI.QAL)
//
// See http://gems.web.psi.ch/ for more information
// E-mail: gems2.support@psi.ch
//-------------------------------------------------------------------
//

#include "m_param.h"
#include "jama_lu.h"
#include "jama_cholesky.h"
using namespace TNT;
using namespace JAMA;

#ifndef IPMGEMPLUGIN
#include "service.h"
#include "stepwise.h"
#endif
#include "node.h"
#include<iomanip>

// #define GEMITERTRACE
#define uDDtrace false

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Call to GEM IPM calculation of equilibrium state in MULTI
// (with already scaled GEM problem)
void TMulti::GibbsEnergyMinimization()
{
  bool IAstatus;
  Reset_uDD( 0L, uDDtrace); // Experimental - added 06.05.2011 KD
FORCED_AIA:
    GEM_IPM_Init();
   if( pmp->pNP )
   {
      if( pmp->ITaia <=30 )       // Foolproof
           pmp->IT = 30;
       else
           pmp->IT = pmp->ITaia;  // Setting number of iterations for the smoothing parameter
   }

   IAstatus = GEM_IPM_InitialApproximation( );
   if( IAstatus == false )
   {
      //Wrapper call for the IPM iteration sequence
      GEM_IPM( -1 );
      if( !pmp->pNP )
          pmp->ITaia = pmp->IT;

       // calculation of demo data for gases
       for( long int ii=0; ii<pmp->N; ii++ )
           pmp->U_r[ii] = pmp->U[ii]*pmp->RT;
       GasParcP();  // do we really need it?
   }
   pmp->IT = pmp->ITG;

   // testing results
   if( pmp->MK == 2 )
   {	if( pmp->pNP )
        {                     // SIA mode failed
            pmp->pNP = 0;
            pmp->MK = 0;
            Reset_uDD( 0L, uDDtrace );  // resetting u divergence detector
            goto FORCED_AIA;  // Trying again with AIA set after bad SIA
         }
        else {                 // AIA mode failed
           if( nCNud <= 0L )   // Generic AIA IPM failure, except the case of u divergence
               Error( pmp->errorCode ,pmp->errorBuf );
           // Now trying again with AIA down to cnr-2 IPM iteration, no PhaseSelection()
           // and possibly cleanup only for species with much lower activity than concentration
           pmp->ITG = 0;
           goto FORCED_AIA;
       }
   }
   pmp->FitVar[0] = bfc_mass();  // getting total mass of solid phases in the system
   if( pmp->MK || pmp->PZ ) // no good solution
       TProfil::pm->testMulti();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Main sequence of GEM IPM algorithm implementation
//  Main place for implementation of diagnostics and setup
//  of IPM precision and convergence
//  rLoop is the index of the primal solution refinement loop (for tracing)
//   or -1 if this is main GEM_IPM call
//
void TMulti::GEM_IPM( long int rLoop )
{
    long int i, j, eRet, status=0; long int csRet=0;
// bool CleanAfterIPM = true;
    SPP_SETTING *pa = &TProfil::pm->pa;

#ifdef GEMITERTRACE
  to_text_file( "MultiDumpB.txt" );   // Debugging
#endif

    pmp->W1=0; pmp->K2=0;         // internal counters and indicators
    pmp->Ec = pmp->MK = pmp->PZ = 0;    // Return codes
    if(!nCNud && !cnr )
        setErrorMessage( 0, "" , "");  // empty error info
 //   if( TProfil::pm->pa.p.PLLG == 0 )  // Disabled by DK 11.05.2011
 //       TProfil::pm->pa.p.PLLG = 20;  // Changed 28.04.2010 KD

    if( pmp->pULR && pmp->PLIM )
        Set_DC_limits( DC_LIM_INIT );

mEFD:  // Mass balance refinement (formerly EnterFeasibleDomain())
     eRet = MassBalanceRefinement( pmp->K2 ); // Here the MBR() algorithm is called

#ifdef GEMITERTRACE
to_text_file( "MultiDumpC.txt" );   // Debugging
#endif

#ifndef IPMGEMPLUGIN
#ifndef Use_mt_mode
    pVisor->Update(false);
#endif
// STEPWISE (2)  - stop point to examine output from EFD()
STEP_POINT("After FIA");
#endif
    switch( eRet )
    {
     case 0:  // OK
              break;
     case 5:  // Initial Lagrange multiplier for metastability broken for DC
     case 4:  // Initial mass balance broken for IC
     case 3:  // too small step length in descent algorithm
     case 2:  // max number of iterations has been exceeded in MassBalanceRefinement()
     case 1: // degeneration in R matrix  for MassBalanceRefinement()
   	         if( pmp->pNP )
                 {   // bad SIA mode - trying the AIA mode
                pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
#ifdef GEMITERTRACE
//f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT << " ! PIA->AIA on E04IPM" << endl;
#endif
               goto FORCED_AIA;
   	         }
   	         else
   	        	 Error( pmp->errorCode ,pmp->errorBuf );
              break;
    }

   // calling the MainIPMDescent() minimization algorithm
   eRet = InteriorPointsMethod( status, pmp->K2 );

#ifdef GEMITERTRACE
to_text_file( "MultiDumpD.txt" );   // Debugging
#endif

#ifndef IPMGEMPLUGIN
#ifndef Use_mt_mode
    pVisor->Update(false);
#endif
// STEPWISE (3)  - stop point to examine output from IPM()
   STEP_POINT("After IPM");
#endif

// Diagnostics of IPM results
   switch( eRet )
   {
     case 0:  // OK
              break;
     case 2:  // max number of iterations has been exceeded in InteriorPointsMethod()
     case 1: // degeneration in R matrix  for InteriorPointsMethod()
         if( pmp->pNP )
         {   // bad PIA mode - trying the AIA mode
                pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
#ifdef GEMITERTRACE
//f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT << " ! PIA->AIA on E06IPM" << endl;
#endif
	        goto FORCED_AIA;
         }

//#ifdef GEMITERTRACE
//f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT << " AIA: DX->1e-4, DHBM->1e-6 on E06IPM" << endl;
//#endif
//                goto mEFD;
//             }
         Error( pmp->errorCode, pmp->errorBuf );
         break;
     case 3:  // bad CalculateActivityCoefficients() status in SIA mode
     case 4: // Mass balance broken after DualTh recover of DC amounts
         if( pmp->pNP )
         {   // bad SIA mode - trying the AIA mode
                pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
	        goto FORCED_AIA;
         }
         Error( pmp->errorCode, pmp->errorBuf );
         break;
     case 5: // Divergence in dual solution approximation
                // no or only partial cleanup can be done
         pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
         if( pmp->pNP )
         {   // bad SIA mode - trying the AIA mode 
              goto FORCED_AIA;
         }
         goto FORCED_AIA; // even if in AIA, start over and go until r-1 then finish and do MBR
         break;
   }

   // Here the entry to new PSSC() module controlled by PC = 2
   if( pa->p.PC >= 2 && nCNud <= 0 )   // only if there is no divergence in the dual solution
   {
       long int ps_rcode, k_miss, k_unst;

       ps_rcode = PhaseSelectionSpeciationCleanup( k_miss, k_unst, csRet );
/*
  #ifndef IPMGEMPLUGIN
  #ifndef Use_mt_mode
      pVisor->Update(false);
  #endif
  // STEPWISE (3)  - stop point to examine output from SpeciationCleanup()
     STEP_POINT("After PSSC()");
  #endif
*/
       switch( ps_rcode )  // analyzing return code of PSSC()
       {
             case 1:   // IPM solution is final and consistent, no phases were inserted
                       pmp->PZ = 0;
                       break;
             case 0:   // some phases were inserted and a new IPM loop is needed
       #ifdef GEMITERTRACE
       //f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " K2=" << pmp->K2 <<
       //      " k_miss=" << k_miss << " k_unst=" << k_unst <<  " ! (new Selekt loop)" << endl;
       #endif
                      pmp->PZ = 1;
                      goto mEFD;
             default:
             case -1:  // the IPM solution is inconsistent after 5 phase insertion loops
             {
                 gstring pmbuf("");
                 if(k_miss >=0 )
                  pmbuf = gstring(pmp->SF[k_miss],0,20);
                 gstring pubuf("");
                 if(k_unst >=0 )
                 pubuf = gstring(pmp->SF[k_unst],0,20);
                 char buf[400];
                 sprintf( buf,
          " The phase assemblage remains inconsistent after 5 PSSC() loops.\n"
          " Problematic phase(s): %ld %s   %ld %s \n"
          " Input thermodynamic data may be inconsistent, or some relevant species missing.",
                          k_miss, pmbuf.c_str(), k_unst, pubuf.c_str() );
                 setErrorMessage( 8, "W08IPM: PSSC():", buf );
                 if( pmp->pNP )
                 {   // bad SIA mode - there are inconsistent phases after 5 attempts. Attempting AIA mode
                         pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
       #ifdef GEMITERTRACE
       //f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT <<
       //      " k_miss=" << k_miss << " k_unst=" << k_unst << " ! PIA->AIA on E08IPM (Selekt)" << endl;
       #endif
                         goto FORCED_AIA;
                 }
                 else
                 { pmp->PZ = 2; // IPM solution could not be improved in PhaseSelect() -
                                //   therefore, some inconsistent phases remain
                   // return;
                 }
             }
       } // end switch

   }
   else if( pa->p.PC == 1 && nCNud <= 0 ) // Old PhaseSelect() mode PC = 1
   {
       //=================== calling old Phase Selection algorithm =====================
        long int ps_rcode, k_miss, k_unst;

        ps_rcode = PhaseSelect( k_miss, k_unst, csRet );

        switch( ps_rcode )
           {
              case 1:   // IPM solution is final and consistent, no phases were inserted
                        pmp->PZ = 0;
                        break;
              case 0:   // some phases were inserted and a new IPM loop is needed
        #ifdef GEMITERTRACE
        //f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " K2=" << pmp->K2 <<
        //      " k_miss=" << k_miss << " k_unst=" << k_unst <<  " ! (new Selekt loop)" << endl;
        #endif
                       pmp->PZ = 1;
                       goto mEFD;
              default:
              case -1:  // the IPM solution is inconsistent after 3 Selekt2() loops
              {
                  gstring pmbuf("");
                  if(k_miss >=0 )
                   pmbuf = gstring(pmp->SF[k_miss],0,20);
                  gstring pubuf("");
                  if(k_unst >=0 )
                  pubuf = gstring(pmp->SF[k_unst],0,20);
                  char buf[400];
                  sprintf( buf,
           " The computed phase assemblage remains inconsistent after 3 PhaseSelect() loops.\n"
           " Problematic phase(s): %ld %s   %ld %s \n"
           " Probably, vector b is not balanced, or DC stoichiometries\n"
           " or standard g0 are wrong, or some relevant phases or DCs are missing.",
                           k_miss, pmbuf.c_str(), k_unst, pubuf.c_str() );
                  setErrorMessage( 8, "W08IPM: PhaseSelect():", buf );
                  if( pmp->pNP )
                  {   // bad PIA mode - there are inconsistent phases after 3 attempts. Attempting AIA mode
                          pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
        #ifdef GEMITERTRACE
        //f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT <<
        //      " k_miss=" << k_miss << " k_unst=" << k_unst << " ! PIA->AIA on E08IPM (Selekt)" << endl;
        #endif
                          goto FORCED_AIA;
                  }
                  else
                  { pmp->PZ = 2; // IPM solution could not be improved in PhaseSelect() -
                                 //   therefore, some inconsistent phases remain
                    // return;
                  }
              }
        } // end switch
   }

   if( pa->p.PRD != 0 && pa->p.PC != 2 && nCNud <= 0 ) // This block is calling the cleanup speciation function
   {
      double AmThExp, AmountThreshold, ChemPotDiffCutoff = 1e-2;
//      long int eRet;

      AmThExp = (double)abs(pa->p.PRD);
      if( AmThExp < 4.)
          AmThExp = 4.;
      AmountThreshold = pow(10,-AmThExp);
      if( pa->p.GAS > 1e-6 )
           ChemPotDiffCutoff = pa->p.GAS;
      for( j=0; j<pmp->L; j++ )
          pmp->XY[j]=pmp->Y[j];    // Storing a copy of speciation vector
      csRet = SpeciationCleanup( AmountThreshold, ChemPotDiffCutoff );
      if( csRet == 1 || csRet == 2 || csRet == -1 || csRet == -2 )
      {  //  Significant cleanup has been done - mass balance refinement is necessary
         TotalPhasesAmounts( pmp->Y, pmp->YF, pmp->YFA );
         for( j=0; j<pmp->L; j++ )
             pmp->X[j]=pmp->Y[j];
         TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
         CalculateConcentrations( pmp->X, pmp->XF, pmp->XFA );  // also ln activities (DualTh)

#ifndef IPMGEMPLUGIN
#ifndef Use_mt_mode
    pVisor->Update(false);
#endif
// STEPWISE (3)  - stop point to examine output from SpeciationCleanup()
   STEP_POINT("After Cleanup");
#endif

      }
      if( csRet != 0 )  {   // Cleanup removed something
 //         for( j=0; j<pmp->L; j++ )   // restoring the Y vector
 //             pmp->Y[j]=pmp->XY[j];
          pmp->W1 = 1;
//          goto mEFD;   // Forced after cleanup ( check what to do with pmp->K2 )
      }
   } // end cleanup

   if( pa->p.PC == 3 )
        XmaxSAT_IPM2();  // Install upper limits to xj of surface species (questionable)!

  if( nCNud <= 0 )
  {  eRet = MassBalanceRefinement( pmp->K2 ); // Mass balance improvement in all normal cases
     switch( eRet )
    {
      case 0:  // OK - refinement of concentrations and activity coefficients
        for( j=0; j<pmp->L; j++ )
           pmp->X[j]=pmp->Y[j];
        TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
        CalculateConcentrations( pmp->X, pmp->XF, pmp->XFA );  // also ln activities (DualTh)
        if( pmp->PD >= 2 )
        {
           CalculateActivityCoefficients( LINK_UX_MODE);
        }
        break;
     case 5:  // Cleaned-up Lagrange multiplier for metastability broken for DC
     case 4:  // Cleaned-up mass balance broken for IC
     case 3:  // too small step length in MB refinement algorithm after cleanup
     case 2:  // max number of iterations has been exceeded in MassBalanceRefinement()
     case 1: // degeneration of R matrix in MassBalanceRefinement() after cleanup
             if( pmp->pNP )
             {   // bad SIA mode - trying the AIA mode
            pmp->MK = 2;   // Set to check in ComputeEquilibriumState() later on
#ifdef GEMITERTRACE
//f_log << " ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT << " ! PIA->AIA on E04IPM" << endl;
#endif
           goto FORCED_AIA;
             }
             else
               Error( pmp->errorCode ,pmp->errorBuf );
          break;
   }
 }

#ifndef IPMGEMPLUGIN
   pmp->t_end = clock();
   pmp->t_elap_sec = double(pmp->t_end - pmp->t_start)/double(CLOCKS_PER_SEC);
// STEPWISE (4) Stop point after PhaseSelect()
   STEP_POINT("Before Refine()");
#ifndef Use_mt_mode
   pVisor->Update( false );
#endif
#endif
//   if( pmp->MK == 2 )
//       goto FORCED_AIA;

#ifdef GEMITERTRACE
/*f_log << "ITF=" << pmp->ITF << " ITG=" << pmp->ITG << " IT=" << pmp->IT << " MBPRL="
   << pmp->W1 << " rLoop=" << rLoop;
    if( pmp->pNP )
	   f_log << " Ok after PIA" << endl;
    else
	   f_log << " Ok after AIA" << endl;*/
#endif

FORCED_AIA:   // Finish
   pmp->FI1 = 0;  // Recomputing the number of non-zeroed-off phases
   pmp->FI1s = 0;
   for( i=0; i<pmp->FI; i++ )
   {
       if( pmp->YF[i] >= min( pmp->PhMinM, 1e-22 ) )  // Check 1e-22 !!!!!
       {
            pmp->FI1++;
            if( i < pmp->FIs )
                pmp->FI1s++;
       }
   }
   for( i=0; i<pmp->L; i++)
      pmp->G[i] = pmp->G0[i];
   // At pmp->MK == 1, normal return after successful improvement of mass balance precision
   pmp->t_end = clock();   // Fix pure runtime
   pmp->t_elap_sec = double(pmp->t_end - pmp->t_start)/double(CLOCKS_PER_SEC);

#ifdef GEMITERTRACE
to_text_file( "MultiDumpE.txt" );   // Debugging
#endif
}

// ------------------------------------------------------------------------------------------------------
// Finding out whether the automatic initial approximation is necessary for
// launching the IPM algorithm.
// Uses a modified simplex method with two-side constraints (Karpov ea 1997)
// Return code:
// false - OK for IPM
// true  - OK solved (pure phases only in the system)
//
bool TMulti::GEM_IPM_InitialApproximation(  )
{
    long int i, j, k, NN, eCode=-1L;
    double minB, sfactor;
    char buf[512];
    SPP_SETTING *pa = &TProfil::pm->pa;

#ifdef GEMITERTRACE
to_text_file( "MultiDumpA.txt" );   // Debugging
#endif

// Scaling the IPM numerical controls for the system total amount and minimum b(IC)
    NN = pmp->N - pmp->E;    // Charge is not checked!
    minB = pmp->B[0]; // pa->p.DB;
    for(i=0;i<NN;i++)
    {
        if( pmp->B[i] < pa->p.DB )
        {
           if( eCode < 0  )
    	   {
              eCode = i;  // Error state is activated
              pmp->PZ = 3;
              sprintf(buf, "Too small input amount of independent component %-3.3s = %lg",
            	pmp->SB[i], pmp->B[i] );
    		  setErrorMessage( 20, "W20IPM: IPM-main():" ,buf);
    	   }
           else
           {
        	  sprintf(buf,"  %-3.3s = %lg" ,  pmp->SB[i], pmp->B[i] );
        	  addErrorMessage( buf );
           }
           pmp->B[i] = pa->p.DB;
        }
    	if( pmp->B[i] < minB )
           minB = pmp->B[i];      // Looking for the smallest IC amount
    }

    if( eCode >= 0 )
    {
       TProfil::pm->testMulti();
       pmp->PZ = 0;
       setErrorMessage( -1, "" , "");
    }

    sfactor = RescaleToSize( false ); //  replacing calcSfactor();

#ifndef IPMGEMPLUGIN
#ifndef Use_mt_mode
   pVisor->Update(false);
#endif
#endif
   bool AllPhasesPure = true;   // Added by DK on 09.03.2010
   // checking if all phases are pure
   for( k=0; k < pmp->FI; k++ )
       if( pmp->L1[k] > 1 )
           AllPhasesPure = false;
   if( AllPhasesPure == true )  // Provisional
       pmp->pNP = 0;  // call SolveSimplex() also in SIA mode!

   // Analyzing if the Simplex LPP approximation is necessary
    if( !pmp->pNP  )
    {
        // Preparing to call SolveSimplex() - "cold start"
//    	pmp->FitVar[4] = 1.0; // Debugging: no smoothing
//        pmp->FitVar[4] = pa->p.AG;  //  initializing the smoothing parameter
        pmp->ITaia = 0;             // resetting the previous number of AIA iterations
        TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
        //      pmp->IC = 0.0;  For reproducibility of simplex LPP-based IA?
        pmp->PCI = 1.0;
        pmp->logCDvalues[0] = pmp->logCDvalues[1] = pmp->logCDvalues[2] = pmp->logCDvalues[3] =
              pmp->logCDvalues[4] = log( pmp->PCI );  // reset CD sampler array

        // Cleaning vectors of activity coefficients
        for( j=0; j<pmp->L; j++ )
        {
            if( pmp->lnGmf[j] )
                pmp->lnGam[j] = pmp->lnGmf[j]; // setting up fixed act.coeff. for SolveSimplex()
            else pmp->lnGam[j] = 0.;
            pmp->Gamma[j] = 1.;
        }
        for( j=0; j<pmp->L; j++)
            pmp->G[j] = pmp->G0[j] + pmp->lnGam[j];  // Provisory cleanup 4.12.2009 DK
        if( pmp->LO )
        {
           CalculateConcentrations( pmp->X, pmp->XF, pmp->XFA );  // cleanup for aq phase?
           pmp->IC = 0.0;  // Important for the LPP-based AIA reproducibility
           if( pmp->E && pmp->FIat > 0 )
           {
              for( k=0; k<pmp->FIs; k++ )
              {
                 long int ist;
                 if( pmp->PHC[k] == PH_POLYEL || pmp->PHC[k] == PH_SORPTION )
                     for( ist=0; ist<pmp->FIat; ist++ ) // loop over surface types
                     {
                        pmp->XetaA[k][ist] = 0.0;
                        pmp->XetaB[k][ist] = 0.0;
                        pmp->XpsiA[k][ist] = 0.0;
                        pmp->XpsiB[k][ist] = 0.0;
                        pmp->XpsiD[k][ist] = 0.0;
                        pmp->XcapD[k][ist] = 0.0;
                     }  // ist
              }  // k
           } // FIat
        }   // LO
//        if( pa->p.PC == 2 )
//           XmaxSAT_IPM2_reset();  // Reset upper limits for surface species
        pmp->IT = 0; pmp->ITF += 1; // Assuming SolveSimplex() time equal to one iteration of MBR()
//        pmp->PCI = 0.0;
     // Calling the simplex LP approximation here
        AutoInitialApproximation( );
// experimental 15.03.10 (probably correct)
        TotalPhasesAmounts( pmp->Y, pmp->YF, pmp->YFA );
        for( j=0; j< pmp->L; j++ )
            pmp->X[j] = pmp->Y[j];
        TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
        // Calculation of mass-balance residuals
#ifdef Use_qd_real
        if( pa->p.PD > 0 )
#endif
            MassBalanceResiduals( pmp->N, pmp->L, pmp->A, pmp->X, pmp->B, pmp->C);
 #ifdef Use_qd_real
        else
            qdMassBalanceResiduals( pmp->N, pmp->L, pmp->A, pmp->X, pmp->B, pmp->C);
#endif
        CalculateConcentrations( pmp->X, pmp->XF, pmp->XFA );  // also ln activities (DualTh)

#ifndef IPMGEMPLUGIN
        if( pa->p.PC == 1 )
            KarpovsPhaseStabilityCriteria( );  // calculation of Karpov phase stability criteria
        else if( pa->p.PC >= 2 )
            StabilityIndexes(); // calculation of new phase stability indexes
#ifndef Use_mt_mode
   pVisor->Update(false);
#endif
#endif
//  STEPWISE (0) - stop point for examining results from LPP-based IA
#ifndef IPMGEMPLUGIN
STEP_POINT( "End Simplex" );
#endif
        if( AllPhasesPure )     // bugfix DK 09.03.2010   was if(!pmp->FIs)
        {                       // no multi-component phases!
            pmp->W1=0; pmp->K2=0;               // set internal counters
            pmp->Ec = pmp->MK = pmp->PZ = 0;
#ifdef GEMITERTRACE
to_text_file( "MultiDumpLP.txt" );   // Debugging
#endif

#ifndef IPMGEMPLUGIN
   pmp->t_end = clock();
   pmp->t_elap_sec = double(pmp->t_end - pmp->t_start)/double(CLOCKS_PER_SEC);
#ifndef Use_mt_mode
   pVisor->Update( false );
#endif
#endif
           pmp->FI1 = 0;
           pmp->FI1s = 0;
           for( i=0; i<pmp->FI; i++ )
           if( pmp->YF[i] > 1e-18 )
           {
             pmp->FI1++;
             if( i < pmp->FIs )
                pmp->FI1s++;
           }
           return true; // If so, the GEM problem is already solved !
        }
        // Setting default trace amounts to DCs that were zeroed off
        DC_RaiseZeroedOff( 0, pmp->L );
        // this operation greatly affects the accuracy of mass balance!
        TotalPhasesAmounts( pmp->Y, pmp->YF, pmp->YFA );
        for( j=0; j< pmp->L; j++ )
            pmp->X[j] = pmp->Y[j];
        TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
        //        if( pa->p.PC == 2 )
        //           XmaxSAT_IPM2_reset();  // Reset upper limits for surface species
        if( pmp->PD == 2 /* && pmp->Lads==0 */ )
        {
            pmp->FitVar[4] = -1.0;   // To avoid smoothing when F0[j] is calculated first time
            CalculateActivityCoefficients( LINK_UX_MODE);
            pmp->FitVar[4] = 1.0;
        }
#ifdef GEMITERTRACE
to_text_file( "MultiDumpAA.txt" );   // Debugging
#endif
    }
    else  // Taking previous GEMIPM result as an initial approximation
    {
        TotalPhasesAmounts( pmp->Y, pmp->YF, pmp->YFA );
        for( j=0; j< pmp->L; j++ )
            pmp->X[j] = pmp->Y[j];
        TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );

pmp->PCI = 1.; // SD 05/05/2010 for smaller number of iterations for systems with adsorbtion

         pmp->logCDvalues[0] = pmp->logCDvalues[1] = pmp->logCDvalues[2] = pmp->logCDvalues[3] =
   	 pmp->logCDvalues[4] = log( pmp->PCI );  // reset CD sampler array
     if( pmp->PD >= 2 /* && pmp->Lads==0 */ )
        {
                pmp->FitVar[4] = -1.0;   // To avoid smoothing when F0[j] is calculated first time
                CalculateActivityCoefficients( LINK_UX_MODE );
                pmp->FitVar[4] = 1.0;
                // if( pmp->PD >= 3 )
                   // CalculateActivityCoefficients( LINK_PP_MODE );  // Temporarily disabled (DK 06.07.2009)
        }

        if( pmp->pNP <= -1 )
        {  // With raising species and phases zeroed off by SolveSimplex()
           // Setting default trace amounts of DCs that were zeroed off
           DC_RaiseZeroedOff( 0, pmp->L );
        }
     }

// STEPWISE (1) - stop point to see IA from old solution or raised LPP IA
#ifndef IPMGEMPLUGIN
STEP_POINT("Before FIA");
#endif

    return false;
}

// ------------------- ------------------ ----------------
// Calculation of a feasible IPM approximation, refinement of the mass balance
//
// Algorithm: see Karpov, Chudnenko, Kulik 1997 Amer.J.Sci. vol 297 p. 798-799
// (Appendix B)
//
// Parameter: WhereCalledFrom, 0 - at entry after automatic LPP-based IA;
//                             1 - at entry in SIA (start without SolveSimplex()
//                             2 - after post-IPM cleanup
//                             3 - additional (after PhaseSelection)
// Control: MaxResidualRatio, 0 (deactivated), > DHBM and < 1 - accuracy for
//     "trace" independent components (max residual for i should not exceed
//     B[i]*MaxResidualRatio)
// Returns: 0 -  OK,
//          1 -  no SLE colution at the specified precision pa.p.DHB
//          2  - used up more than pa.p.DP iterations
//          3  - too small step length (< 1e-6), no descent possible
//          4  - error in Initial mass balance residuals (debugging)
//          5  - error in MetastabilityLagrangeMultiplier() (debugging)
//
long int TMulti::MassBalanceRefinement( long int WhereCalledFrom )
{
    long int IT1;
    long int I, J, Z,  N, sRet, iRet=0, j, jK;
    double LM, pmp_PCI;
    SPP_SETTING *pa = &TProfil::pm->pa;

    ErrorIf( !pmp->MU || !pmp->W, "MassBalanceRefinement()",
                              "Error of memory allocation for pmp->MU or pmp->W." );

    // calculation of total mole amounts of phases
    TotalPhasesAmounts( pmp->Y, pmp->YF, pmp->YFA );

    if( pmp->PLIM )
        Set_DC_limits(  DC_LIM_INIT );

    // Adjustment of primal approximation according to kinetic constraints
    // Now returns <0 (OK) or index of DC that caused a problem
    jK = MetastabilityLagrangeMultiplier();
    if( jK >= 0 )
    {  // Experimental
        char buf[320];
        sprintf( buf, "(EFD(%ld)) Invalid initial Lagrange multiplier for metastability-constrained DC %16s ",
                 WhereCalledFrom, pmp->SM[jK] );
                setErrorMessage( 17, "E17IPM: MassBalanceRefinement(): ", buf);
      	return 5;
    }


//----------------------------------------------------------------------------
// BEGIN:  main loop
    for( IT1=0; IT1 < pa->p.DP; IT1++, pmp->ITF++ )
    {
        // get size of task
        pmp->NR=pmp->N;
        if( pmp->LO )
        {   if( pmp->YF[0] < pmp->DSM && pmp->YFA[0] < pmp->XwMinM ) // fixed 30.08.2009 DK
                 pmp->NR= pmp->N-1;
        }
        N=pmp->NR;
       // Calculation of mass-balance residuals in IPM
#ifdef Use_qd_real
        if( pa->p.PD > 0 )
#endif
            MassBalanceResiduals( pmp->N, pmp->L, pmp->A, pmp->Y, pmp->B, pmp->C);
#ifdef Use_qd_real
        else
           qdMassBalanceResiduals( pmp->N, pmp->L, pmp->A, pmp->Y, pmp->B, pmp->C);
#endif

       // Testing mass balance residuals
       Z = pmp->N - pmp->E;
       if( !pa->p.DT )
       {   // relative balance accuracy for all ICs
           for( I=0;I<Z;I++ )
             if( fabs(pmp->C[I]) > pmp->B[I] * pmp->DHBM )
               break;
       }
       else { // combined balance accuracy - absolute for major and relative for trace ICs
           double AbsMbAccExp, AbsMbCutoff;
           AbsMbAccExp = (double)abs( pa->p.DT );
           if( AbsMbAccExp < 2. )  // If DT is set to 1 or -1 then DHBM is used also as the absolute cutoff
               AbsMbCutoff = pmp->DHBM;
           else
               AbsMbCutoff = pow( 10, -AbsMbAccExp );
           for( I=0;I<Z;I++ )
              if( fabs( pmp->C[I]) > AbsMbCutoff || fabs(pmp->C[I]) > pmp->B[I] * pmp->DHBM )
                  break;
       }
       if( I == Z ) // balance residuals OK
       { // very experimental - updating activity coefficients
           for( j=0; j< pmp->L; j++ )
               pmp->X[j] = pmp->Y[j];
           TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
           //        if( pa->p.PC == 2 )
           //           XmaxSAT_IPM2_reset();  // Reset upper limits for surface species
           if( pmp->PD == 2 /* && pmp->Lads==0 */ )
           {
//               CalculateConcentrations( pmp->X, pmp->XF, pmp->XFA );
               CalculateActivityCoefficients( LINK_UX_MODE);
           }
           return iRet;       // mass balance refinement finished OK
       }

       WeightMultipliers( true );  // creating R matrix

       // Assembling and solving the system of linearized equations
 #ifdef Use_qd_real
       if( pa->p.PD > 0)
#endif
           sRet = MakeAndSolveSystemOfLinearEquations( N, true );
#ifdef Use_qd_real
       else
           sRet = qdMakeAndSolveSystemOfLinearEquations( N, true );
#endif

       if( sRet == 1 )  // error: no SLE solution!
       {
    	 iRet = 1;
         char buf[320];
         sprintf( buf, "(EFD(%ld)) Degeneration in R matrix (fault in the linearized system solver).\n"
                  "Mass balance cannot be improved, feasible approximation not obtained.",
                  WhereCalledFrom );
         setErrorMessage( 5, "E05IPM: MassBalanceRefinement(): " , buf );
       }

      // SOLVED: solution of linear matrix has been obtained
#ifdef Use_qd_real
       if(pa->p.PD > 0 )
#endif
         //          pmp->PCI = calcDikin( N, true);
         pmp_PCI = DikinsCriterion( N, true);  // calc of MU values and Dikin criterion
#ifdef Use_qd_real
       else
//          pmp->PCI = qdcalcDikin( N, true);
          pmp_PCI = qdDikinsCriterion( N, true);  // calc of MU values and Dikin criterion
#endif

      LM = StepSizeEstimate( true ); // Estimation of the MBR() iteration step size LM

      if( LM < 1e-6 )
      {  // Experimental
          iRet = 3;
          char buf[320];
          sprintf( buf, "(MBR(%ld)) Too small LM step size - cannot converge (check Pa_DG?).",
                    WhereCalledFrom );
          setErrorMessage( 3, "E03IPM: MassBalanceRefinement():", buf );
          break;
       }
      if( LM > 1.)
         LM = 1.;
//      cout << "LM " << LM << endl;

      // calculation of new primal solution approximation
      // from step size LM and the gradient vector MU
      for(J=0;J<pmp->L;J++)
            pmp->Y[J] += LM * pmp->MU[J];

#ifndef IPMGEMPLUGIN
#ifndef Use_mt_mode
  pVisor->Update( false );
#endif
// STEPWISE (5) Stop point at end of iteration of FIA()
STEP_POINT("FIA Iteration");
#endif
}  /* End loop on IT1 */
//----------------------------------------------------------------------------
    //  Prescribed mass balance precision cannot be reached
                    // Temporary workaround for pathological systems 06.05.2010 DK
   if( pa->p.DW && ( WhereCalledFrom == 0L || pmp->pNP ) )  // Now controlled by DW flag
   {  // Strict mode of mass balance control
       iRet = 2;
       char buf[320];
       sprintf( buf, "(MBR(%ld)) Maximum allowed number of MBR() iterations (%ld) exceeded! ",
                WhereCalledFrom, pa->p.DP );
       setErrorMessage( 4, "E04IPM: MassBalanceRefinement(): ", buf );
       return iRet; // no MBR() solution
   }
   // very experimental - updating activity coefficients after MBR()
   for( j=0; j< pmp->L; j++ )
         pmp->X[j] = pmp->Y[j];
   TotalPhasesAmounts( pmp->X, pmp->XF, pmp->XFA );
              //        if( pa->p.PC == 2 )
              //           XmaxSAT_IPM2_reset();  // Reset upper limits for surface species
   if( pmp->PD == 2 /* && pmp->Lads==0 */ )
   {
   //        CalculateConcentrations( pmp->X, pmp->XF, pmp->XFA );
         CalculateActivityCoefficients( LINK_UX_MODE);
   }
   return iRet;   // inaccurate MBR() solution
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Calculation of chemical equilibrium using the Interior Points
//  Method algorithm (see Karpov et al., 1997, p. 785-786)
//  GEM IPM
// Returns: 0, if converged;
//          1, in the case of R matrix degeneration
//          2, (more than max iteration) - no convergence
//              or user's interruption
//          3, CalculateActivityCoefficients() returns bad (non-zero) status
//          4, Mass balance broken  in DualTH (Mol_u)
//     5, Divergence in dual solution u vector has been detected
//
long int TMulti::InteriorPointsMethod( long int &status, long int rLoop )
{
    bool StatusDivg;
    long int N, IT1,J,Z,iRet,i,  nDivIC;
    double LM=0., LM1=1., FX1,    DivTol;
    SPP_SETTING *pa = &TProfil::pm->pa;

    status = 0;
    if( pmp->FIs )
      for( J=0; J<pmp->Ls; J++ )
            pmp->lnGmo[J] = pmp->lnGam[J];

#ifdef Use_qd_real
    if( pa->p.PD > 0 )
#endif
        pmp->FX=GX( LM  );  // calculation of G(x)
#ifdef Use_qd_real
    else
    {
        unsigned int old_cw;
        fpu_fix_start(&old_cw);
        pmp->FX=to_double(qdGX( LM  ));  // calculation of G(x)
        fpu_fix_end(&old_cw);
    }
#endif

    if( pmp->FIs ) // multicomponent phases are present
      for(Z=0; Z<pmp->FIs; Z++)
        pmp->YFA[Z]=pmp->XFA[Z];

//----------------------------------------------------------------------------
//  Main loop of IPM iterations
    for( IT1 = 0; IT1 < pa->p.IIM; IT1++, pmp->IT++, pmp->ITG++ )
    {
        StatusDivg = false;
        pmp->NR=pmp->N;
        if( pmp->LO ) // water-solvent is present
        {
            if( pmp->YF[0]<pmp->DSM && pmp->Y[pmp->LO]< pmp->XwMinM )  // fixed 30.08.2009 DK
                pmp->NR=pmp->N-1;
        }
        N = pmp->NR;

#ifdef GEMITERTRACE
to_text_file( "MultiDumpDC1.txt" );   // Debugging
#endif

        PrimalChemicalPotentials( pmp->F, pmp->Y, pmp->YF, pmp->YFA );

        // Saving previous content of the U vector to Uc vector
        if( pmp->PCI <= pmp->DXM * 10. ) // only at low enough Dikin criterion values
        {
           for(J=0;J<pmp->N;J++)
              pmp->Uc[J][0] = pmp->U[J];
        }
        // Setting weight multipliers for DC
        WeightMultipliers( false );

        // Making and solving the R matrix of IPM linearized equations
#ifdef Use_qd_real
        if( pa->p.PD> 0 )
#endif
            iRet = MakeAndSolveSystemOfLinearEquations( N, false );
#ifdef Use_qd_real
        else
                iRet = qdMakeAndSolveSystemOfLinearEquations( N, false );
#endif
        if( iRet == 1 )
        {
        	setErrorMessage( 7, "E07IPM: IPM-main(): ",
   " Degeneration in R matrix (fault in the linearized system solver).\n"
   " It is not possible to obtain a valid GEMIPM solution.\n"  );
          return 1;
        }

   if( !nCNud && TProfil::pm->pa.p.PLLG )   // disabled if PLLG = 0
   { // Experimental - added 06.05.2011 by DK
      Increment_uDD( pmp->ITG, uDDtrace );
//   DivTol = pow( 10., -fabs( (double)TProfil::pm->pa.p.PLLG ) );
      DivTol = (double)TProfil::pm->pa.p.PLLG;
      if( fabs(DivTol) >= 30000. )
          DivTol = 1e6;  // this is to allow complete tracing in the case of divergence
//      if( pmp->ITG )
//          DivTol /= pmp->ITG;
//       DivTol -= log(pmp->ITG);
//      if( DivTol < 0.3 )
//          DivTol = 0.3;
      // Checking the dual solution for divergence
      if(DivTol < 0. )
         nDivIC = Check_uDD( 0, -DivTol, uDDtrace );
      else
         nDivIC = Check_uDD( 1, DivTol, uDDtrace );

      if( nDivIC )
      { // Printing error message
        char buf[512];
        StatusDivg = true;
        sprintf( buf, "Divergence in dual solution approx. (u) \n at IPM iteration %ld with gen.tolerance %g "
             "for %ld ICs:   %-6.5s", pmp->ITG, DivTol, nDivIC, pmp->SB[ICNud[0]] );
        setErrorMessage( 14, "W14IPM: IPM-main():", buf);
        for( Z =1; Z<nCNud; Z++ )
        {
            sprintf(buf,"%-6.5s",  pmp->SB[ICNud[Z]] );
            addErrorMessage( buf );
        }
      }
   }

// Got the dual solution u vector - calculating the Dikin's Criterion of IPM convergence
#ifdef Use_qd_real
       if( pa->p.PD > 0 )
#endif
           pmp->PCI = DikinsCriterion( N, false );
#ifdef Use_qd_real
        else
            pmp->PCI = qdDikinsCriterion( N, false );
#endif

#ifdef GEMITERTRACE
to_text_file( "MultiDumpDC.txt" );   // Debugging
#endif

       if( StatusDivg )
           return 5L;

       // Initial estimate of IPM descent step size LM
       LM = StepSizeEstimate( false );

#ifdef Use_qd_real
       if( pa->p.PD > 0 )
       {
#endif
           LM1 = OptimizeStepSize( LM ); // Finding an optimal value of the descent step size
           FX1 = GX( LM1 ); // Calculation of the total Gibbs energy of the system G(X)
                          // and copying of Y, YF vectors into X,XF, respectively.
#ifdef Use_qd_real
       }
       else
       {
           LM1=qdOptimizeStepSize( LM ); // Finding an optimal value of the descent step
           unsigned int old_cw;
           fpu_fix_start(&old_cw);
           FX1 = to_double(qdGX( LM1 ));  // Calculation of the total Gibbs energy of the system G(X)
           fpu_fix_end(&old_cw);
       }
#endif
       pmp->FX=FX1;
       // temporary
       for(i=4; i>0; i-- )
            pmp->logCDvalues[i] = pmp->logCDvalues[i-1];
       pmp->logCDvalues[0] = log( pmp->PCI );  // updating CD sampler array

       if( pmp->PHC[0] == PH_AQUEL && ( pmp->XF[0] < pmp->DSM ||
            pmp->X[pmp->LO] <= pmp->XwMinM ))    // fixed 28.04.2010 DK
       {
           pmp->XF[0] = 0.;  // elimination of aqueous phase if too little amount
           pmp->XFA[0] = 0.;
       }

       // Main IPM iteration done
       // Main calculation of activity coefficients
        if( pmp->PD >= 2 || pmp->PD == -2 )
            status = CalculateActivityCoefficients( LINK_UX_MODE );

if( pmp->pNP && status ) // && rLoop < 0  )
{
        setErrorMessage( 18, "E18IPM: IPM-main():", "Bad CalculateActivityCoefficients() status in SIA mode");
	return 3L;
}

#ifndef IPMGEMPLUGIN
#ifndef Use_mt_mode
  pVisor->Update( false );
#endif
// STEPWISE (6)  Stop point at IPM() main iteration
STEP_POINT( "IPM Iteration" );
#endif

        if( pmp->PCI <= pmp->DXM )  // Dikin criterion satisfied - converged!
            goto CONVERGED;
        if( nCNud > 0L && (IT1 >= cnr-2 && IT1 >= 2 ) )  // finish here because u vector diverges at further IPM iterations
            goto CONDITIONALLY_CONVERGED;
        // Restoring vectors Y and YF from X and XF for the next IPM iteration
        Restore_Y_YF_Vectors();
    } // end of the main IPM cycle
    // DXM was not reached in IPM iterations
    setErrorMessage( 6, "E06IPM: IPM-main(): " ,
            "IPM convergence criterion threshold (Pa_DK) could not be reached"
    		" (more than Pa_IIM iterations done);\n" );
    return 2L;  // bad convergence - too many IPM iterations or deterioration of dual solution!
//----------------------------------------------------------------------------
CONVERGED:
// if( !StatusDivg )
   pmp->PCI = pmp->DXM * 0.999999; // temporary - for smoothing
  return 0L;
CONDITIONALLY_CONVERGED:
   pmp->PZ = 5; // Evtl. do something to reconfigure or circumvent PSSC()
  return 0L;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Calculation of mass-balance residuals in GEM IPM CSD
//  Params: N - number of IC in IPM problem
//          L -   number of DC in IPM problem
//          A - DC stoichiometry matrix (LxN)
//          Y - moles  DC quantities in IPM solution (L)
//          B - Input bulk chem. compos. (N)
//          C - mass balance residuals (N)
void TMulti::MassBalanceResiduals( long int N, long int L, double *A, double *Y, double *B,
         double *C )
{
    long int ii, jj, i;
    for(ii=0;ii<N;ii++)
        C[ii]=B[ii];
    for(jj=0;jj<L;jj++)
     for( i=arrL[jj]; i<arrL[jj+1]; i++ )
     {  ii = arrAN[i];
         C[ii]-=(*(A+jj*N+ii))*Y[jj];
     }
//    cout << "MassBalanceResiduals" << endl;
//    for(ii=0;ii<N;ii++)
//        cout << setprecision(16) << scientific<< C[ii] << endl;
}

#ifdef Use_qd_real
void TMulti::qdMassBalanceResiduals( long int N, long int L, double *A, double *Y, double *B,
         double *C )
{
    unsigned int old_cw;
    fpu_fix_start(&old_cw);
    long int ii, jj, i;
    qd_real AjixYj;
    qd_real* qdC = new qd_real[N];
    for(ii=0;ii<N;ii++)
        qdC[ii]=(qd_real)B[ii];
    for(jj=0;jj<L;jj++)
     for( i=arrL[jj]; i<arrL[jj+1]; i++ )
     {  ii = arrAN[i];
         AjixYj = (*(A+jj*N+ii))*Y[jj];
         qdC[ii]-= AjixYj;
     }
//    cout << "qdMassBalanceResiduals" << endl;
    for(ii=0;ii<N;ii++)
    {    C[ii]= to_double(qdC[ii]);
//        cout << setprecision(16) << scientific << C[ii] << "   ";
//        cout << setprecision(20) << scientific << qdC[ii] << endl;
    }
    delete qdC;
    fpu_fix_end(&old_cw);
}
#endif

// Diagnostics for a severe break of mass balance (abs.moles)
// after GEM IPM PhaseSelect() (when pmp->X is passed as parameter)
// Returns -1 (Ok) or index of the first IC for which the balance is
// broken
long int
TMulti::CheckMassBalanceResiduals(double *Y )
{
    double cutoff;
    long int iRet = -1L;
    char buf[300];

        cutoff = min( pmp->DHBM * 1e10, 1e-2 );  // 11.05.2010 DK

#ifdef Use_qd_real
        if(TProfil::pm->pa.p.PD > 0 )
#endif
            MassBalanceResiduals( pmp->N, pmp->L, pmp->A, Y, pmp->B, pmp->C);
#ifdef Use_qd_real
        else
		qdMassBalanceResiduals( pmp->N, pmp->L, pmp->A, Y, pmp->B, pmp->C);
#endif

        for(long int i=0; i<(pmp->N - pmp->E); i++)
	{
           if( fabs( pmp->C[i] ) < cutoff )
	    	  continue;
	   if( iRet < 0  )
	   {
              iRet = i;  // Error state is activated
                  sprintf(buf, "Mass balance is broken on iteration %ld  for ICs %-3.3s",
					     pmp->ITG, pmp->SB[i] );
		  setErrorMessage( 2, "E02IPM: IPM-main():" ,buf);
		}
		else
		{
		 sprintf(buf," %-3.3s" ,  pmp->SB[i] );
		 addErrorMessage( buf );
		 }
        } // i
	return iRet;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Interior Points Method:
// subroutine for unconditional optimization of the descent step length
// on the interval 0 to LM
// uses the "Golden Section" algorithm
// Formerly called LMD()
// Returns: optimal value of LM which provides the largest possible monotonous
// decrease in G(X)
//
double TMulti::OptimizeStepSize( double LM )
{
    double A,B,C,LM1,LM2;
    double FX1,FX2;
    A=0.0;
    B=LM;
    if( LM<2. )
        C=.05*LM;
    else C=.1;
    if( B-A<C)
        goto OCT;
    LM1=A+.382*(B-A);
    LM2=A+.618*(B-A);

    FX1= GX( LM1 );
    FX2= GX( LM2 );

SH1:
    if( FX1>FX2)
        goto SH2;
    else goto SH3;
SH2:
    A=LM1;
    if( B-A<C)
        goto OCT;
    LM1=LM2;
    FX1=FX2;
    LM2=A+.618*(B-A);
    FX2=GX( LM2 );

    goto SH1;
SH3:
    B=LM2;
    if( B-A<C)
        goto OCT;
    LM2=LM1;
    FX2=FX1;
    LM1=A+.382*(B-A);
    FX1=GX( LM1 );
    goto SH1;
OCT:
    LM1=A+(B-A)/2;
    return(LM1);
}

#ifdef Use_qd_real
double TMulti::qdOptimizeStepSize( double LM )
{
    unsigned int old_cw;
    fpu_fix_start(&old_cw);
    double A,B,C,LM1,LM2;
    qd_real FX1,FX2;
    A=0.0;
    B=LM;
    if( LM<2. )
        C=.05*LM;
    else C=.1;
    if( B-A<C)
        goto OCT;
    LM1=A+.382*(B-A);
    LM2=A+.618*(B-A);

    FX1= qdGX( LM1 );
    FX2= qdGX( LM2 );

SH1:
    if( FX1>FX2)
        goto SH2;
    else goto SH3;
SH2:
    A=LM1;
    if( B-A<C)
        goto OCT;
    LM1=LM2;
    FX1=FX2;
    LM2=A+.618*(B-A);
    FX2=qdGX( LM2 );

    goto SH1;
SH3:
    B=LM2;
    if( B-A<C)
        goto OCT;
    LM2=LM1;
    FX2=FX1;
    LM1=A+.382*(B-A);
    FX1=qdGX( LM1 );
    goto SH1;
OCT:
    LM1=A+(B-A)/2;
    fpu_fix_end(&old_cw);
    return(LM1);
}
#endif

//===================================================================

// Cleaning the unstable phase with index k >= 0 (if k < 0 only DC will be cleaned)

void TMulti::DC_ZeroOff( long int jStart, long int jEnd, long int k )
{
  if( k >=0 )
     pmp->YF[k] = 0.;

  for(long int j=jStart; j<jEnd; j++ )
     pmp->Y[j] =  0.0;
}

// Inserting minor quantities of DC which were zeroed off by SolveSimplex()
// (important for the automatic initial approximation with solution phases
//  (k = -1)  or inserting a solution phase after PhaseSelect() (k >= 0)
//
void TMulti::DC_RaiseZeroedOff( long int jStart, long int jEnd, long int k )
{
//  double sfactor = scalingFactor;
//  SPP_SETTING *pa = &TProfil::pm->pa;

//  if( fabs( sfactor ) > 1. )   // can reach 30 at total moles in system above 300000 (DK 11.03.2008)
//	  sfactor = 1.;       // Workaround for very large systems (insertion breaks the EFD convergence)
  if( k >= 0 )
       pmp->YF[k] = 0.;

  for(long int j=jStart; j<jEnd; j++ )
  {
     switch( pmp->DCC[j] )
     {
       case DC_AQ_PROTON:
       case DC_AQ_ELECTRON:
       case DC_AQ_SPECIES:
       case DC_AQ_SURCOMP:
            if( k >= 0 || pmp->Y[j] < pmp->DFYaqM )
               pmp->Y[j] =  pmp->DFYaqM;
           break;
       case DC_AQ_SOLVCOM:
       case DC_AQ_SOLVENT:
            if( k >= 0 || pmp->Y[j] < pmp->DFYwM )
                pmp->Y[j] =  pmp->DFYwM;
            break;
       case DC_GAS_H2O:
       case DC_GAS_CO2:
       case DC_GAS_H2:
       case DC_GAS_N2:
       case DC_GAS_COMP:
       case DC_SOL_IDEAL:
            if( k >= 0 || pmp->Y[j] < pmp->DFYidM )
                  pmp->Y[j] = pmp->DFYidM;
             break;
       case DC_SOL_MINOR:
            if( k >= 0 || pmp->Y[j] < pmp->DFYhM )
                   pmp->Y[j] = pmp->DFYhM;
             break;
       case DC_SOL_MAJOR:
            if( k >= 0 || pmp->Y[j] < pmp->DFYrM )
                  pmp->Y[j] =  pmp->DFYrM;
             break;
       case DC_SCP_CONDEN:
             if( k >= 0 )
             {                // Added 05.11.2007 DK
                 pmp->Y[j] =  pmp->DFYsM;
                 break;
             }
             if( pmp->Y[j] < pmp->DFYcM )
                  pmp->Y[j] =  pmp->DFYcM;
              break;
                    // implementation for adsorption?
       default:
             if( k >= 0 || pmp->Y[j] < pmp->DFYaqM )
                   pmp->Y[j] =  pmp->DFYaqM;
             break;
     }
     if( k >=0 )
     pmp->YF[k] += pmp->Y[j];
   } // i
}

// Adjustment of primal approximation according to kinetic constraints
long int TMulti::MetastabilityLagrangeMultiplier()
{
    double E = TProfil::pm->pa.p.DKIN; //1E-8;  Default min value of Lagrange multiplier p
//    E = 1E-30;

    for(long int J=0;J<pmp->L;J++)
    {
        if( pmp->Y[J] < 0. )   // negative number of moles!
        	return J;
        if( pmp->Y[J] < min( pmp->lowPosNum, pmp->DcMinM ))
            continue;

        switch( pmp->RLC[J] )
        {
        case NO_LIM:
        case LOWER_LIM:
            if( pmp->Y[J]<=pmp->DLL[J])
                pmp->Y[J]=pmp->DLL[J]+E;
            break;
        case BOTH_LIM:
            if( pmp->Y[J]<=pmp->DLL[J])
                pmp->Y[J]=pmp->DLL[J]+E;
            if( pmp->Y[J]>=pmp->DUL[J])     // SD 22/01/2009
            {
                if( pmp->DUL[J] == 1e6 )
                   return J;   // Broken initial approximation!
                pmp->Y[J]=pmp->DUL[J]-E;
                if( pmp->Y[J]<=pmp->DLL[J])
                  	pmp->Y[J]=(pmp->DUL[J]+pmp->DLL[J])/2.;
             }
             break;
        case UPPER_LIM:
            if( pmp->Y[J]>=pmp->DUL[J])
            {
               	if( pmp->DUL[J] == 1e6 )
               	    return J;   // Broken initial approximation!
            	pmp->Y[J]=pmp->DUL[J]-E;
                if( pmp->Y[J]<=0)         // SD 22/01/2009
                	pmp->Y[J]=(pmp->DUL[J])/2.;
            }
            break;
        }
    }   // J
    return -1L;
}

// Calculation of weight multipliers for DCs
void TMulti::WeightMultipliers( bool square )
{
  long int J;
  double  W1, W2;

  for( J=0; J<pmp->L; J++)
  {
    switch( pmp->RLC[J] )
    {
      case NO_LIM:
      case LOWER_LIM:
           W1=(pmp->Y[J]-pmp->DLL[J]);
           if( square )
             pmp->W[J]= W1 * W1;
             else
             pmp->W[J] = max( W1, 0. );
           break;
      case UPPER_LIM:
           W1=(pmp->DUL[J]-pmp->Y[J]);
           if( square )
             pmp->W[J]= W1 * W1;
           else
             pmp->W[J] = max( W1, 0.);
           break;
      case BOTH_LIM:
           W1=(pmp->Y[J]-pmp->DLL[J]);
           W2=(pmp->DUL[J]-pmp->Y[J]);
           if( square )
           {
             W1 = W1*W1;
             W2 = W2*W2;
           }
           pmp->W[J]=( W1 < W2 ) ? W1 : W2 ;
           if( !square && pmp->W[J] < 0. ) pmp->W[J]=0.;
           break;
      default: // error
    	  setErrorMessage( 16, "E16IPM: IPM-main():", "Error in codes of some DC metastability constraints" );
          Error( pmp->errorCode, pmp->errorBuf );
    }
  } // J
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Make and solve a system of linear equations to find the dual vector
// approximation using a method of Cholesky Decomposition (good if a
// square matrix R happens to be symmetric and positive defined).
// If Cholesky Decomposition does not solve the problem, an attempt is done
// to solve the SLE using method of LU Decomposition
// (A = L*U , L is lower triangular ( has elements only on the diagonal and below )
//   U is is upper triangular ( has elements only on the diagonal and above))
// Parameters:
// bool initAppr - Inital approximation point(true) or iteration of IPM (false)
// int N - dimension of the matrix R (number of equations)
// pmp->U - dual solution vector (N).
// Return values: 0  - solved OK;
//                1  - no solution, degenerated or inconsistent system
//
#define  a(j,i) ((*(pmp->A+(i)+(j)*Na)))
long int TMulti::MakeAndSolveSystemOfLinearEquations( long int N, bool initAppr )
{
  long int ii, i, jj, kk, k, Na = pmp->N;
  Alloc_A_B( N );

  // Making the  matrix of IPM linear equations
  for( kk = 0; kk < N; kk++)
   for( ii=0; ii < N; ii++ )
      (*(AA+(ii)+(kk)*N)) = 0.;

  for( jj=0; jj < pmp->L; jj++ )
   if( pmp->Y[jj] > min( pmp->lowPosNum, pmp->DcMinM ) )
   {
      for( k = arrL[jj]; k < arrL[jj+1]; k++)
        for( i = arrL[jj]; i < arrL[jj+1]; i++ )
        { ii = arrAN[i];
          kk = arrAN[k];
          if( ii >= N || kk >= N )
           continue;
          (*(AA+(ii)+(kk)*N)) += a(jj,ii) * a(jj,kk) * pmp->W[jj];
        }
   }

   if( initAppr )
     for( ii = 0; ii < N; ii++ )
         BB[ii] = pmp->C[ii];
   else {
     for( ii = 0; ii < N; ii++ )
         BB[ii] = 0.;
     for( jj=0; jj < pmp->L; jj++ )
        if( pmp->Y[jj] > min( pmp->lowPosNum, pmp->DcMinM ) )
           for( i = arrL[jj]; i < arrL[jj+1]; i++ )
           {  ii = arrAN[i];
              if( ii >= N )
                continue;
              BB[ii] += pmp->F[jj] * a(jj,ii) * pmp->W[jj];
           }
    }

#ifndef PGf90
  Array2D<double> A( N, N, AA );
  Array1D<double> B( N, BB );
#else
  Array2D<double> A( N, N);
  Array1D<double> B( N );

  for( kk = 0; kk < N; kk++)
   for( ii = 0; ii < N; ii++ )
      A[kk][ii] = (*(AA+(ii)+(kk)*N));
   for( ii = 0; ii < N; ii++ )
     B[ii] = BB[ii];
#endif
// From here on, the NIST TNT Jama/C++ linear algebra package is used
//    (credit: http://math.nist.gov/tnt/download.html)
// this routine constructs the Cholesky decomposition, A = L x LT .
  Cholesky<double>  chol(A);

  if( chol.is_spd() )  // is positive definite A.
  {
    B = chol.solve( B );
  }
  else
  {
// no solution by Cholesky decomposition; Trying the LU Decompositon
// The LU decompostion with pivoting always exists, even if the matrix is
// singular, so the constructor will never fail.

   LU<double>  lu(A);

// The primary use of the LU decomposition is in the solution
// of square systems of simultaneous linear equations.
// This will fail if isNonsingular() returns false.
   if( !lu.isNonsingular() )
     return 1; // Singular matrix - too bad! No solution ...

  B = lu.solve( B );
  }

if( initAppr )
{
   for( ii = 0; ii < N; ii++ )
     pmp->Uefd[ii] = B[(int)ii];
}
else {
  for( ii = 0; ii < N; ii++ )
     pmp->U[ii] = B[(int)ii];
}
  return 0;
}

#ifdef Use_qd_real
long int TMulti::qdMakeAndSolveSystemOfLinearEquations( long int N, bool initAppr )
{
  long int ii,i, jj, kk, k, Na = pmp->N;
  Alloc_A_B( N );

  // Making the  matrix of IPM linear equations
  for( kk=0; kk<N; kk++)
   for( ii=0; ii<N; ii++ )
      (*(qdAA+(ii)+(kk)*N)) = 0.;

  for( jj=0; jj<pmp->L; jj++ )
   if( pmp->Y[jj] > min( pmp->lowPosNum, pmp->DcMinM ) )
   {
      for( k=arrL[jj]; k<arrL[jj+1]; k++)
        for( i=arrL[jj]; i<arrL[jj+1]; i++ )
        { ii = arrAN[i];
          kk = arrAN[k];
          if( ii>= N || kk>= N )
           continue;
          (*(qdAA+(ii)+(kk)*N)) += a(jj,ii) * a(jj,kk) * pmp->W[jj];
        }
    }

   if( initAppr )
     for( ii=0; ii<N; ii++ )
         qdBB[ii] = pmp->C[ii];
   else
      {
         for( ii=0; ii<N; ii++ )
            qdBB[ii] = 0.;
          for( jj=0; jj<pmp->L; jj++)
           if( pmp->Y[jj] > min( pmp->lowPosNum, pmp->DcMinM ) )
              for( i=arrL[jj]; i<arrL[jj+1]; i++ )
              {  ii = arrAN[i];
                 if( ii>= N )
                  continue;
                 qdBB[ii] += pmp->F[jj] * a(jj,ii) * pmp->W[jj];
              }
      }

#ifndef PGf90

  for( kk=0; kk<N; kk++)
    for( ii=0; ii<N; ii++ )
       (*(AA+(ii)+(kk)*N))= to_double((*(qdAA+(ii)+(kk)*N)));

    for( ii=0; ii<N; ii++ )
      BB[ii] = to_double(qdBB[ii]);

  Array2D<double> A(N,N, AA);
  Array1D<double> B(N, BB);

#else

  Array2D<double> A(N,N);
  Array1D<double> B(N);

  for( kk=0; kk<N; kk++)
   for( ii=0; ii<N; ii++ )
      A[kk][ii] = to_double((*(qdAA+(ii)+(kk)*N)));

   for( ii=0; ii<N; ii++ )
     B[ii] = to_double(qdBB[ii]);

#endif


// this routine constructs its Cholesky decomposition, A = L x LT .
  Cholesky<double>  chol(A);

  if( chol.is_spd() )  // is positive definite A.
  {
    B = chol.solve( B );
  }
  else
  {
// no solution by Cholesky decomposition; Trying the LU Decompositon
// The LU decompostion with pivoting always exists, even if the matrix is
// singular, so the constructor will never fail.

   LU<double>  lu(A);

// The primary use of the LU decomposition is in the solution
// of square systems of simultaneous linear equations.
// This will fail if isNonsingular() returns false.
   if( !lu.isNonsingular() )
     return 1; // Singular matrix - it's a pity.

  B = lu.solve( B );
  }

  if( initAppr )
  {
     for( ii=0; ii<N; ii++ )
       pmp->Uefd[ii] = B[(int)ii];
  }
  else {
    for( ii=0; ii<N; ii++ )
       pmp->U[ii] = B[(int)ii];
  }
  return 0;
}
#endif
#undef a

// Calculation of MU values (in the vector of direction of descent) and Dikin criterion
// Parameters:
// bool initAppr - use in MassBalanceRefinement() (true) or main iteration of IPM (false)
// int N - dimension of the matrix R (number of equations)
double TMulti::DikinsCriterion(  long int N, bool initAppr )
{
  long int  J;
  double Mu, PCI=0., qMu;

  for(J=0;J<pmp->L;J++)
  {
    if( pmp->Y[J] > min( pmp->lowPosNum, pmp->DcMinM ) )
    {
      if( initAppr )
      {
          Mu = DC_DualChemicalPotential( pmp->Uefd, pmp->A+J*pmp->N, N, J );
          qMu = Mu*pmp->W[J];
          pmp->MU[J] = qMu;
          PCI += qMu*qMu;
//          PCI += sqrt(fabs(qMu));  // Experimental - absolute differences?
      }
      else {
          Mu = DC_DualChemicalPotential( pmp->U, pmp->A+J*pmp->N, N, J );
          Mu -= pmp->F[J];
          qMu =  Mu*pmp->W[J];
          pmp->MU[J] = qMu;
          PCI += fabs(qMu);
//          PCI += qMu*qMu;    // sum of squares (see Chudnenko ea 2001 report)
//          PCI += fabs(qMu*Mu);   // As it was before 2009
      }
    }
    else
      pmp->MU[J]=0.; // initializing dual potentials
  }
  if( initAppr )
  {
     if( PCI > pmp->lowPosNum  )
     {
         PCI=1./sqrt(PCI);
//         PCI = 1./PCI;
//         PCI = 1./PCI/PCI;
     }
         else PCI=1.; // zero Psi value ?
  }
  else {  // if PCI += qMu * qMu
          ;
//      PCI = sqrt( PCI );
//      PCI *= PCI;
  }
//  cout << "DikinsCriterion() ";
// cout << setprecision(20) << scientific << PCI << endl;
  return PCI;
}

#ifdef Use_qd_real
// variant for use with the qd library invoked
double TMulti::qdDikinsCriterion(  long int N, bool initAppr )
{
    unsigned int old_cw;
    fpu_fix_start(&old_cw);
  long int  J;
  double Mu, qMu;
  qd_real PCI=0.;

  for(J=0;J<pmp->L;J++)
  {
    if( pmp->Y[J] > min( pmp->lowPosNum, pmp->DcMinM ) )
    {
      if( initAppr )
      {
          Mu = DC_DualChemicalPotential( pmp->Uefd, pmp->A+J*pmp->N, N, J );
          qMu = Mu*pmp->W[J];
          pmp->MU[J] = qMu;
          PCI += qMu*qMu;
//          PCI += sqrt(fabs(qMu));  // Experimental - absolute differences?
      }
      else {
          Mu = DC_DualChemicalPotential( pmp->U, pmp->A+J*pmp->N, N, J );
          Mu -= pmp->F[J];
          qMu =  Mu*pmp->W[J];
          pmp->MU[J] = qMu;
          PCI += fabs(qMu);
//          PCI += qMu*qMu;    // sum of squares (see Chudnenko ea 2001 report)
//          PCI += fabs(qMu*Mu);   // As it was before 2009
      }
    }
    else
      pmp->MU[J]=0.; // initializing dual potentials
  }
  if( initAppr )
  {
     if( PCI > pmp->lowPosNum  )
     {
         PCI=1./sqrt(PCI);
//         PCI = 1./PCI;
//         PCI = 1./PCI/PCI;
     }
         else PCI=1.; // zero Psi value ?
  }
  else {  // if PCI += qMu * qMu
          ;
//      PCI = sqrt( PCI );
//      PCI *= PCI;
  }
  double PCI_ = to_double(PCI);
//  cout << "qdDikinsCriterion() ";
//  cout << setprecision(20) << scientific << PCI << endl;
  fpu_fix_end(&old_cw);
  return PCI_;
}
#endif

// Estimation of the descent step length LM
// Parameters:
// bool initAppr - MBR() (true) or iteration of IPM (false)
double TMulti::StepSizeEstimate(  bool initAppr )
{
   long int J, Z = -1;
   double LM=1., LM1=1., Mu;

   for(J=0;J<pmp->L;J++)
   {
     Mu = pmp->MU[J];
    if( pmp->RLC[J]==NO_LIM ||
        pmp->RLC[J]==LOWER_LIM || pmp->RLC[J]==BOTH_LIM )
    {
       if( Mu < 0 && fabs(Mu) > pmp->lowPosNum )
       {
         if( Z == -1 )
         { Z = J;
           LM = (-1)*(pmp->Y[Z]-pmp->DLL[Z])/Mu;
         }
         else
         {
           LM1 = (-1)*(pmp->Y[J]-pmp->DLL[J])/Mu;
           if( LM > LM1)
             LM = LM1;
         }
       }
    }
    if( pmp->RLC[J]==UPPER_LIM || pmp->RLC[J]==BOTH_LIM )
    {
       if( Mu > pmp->lowPosNum ) // *100.)
       {
         if( Z == -1 )
         { Z = J;
           LM = (pmp->DUL[Z]-pmp->Y[Z])/Mu;
         }
         else
         {
           LM1=(pmp->DUL[J]-pmp->Y[J])/Mu;
           if( LM>LM1)
               LM=LM1;
         }
      }
    }
  }

  if( initAppr )
  { if( Z == -1 )
     LM = pmp->PCI;
    else
     LM *= .95;     // Smoothing of final lambda value
  }
  else
  {  if( Z == -1 )
       LM = 1./sqrt(pmp->PCI);  // Might cause infinite loop in OptimizeStepSize() if PCI is too low?
//     LM = min( LM, 10./pmp->DX );
       LM = min( LM, 1.0e10 );  // Set an empirical upper limit for LM to prevent freezing
  }
  return LM;
}

// Restoring primal vectors Y and YF
void TMulti::Restore_Y_YF_Vectors()
{
 long int Z, I, JJ = 0;

 for( Z=0; Z<pmp->FI ; Z++ )
 {
   if( pmp->XF[Z] <= pmp->DSM ||
       ( pmp->PHC[Z] == PH_SORPTION &&
       ( pmp->XFA[Z] < TProfil::pm->pa.p.ScMin) ) )
   {
      pmp->YF[Z]= 0.;
      if( pmp->FIs && Z<pmp->FIs )
         pmp->YFA[Z] = 0.;
      for(I=JJ; I<JJ+pmp->L1[Z]; I++)
      {
        pmp->Y[I]=0.;
        pmp->lnGam[I] = 0.;
      }
   }
   else
   {
     pmp->YF[Z] = pmp->XF[Z];
     if( pmp->FIs && Z < pmp->FIs )
        pmp->YFA[Z] = pmp->XFA[Z];
     for(I = JJ; I < JJ+pmp->L1[Z]; I++)
        pmp->Y[I]=pmp->X[I];
   }
   JJ += pmp->L1[Z];
 } // Z

}

// Calculation of the system size scaling factor and modified thresholds/cutoffs/insertion values
// Replaces calcSfactor()
double TMulti::RescaleToSize( bool standard_size )
{
    double SizeFactor=1.;
    SPP_SETTING *pa = &TProfil::pm->pa;

    pmp->SizeFactor = 1.;
//  re-scaling numeric settings
    pmp->DHBM = SizeFactor * pa->p.DHB; // Mass balance accuracy threshold
    pmp->DXM =  SizeFactor * pa->p.DK;   // Dikin' convergence threshold
//    pmp->DX = pa->p.DK;
    pmp->DSM =  SizeFactor * pa->p.DS;   // Cutoff for solution phase amount
// Cutoff amounts for DCs
    pmp->XwMinM = SizeFactor * pa->p.XwMin;  // cutoff for the amount of water-solvent
    pmp->ScMinM = SizeFactor * pa->p.ScMin;  // cutoff for amount of the sorbent
    pmp->DcMinM = SizeFactor * pa->p.DcMin;  // cutoff for Ls set (amount of solution phase component)
    pmp->PhMinM = SizeFactor * pa->p.PhMin;  // cutoff for single-comp.phase amount and its DC
  // insertion values before SolveSimplex() (re-scaled to system size)
    pmp->DFYwM = SizeFactor * pa->p.DFYw;
    pmp->DFYaqM = SizeFactor * pa->p.DFYaq;
    pmp->DFYidM = SizeFactor * pa->p.DFYid;
    pmp->DFYrM = SizeFactor * pa->p.DFYr;
    pmp->DFYhM = SizeFactor * pa->p.DFYh;
    pmp->DFYcM = SizeFactor * pa->p.DFYc;
    // Insertion value for PhaseSelection()
    pmp->DFYsM = SizeFactor * pa->p.DFYs; // pure condenced phase and its DC

    return SizeFactor;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Internal memory allocation for IPM performance optimization
// (since version 2.2.0)
//
void TMulti::Alloc_A_B( long int newN )
{
  if( AA && BB && (newN == sizeN) )
    return;
  Free_A_B();
  AA = new  double[newN*newN];
  BB = new  double[newN];
 #ifdef Use_qd_real
  qdAA = new  qd_real[newN*newN];
  qdBB = new  qd_real[newN];
#endif
  sizeN = newN;
}

void TMulti::Free_A_B()
{
  if( AA  )
    { delete[] AA; AA = 0; }
  if( BB )
    { delete[] BB; BB = 0; }
#ifdef Use_qd_real
  if( qdAA  )
    { delete[] qdAA; qdAA = 0; }
  if( qdBB )
    { delete[] qdBB; qdBB = 0; }
#endif
  sizeN = 0;
}

// Building an index list of non-zero elements of the matrix pmp->A
#define  a(j,i) ((*(pmp->A+(i)+(j)*pmp->N)))
void TMulti::Build_compressed_xAN()
{
 long int ii, jj, k;

 // Calculate number of non-zero elements in A matrix
 k = 0;
 for( jj=0; jj<pmp->L; jj++ )
   for( ii=0; ii<pmp->N; ii++ )
     if( fabs( a(jj,ii) ) > 1e-12 )
       k++;

   // Free old memory allocation
    Free_compressed_xAN();

   // Allocate memory
   arrL = new long int[pmp->L+1];
   arrAN = new long int[k];

   // Set indexes in the index arrays
   k = 0;
   for( jj=0; jj<pmp->L; jj++ )
   { arrL[jj] = k;
     for( ii=0; ii<pmp->N; ii++ )
       if( fabs( a(jj,ii) ) > 1e-12 )
       {
        arrAN[k] = ii;
        k++;
       }
   }
   arrL[jj] = k;
}
#undef a

void TMulti::Free_compressed_xAN()
{
  if( arrL  )
    { delete[] arrL; arrL = 0;  }
  if( arrAN )
    { delete[] arrAN; arrAN = 0;  }
}

void TMulti::Free_internal()
{
  Free_compressed_xAN();
  Free_A_B();
}

void TMulti::Alloc_internal()
{
// optimization 08/02/2007
 Alloc_A_B( pmp->N );
 Build_compressed_xAN();
}

// add09
void TMulti::setErrorMessage( long int num, const char *code, const char * msg)
{
  long int len_code, len_msg;
  pmp->Ec  = num;
  len_code = strlen(code);
  if(len_code > 99)
      len_code = 99;
  strncpy( pmp->errorCode, code, len_code );
  pmp->errorCode[len_code] ='\0';
  len_msg = strlen(msg);
  if(len_msg > 1023)
      len_msg = 1023;
  strncpy( pmp->errorBuf,  msg,  len_msg );
  pmp->errorBuf[len_msg] ='\0';
}

void TMulti::addErrorMessage( const char * msg)
{
  long int len = strlen(pmp->errorBuf);
  long int lenm = strlen( msg );
  if( len + lenm < 1023 )
  {
    strcpy(pmp->errorBuf+len, msg ); // , lenm  );
//    pmp->errorBuf[len+lenm] ='\0';
  }
}

// Added for implementation of divergence detection in dual solution 06.05.2011 DK
void TMulti::Alloc_uDD( long int newN )
{
    if( U_mean && U_M2 && U_CVo && U_CV && ICNud && (newN == nNu) )
      return;
    Free_uDD();
    U_mean = new  double[newN]; // w3 u mean values for r
    U_M2 = new  double[newN];   // w3 u mean values for r-1
    U_CVo = new  double[newN];  // w3 u mean difference for r-1
    U_CV = new  double[newN];   // w3 u mean difference for r
    ICNud = new long int[newN];
    nNu = newN;
}

void TMulti::Free_uDD()
{
    if( U_mean  )
      { delete[] U_mean; U_mean = 0; }
    if( U_M2  )
      { delete[] U_M2; U_M2 = 0; }
    if( U_CVo  )
      { delete[] U_CVo; U_CVo = 0; }
    if( U_CV  )
      { delete[] U_CV; U_CV = 0; }
    if( ICNud )
      { delete[] ICNud; ICNud = 0; }
    nNu = 0;
}

// initializing data for u divergence detection
void TMulti::Reset_uDD( long int nr, bool trace )
{
    long int i;
    cnr = nr;
    for( i=0; i<nNu; i++)
    {
      U_mean[i] = 0.; U_M2[i] = 0.;
      U_CVo[i] = 0.; U_CV[i] = 0;
      ICNud[i] = -1L;
    }
    nCNud = 0;
    if ( trace )
    {
       cout << " UD3 trace: " << pmp->stkey << " SIA= " << pmp->pNP << endl;
       cout << " Itr   C_D:   " << pmp->SB1[0] ;
    }
    if( TProfil::pm->pa.p.PSM >= 3 )
    {
      fstream f_log("ipmlog.txt", ios::out|ios::app );
      f_log << " UD3 trace: " << pmp->stkey << " SIA= " << pmp->pNP << endl;
      f_log << " Itr   C_D:   " << pmp->SB1[0] ;
    }
}

// incrementing mean u values for r-th (current) IPM iteration
void TMulti::Increment_uDD( long int r, bool trace )
{
    long int i;
    double delta;
    cnr = r; // r+1;
    if( cnr == 0 )
        return;
    if( TProfil::pm->pa.p.PSM >= 3 )
    {
       fstream f_log("ipmlog.txt", ios::out|ios::app );
       f_log << r << " " << pmp->PCI << " ";
    }
    if( trace )
       cout << r << " " << pmp->PCI << " ";

    for( i=0; i<nNu; i++)
    {
// Calculating moving average of three u_i values
      switch( cnr )
      {
          case 1: U_mean[i] = pmp->U[i];
                  U_M2[i] = U_mean[i];
                  U_CV[i] = 0.;
                  pmp->Uc[i][0] = pmp->U[i];
                  pmp->Uc[i][1] = pmp->U[i];
                  break;
          case 2: U_M2[i] = U_mean[i];
                  U_mean[i] = (pmp->U[i] + pmp->Uc[i][0] + pmp->Uc[i][0] )/3.;
                  pmp->Uc[i][1] = pmp->Uc[i][0];
                  pmp->Uc[i][0] = pmp->U[i];
                  break;
          default:U_M2[i] = U_mean[i];
                  U_mean[i] = (pmp->U[i] + pmp->Uc[i][0] + pmp->Uc[i][1])/3.;
                  pmp->Uc[i][1] = pmp->Uc[i][0];
                  pmp->Uc[i][0] = pmp->U[i];
                  break;
      }
      U_CVo[i] = U_CV[i];
      U_CV[i] = U_mean[i] - U_M2[i];
      delta = fabs(U_CV[i] - U_CVo[i]);
      if( trace )
      {
//      cout << pmp->U[i] << " ";
        cout << U_mean[i] << " ";
//      cout << U_CV[i] << " ";
//      cout << delta << " ";
      }
      if( TProfil::pm->pa.p.PSM >= 3 )
      {
          fstream f_log("ipmlog.txt", ios::out|ios::app );
          f_log << U_mean[i] << " ";
  //      f_log << pmp->U[i] << " ";
  //      f_log << U_CV[i] << " ";
  //      f_log << delta << " ";
      }
//      delta = pmp->U[i] - U_mean[i];
//      U_mean[i] += delta / cnr;
//      U_M2[i] += delta * ( pmp->U[i] - U_mean[i] );
//      if( cnr > 2 )
//          U_CVo[i] = U_CV[i];
//      U_CV[i] = sqrt( U_M2[i]/cnr ) / fabs( U_mean[i] );
//      if( cnr < 2 )
//          U_CVo[i] = U_mean[i];
      // Copy of dual solution approximation
//      if( cnr < 2 )
//          pmp->Uc[i][0] = pmp->U[i];
//
    } // end for i
//  if( trace )
//      cout << endl;
}

// Checking for divergence in coef.variation of dual solution approximation
// Compares with CV value tolerance (mode = 0) or with CV increase
//          tolerance (mode = 1)
// returns:  0 if no divergence has been detected
//          >0 - number of diverging dual chemical potentials
//            (their IC names are collected in the ICNud list)
//
long int TMulti::Check_uDD( long int mode, double DivTol,  bool trace )
{
    long int i;
    double delta, tol_gen, tolerance, log_bi;
    bool FirstTime = true;
    char buf[MAXICNAME+2];

    //    tolerance = DivTol / fabs(log(pmp->PCI));
    tol_gen = DivTol;
    if( pmp->PCI < 1 )
    tol_gen *= pmp->PCI;
    if( TProfil::pm->pa.p.PSM >= 3 )
    {
        fstream f_log("ipmlog.txt", ios::out|ios::app );
        f_log << " Tol= " << tol_gen << " |" << endl;
    }
    if( trace )
          cout << " Tol= " << tol_gen << " |" << endl;
    if( cnr <= 1 )
        return 0;

  //  Check here that pmp->PCI is reasonable (i.e. C_D < 1)?
  //
    for( i=0; i<nNu; i++)
    {    
      if( DivTol >= 1e6 )
          continue;     // Disabling divergence checks for complete tracing
      // Checking absolute ranges of u[i] - to be checked for 'exotic' systems!
      if( i == nNu-1 && pmp->E && pmp->U[i] >= -50. && pmp->U[i] <= 100.) // charge
          continue;
      else if( pmp->U[i] >= -600. && pmp->U[i] <= 400. ) // range for other ICs
      {
        tolerance = tol_gen;
        log_bi = log( pmp->B[i] ) - 4.6;
        if( log_bi > 1. )
            tolerance = tol_gen / log_bi;
        if( log_bi < -1. )
             tolerance = tol_gen * -log_bi;
        if( tolerance < 1. )
            tolerance = 1.;     // To prevent too low tolerances DK 17.06.2011
        if( !mode ) // Monitor difference between new and old mean3 u_i
        {
            // Calculation of abs.difference of moving averages at r and r-1
            delta = fabs(U_mean[i] - U_M2[i]);
            if( delta <= tolerance || cnr <= 2 )
                continue;
        }
        if( mode ) // Monitor the difference between differences between new and old mean3 u_i
        {
            // Calculation of abs.difference of moving average differences at r and r-1
            delta = fabs(U_CV[i] - U_CVo[i]);
            if( delta <= tolerance || cnr <= 2 )
                 continue;
        }
      }
      // Divergence detected
      ICNud[nCNud++] = i;
      if( FirstTime )
      {
         if( trace )
            cout << "uDD ITG= " << pmp->ITG << " Tol= " << tol_gen << " |" << " Divergent ICs: ";
         if( TProfil::pm->pa.p.PSM >= 3 )
         {
            fstream f_log("ipmlog.txt", ios::out|ios::app );
            f_log << "uDD ITG= " << pmp->ITG << " Tol= " << tol_gen << " |" << " Divergent ICs: ";
         }
         FirstTime = false;
      }
      if( trace )
      {
          memcpy(buf, pmp->SB[i], MAXICNAME );
          buf[MAXICNAME] = '\0';
          cout << buf << " ";
      }
      if( TProfil::pm->pa.p.PSM >= 3 )
      {
          fstream f_log("ipmlog.txt", ios::out|ios::app );
          memcpy(buf, pmp->SB[i], MAXICNAME );
          buf[MAXICNAME] = '\0';
          f_log << buf << " ";
      }
    } // for i
    if( !FirstTime )
    {    if( trace )
           cout << " |" << endl;
         if( TProfil::pm->pa.p.PSM >= 3 )
         {
             fstream f_log("ipmlog.txt", ios::out|ios::app );
             f_log << " |" << endl;
         }
    }
    return nCNud;
}

//--------------------- End of ipm_main.cpp ---------------------------
