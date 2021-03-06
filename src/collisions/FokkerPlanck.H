#ifndef  _FOKKERPLANCK_H_
#define  _FOKKERPLANCK_H_

#include "BandedMatrix.H"
#include "FArrayBox.H"
#include "LevelData.H"
#include "RefCountedPtr.H"
#include "Kernels.H"
#include "KineticSpecies.H"
#include "KineticFunction.H"
#include "REAL.H"
#include "GlobalDOF.H"
#include "CLSInterface.H"
#include "ParmParse.H"
#include <sstream>

#include "NamespaceHeader.H"


/**
 * FokkerPlanck collisions operator class.
*/
class FokkerPlanck
   : public CLSInterface
{
public:

  /// Constructor with initialization.
  /**
   *  @param[in] pp the input database.
   */
  FokkerPlanck( ParmParse&  a_ppcls,
        const int a_verbosity );

  /// Destructor.
  /**
   */
  virtual ~FokkerPlanck();

  /// Accumulates the RHS of the collision operator.
  /**
   * @param[out] rhs data holder for rhs (without ghost cells).
   * @param[in] soln current solution with which to evaluate RHS.
   * @param[in] species index of species to which collision is applied.
   * @param[in] time current time.
   */
  virtual void evalClsRHS( KineticSpeciesPtrVect&       rhs,
                           const KineticSpeciesPtrVect& soln,
                           const int                    species,
                           const Real                   time,
                           const int                    flag = 1);

  /// Compute a stable time step.
  /**
   * Computes and returns an estimate of the maximum stable time step.
   */
  Real computeDt(const KineticSpeciesPtrVect& soln);

  /// Report that collision operator is nonlinear
  /**
   * Reports that this collision operator is nonlinear
   */
  inline bool isLinear() { return false; }

  inline int precondMatrixBands() { return(m_nbands); }

  void assemblePrecondMatrix( void*,
                              const KineticSpecies&,
                              const GlobalDOFKineticSpecies& );

  inline void preTimeStep  ( const KineticSpeciesPtrVect& a_soln_mapped, 
                             const int a_species, 
                             const Real a_time,
                             const KineticSpeciesPtrVect& a_soln_physical)
    { 
      computeReferenceSolution  ( a_soln_physical, a_species, a_time);
      computePotentialsAndCoeffs( a_soln_mapped, a_species, a_time, true ); 
      m_it_counter+=1;
      m_first_step = false;
    }

  inline void postTimeStage( const KineticSpeciesPtrVect& a_soln, 
                             const int a_species, 
                             const Real a_time,
                             const int a_stage)
    { 
      if (a_stage) {
        computePotentialsAndCoeffs( a_soln, a_species, a_time, false ); 
      }
    }

private:

   int m_verbosity;
   ParmParse m_ppcls;

   bool m_fixed_cls_freq;
   Real m_cls_freq;
   Real m_cls_norm;
   Real m_pcg_tol;
   Real m_pcg_maxiter;
   int  m_mult_num;
   int  m_nD;

   int  m_update_freq;
   int  m_it_counter;

   bool m_first_step;
   bool m_subtract_background;

   LevelData<FArrayBox> m_F0;
   LevelData<FArrayBox> m_phi;
   LevelData<FArrayBox> m_phi_F0;
   LevelData<FArrayBox> m_D;
   LevelData<FArrayBox> m_D_F0;

   int m_nbands;

   RefCountedPtr<KineticFunction> m_ref_func;

   bool m_debug, m_rosenbluth_skip_stage, m_compute_maxwellian;

   void computePotentialsAndCoeffs(const KineticSpeciesPtrVect&, const int, const Real, const bool);

   void computeReferenceSolution(const KineticSpeciesPtrVect&, const int, const Real);
   
   /// Computes the Rosenbluth potentials

   void evalRosenbluthPotentials( LevelData<FArrayBox>& phi,
                                  const PhaseGeom& phase_geom,
                                  const LevelData<FArrayBox>& dfn,
                                  const double mass ) const;

  /// Computes the coefficients D_v, D_mu, D_v_v, D_v_mu, D_mu_mu
  //  from the Rosenbluth potentials
  
  void evalCoefficients( LevelData<FArrayBox>& D,
                         const LevelData<FArrayBox>& phi,
                         const PhaseGeom& phase_geom,
                         const double mass_tp,
                         const double mass_fp) const;
  

  /// Computes the collision flux 

   void computeFlux( LevelData<FluxBox>& flux,
                     const PhaseGeom& phase_geom,
                     const LevelData<FArrayBox>& D,
                     const LevelData<FArrayBox>& dfn ) const;

   void computeFluxCellCentered(  LevelData<FArrayBox>& flux,
                                  const PhaseGeom& phase_geom,
                                  const LevelData<FArrayBox>& D,
                                  const LevelData<FArrayBox>& dfn,
                                  const LevelData<FArrayBox>& dfn_ref) const;

   void computeDivergence(  LevelData<FArrayBox>&,
                            const PhaseGeom&,
                            const LevelData<FArrayBox>&) const;

  ///Computes the normalization factor 
  /**
   * Computes the normalization factor 
   * 
   * cls_norm = dfn_norm[s^3/m^6] * tau_norm[s] * 
   *            Coulomb_Lg * a_charge^4 * ech[SI]^4 / (a_mass * M[kg] * eps0 )^2
   *
   * dfn_norm = (1/Pi) * N[1/m^3] / (T[J]/M[kg])^(3/2)
   *
   * tau_norm = L[m] / Sqrt(T[J]/M[kg])
   * 
   */
  void computeClsNorm(Real&                       a_cls_norm,
                      const double                a_mass,
                      const double                a_charge      ) const;


  /// Fourth-order conversion utilities

  void convertToCellAverage(const PhaseGeom&  coord_sys,
                            LevelData<FArrayBox>&   dfn) const;

  void convertToCellCenters(const PhaseGeom&  coord_sys,
                            LevelData<FArrayBox>&  dfn) const;
  
  void convertToCellFaces(LevelData<FluxBox>& faceData,
                          const LevelData<FArrayBox>& cellData,
                          const PhaseGeom& coord_sys) const;

  void FillGhostCells(const PhaseGeom&  coord_sys,
                            LevelData<FArrayBox>&   dfn) const;

   
  void evalClsRHS_Main( KineticSpeciesPtrVect&       rhs,
                        const KineticSpeciesPtrVect& soln,
                        const int                    species,
                        const Real                   time,
                        const int                    flag);
  void evalClsRHS_LowOrder( KineticSpeciesPtrVect&       rhs,
                            const KineticSpeciesPtrVect& soln,
                            const int                    species,
                            const Real                   time,
                            const int                    flag);
   /// Parse parameters.
   /**
    * Private method to obtain control parameters from "CLS.species" section
    * of ParmParse database, where "species" is replaced by the species name
    *
    * \param[in] ppcls CLS.species input database.
    */
   void parseParameters( ParmParse& pp );
   
   /// Print parameters.
   /**
    * Private method to print parameters.
    */
   void printParameters();
   
};

#include "NamespaceFooter.H"

#endif
