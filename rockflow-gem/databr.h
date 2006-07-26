//-------------------------------------------------------------------
// DATABRIDGE - defines the structure of node-dependent data for
// exchange between the coupled GEM IPM and FMT code parts.
// Requires DATACH.H
// Used in Tnode and Tnodearray classes
//
//      CH: chemical structure in GEM IPM
//      FMT: fluid mass transport
//
// Written by D.Kulik, W.Pfingsten, F.Enzmann and S.Dmytriyeva
// Copyright (C) 2003-2006
//
// This file is part of GEMIPM2K and GEMS-PSI codes for
// thermodynamic modelling by Gibbs energy minimization
// developed by the Laboratory of Waste Management, Paul Scherrer Institute
// This file may be distributed together with GEMIPM2K source code
// under the licence terms defined in GEMIPM2K.QAL
//
// See also http://les.web.psi.ch/Software/GEMS-PSI
// E-mail: gems2.support@psi.ch
//-------------------------------------------------------------------
//

#ifndef _DataBr_H_
#define _DataBr_H_

typedef struct
{  // DATABR - template node data bridge structure
   short
     NodeHandle,    // Node identification handle
     NodeTypeHY,    // Node type (hydraulic); see typedef NODETYPE
     NodeTypeMT,    // Node type (mass transport); see typedef NODETYPE
     NodeStatusFMT, // Node status code FMT; see typedef NODECODEFMT
     NodeStatusCH,  // Node status code CH;  see typedef NODECODECH
     IterDone;      // Number of iterations performed by IPM

/*  these important dimensions are provided in the DATACH structure
   unsigned short
    nICb,       // number of stoichiometry units (<= nIC) used in the data bridge
    nDCb,      	// number of DC (chemical species, <= nDC) used in the data bridge
    nPHb,     	// number of phases (<= nPH) used in the data bridge
    nPSb;       // number of multicomponent phases (<= nPS) used in the data bridge
*/

//      Usage of this variable (DB - data bridge)        MT-DB DB-GEM GEM-DB DB-MT
   double
// Chemical scalar variables
    T,      	// Temperature T, K                        +      +      -     -
    P, 	        // Pressure P, bar                         +      +      -     -
    Vs,         // Volume V of reactive subsystem, cm3     -      -      +     +
    Vi,         // Volume of inert subsystem, cm3          +      -      -     +
    Ms,         // Mass of reactive subsystem,  g          +      +      -     -
    Mi,         // Mass of inert subsystem, g              +      -      -     +

    Gs,         // Gibbs energy of reactive subsystem, J   -      -      +     +
    Hs, 	// Enthalpy of reactive subsystem, J       -      -      +     +
    Hi,         // Enthalpy of inert subsystem, J          +      -      -     +

    IC,     // Effective aqueous ionic strength, molal     -      -      +     +
    pH,     // pH of aqueous solution                      -      -      +     +
    pe,     // pe of aqueous solution                      -      -      +     +
    Eh,     // Eh of aqueous solution, V                   -      -      +     +

//  FMT variables (units need dimensionsless form) - to be used for storing them
//  at the nodearray level, normally not used in the single-node FMT-GEM coupling
    Tm,         // actual total simulation time, s
    dt,         // actual time step
    Dif,        // General diffusivity of disolved matter in the mode
    Vt,		// total volume of the node (voxel), m3
    vp,		// advection velocity (in pores) in this node
    eps,	// effective (actual) porosity normalized to 1
    Km,		// actual permeability, m2
    Kf,		// actual DARCY`s constant, m2/s
    S,		// specific storage coefficient, dimensionless
    Tr,         // transmissivity m2/s
    h,		// actual hydraulic head (hydraulic potential), m
    rho,	// actual carrier density for density-driven flow, g/cm3
    al,		// specific longitudinal dispersivity of porous media, m
    at,		// specific transversal dispersivity of porous media, m
    av,		// specific vertical dispersivity of porous media, m
    hDl,	// hydraulic longitudinal dispersivity, m2/s, diffusities from chemical database
    hDt,	// hydraulic transversal dispersivity, m2/s
    hDv,	// hydraulic vertical dispersivity, m2/s
    nto;	// tortuosity factor

// Data arrays - dimensions see in DATACH.H structures
// exchange of values occurs through lists of indices, e.g. xDC, xPH

//      Usage of this variable (DB - data bridge)        MT-DB DB-GEM GEM-DB DB-MT
   double
// IC (stoichiometry units)
    *bIC,  // bulk mole amounts of IC[nICb]                +      +      -     -
    *rMB,  // MB Residuals from GEM IPM [nICb]             -      -      +     +
    *uIC,  // IC chemical potentials (mol/mol)[nICb]       -      -      +     +
// DC (species) in reactive subsystem
    *xDC,  // DC mole amounts at equilibrium [nDCb]        -      -      +     +
    *gam,  // activity coeffs of DC [nDCb]                 -      -      +     +
// Metastability/kinetic controls
    *dul,  // upper kinetic restrictions [nDCb]            +      +      -     -
    *dll,  // lower kinetic restrictions [nDCb]            +      +      -     -
// Phases in reactive subsystem
*aPH,  // Specific surface areas of phases (m2/g)          +      +      -     -
    *xPH,  // total mole amounts of phases [nPHb]          -      -      +     +
    *vPS,  // phase volume, cm3/mol        [nPSb]          -      -      +     +
    *mPS,  // phase (carrier) mass, g      [nPSb]          -      -      +     +
    *bPS,  // bulk compositions of phases  [nPSb][nICb]    -      -      +     +
    *xPA,  // amount of carrier in phases  [nPSb] ??       -      -      +     +

  // What else?
    *dRes1;
}
DATABR;

typedef DATABR*  DATABRPTR;

typedef enum {  // NodeStatus codes with respect to GEMIPM calculations
 NEED_GEM_AIA = 1,   // GEM calculation starts with simplex initial approximation (IA)
 OK_GEM_AIA   = 2,   // OK after GEM calculation with simplex IA
 BAD_GEM_AIA  = 3,   // Bad result after GEM calculation with simplex IA
 ERR_GEM_AIA  = 4,   // Failure in GEM calculation with simplex IA
 NEED_GEM_PIA = 5,   // GEM calculation starts without simplex using IA from previous solution
 OK_GEM_PIA   = 6,   // OK after GEM calculation without simplex IA
 BAD_GEM_PIA  = 7,   // Bad result after GEM calculation without simplex IA
 ERR_GEM_PIA  = 8,   // Failure in after GEM calculation without simplex IA
 TERROR_GEM   = 9    // Terminal error occurred in GEMIPM2K
} NODECODECH;

typedef enum {  // NodeStatus codes FluidMassTransport

 Initial_RUN   = 1,
 OK_Hydraulic  = 2,
 BAD_Hydraulic = 3,  // insufficient convergence
 OK_Transport  = 4,
 BAD_Transport = 5,  // insufficient convergence
 NEED_RecalcMT = 6,
 OK_MassBal    = 7,
 OK_RecalcPar  = 8,
 Bad_Recalc    = 9,
 OK_Time       = 10
} NODECODEFMT;

typedef enum {  // Node Type codes controlling hydraulic/masstransport behavior
 normal       = 0, // normal node
// boundary condition node
  NBC1source   = 1, // Dirichlet source ( constant concentration )
  NBC1sink    = -1, // Dirichlet sink
  NBC2source   = 2, // Neumann source ( constant gradient )
  NBC2sink    = -2, // Neumann sink
  NBC3source   = 3, // Cauchy source ( constant flux )
  NBC3sink    = -3, // Cauchy sink
  INIT_FUNK    = 4  // functional initial conditions (e.g. input time depended functions)
} NODETYPE;

#endif   //_DataBr_h

