#ifdef with_petsc

#ifndef _PETSCTIMEINTEGRATOR_H_
#define _PETSCTIMEINTEGRATOR_H_

#include <string>
#include "ParmParse.H"
#include "CH_HDF5.H"
#include "parstream.H"
#include "REAL.H"

/* include PETSc header files */
#include <petscsys.h>
#include <petscvec.h>
#include <petscmat.h>
#include <petscksp.h>
#include <petscsnes.h>
#include <petscts.h>

#include "NamespaceHeader.H"

template <class System>
class PetscTimeIntegrator
{
  public: 
    PetscTimeIntegrator<System>(ParmParse&,bool);
    ~PetscTimeIntegrator<System>();

    void finalize();
    void solve() { TSSolve(m_ts, NULL); }

    void  copyStateToPetsc    (Vec& a_Y, System* a_System);
    void  copyStateFromPetsc  (Vec& a_Y, System* a_System);
    void  addStateFromPetsc   (Vec& a_Y, System* a_System, Real a_scale = 1.0);
    void  copyRHSToPetsc      (Vec& a_Y, System* a_System);
    void  copyRHSFromPetsc    (Vec& a_Y, System* a_System);

    inline System* getSystem()        { return m_system; }
    inline int     getVerbosity()     { return m_verbosity; }
    inline int     getPlotInterval()  { return m_plot_interval; }
    inline int     getChkptInterval() { return m_checkpoint_interval; }
    inline Real    getShift()         { return m_shift; }
    inline Real    getTime()          { return m_cur_time; }
    inline Real    getEpsilonJFNK()   { return m_epsilon_jfnk; }
    inline bool    isLinear()         { return m_is_linear; }

    inline void    setLastPlot  (int a_int) { m_last_plot = a_int; }      
    inline void    setLastChkpt (int a_int) { m_last_checkpoint = a_int; }      

    inline void    setTime            (Real a_t)    { m_cur_time = a_t; }
    inline void    setTimeStep        (Real a_dt)   { m_dt = a_dt; }
    inline void    setTimeStepNumber  (int a_iter)  { m_cur_step = a_iter; }
    inline void    setShift           (Real a_a)    { m_shift = a_a; }

    void writeHistFile(bool);
    void writePlotFile();
    void writeCheckpointFile();

    Vec   m_Yref, m_Fref;

  private:

    void parseParametersSimulation( ParmParse& );
    void parseParametersPetsc     ( ParmParse& );

    System      *m_system;
    int         m_cur_step;
    int         m_max_step;
    Real        m_cur_time;
    Real        m_max_time;
    Real        m_dt;
    int         m_verbosity;
    int         m_plottedonce;

    int         m_checkpoint_interval;
    int         m_last_checkpoint;
    std::string m_checkpoint_prefix;

    int         m_plot_interval;
    int         m_last_plot;
    std::string m_plot_prefix;

    int         m_local_size;

    std::string m_ts_type, 
                m_ts_subtype,
                m_ts_adapt_type;

    Real  m_shift,
          m_epsilon_jfnk,
          m_snes_atol,
          m_snes_rtol,
          m_snes_stol,
          m_ksp_atol,
          m_ksp_rtol;

    bool  m_is_linear,
          m_ts_monitor,
          m_ts_adapt_monitor,
          m_snes_monitor,
          m_ksp_monitor,
          m_ts_arkimex_fully_implicit;
    
    Vec   m_Y;
    TS    m_ts;
    Mat   m_Jac;
};

static PetscErrorCode RHSFunction(TS ts, PetscReal t, Vec Y, Vec F, void* ctxt)
{
  PetscTimeIntegrator<GKSystem> *context = (PetscTimeIntegrator<GKSystem>*) ctxt;
  GKSystem *system = context->getSystem();

  context->copyStateFromPetsc(Y,system);
  system->computeRHSFunctionExp((Real)t);
  context->copyRHSToPetsc(F,system);

  return(0);
}

static PetscErrorCode RHSFunctionIMEX(TS ts, PetscReal t, Vec Y, Vec F, void* ctxt)
{
  PetscTimeIntegrator<GKSystem> *context = (PetscTimeIntegrator<GKSystem>*) ctxt;
  GKSystem *system = context->getSystem();

  context->copyStateFromPetsc(Y,system);
  system->computeRHSFunctionImEx((Real)t);
  context->copyRHSToPetsc(F,system);

  return(0);
}

static PetscErrorCode IFunctionIMEX(TS ts, PetscReal t, Vec Y, Vec Ydot, Vec F, void* ctxt)
{
  PetscTimeIntegrator<GKSystem> *context = (PetscTimeIntegrator<GKSystem>*) ctxt;
  GKSystem *system = context->getSystem();

  context->copyStateFromPetsc(Y,system);
  system->computeIFunctionImEx((Real)t);
  context->copyRHSToPetsc(F,system);

  VecCopy(Y,context->m_Yref);
  VecCopy(F,context->m_Fref);

  VecAYPX(F,-1.0,Ydot);

  return(0);
}

static PetscErrorCode IJacobianIMEX(TS ts, PetscReal t, Vec Y, Vec Ydot, PetscReal a,
                                    Mat A, Mat B, void* ctxt)
{
  PetscTimeIntegrator<GKSystem> *context = (PetscTimeIntegrator<GKSystem>*) ctxt;

  context->setShift((Real)a);
  context->setTime((Real)t);

  return(0);
}

static PetscErrorCode JacobianFunctionIMEX(Mat A, Vec Y, Vec F)
{
  PetscTimeIntegrator<GKSystem> *context;
  MatShellGetContext(A,&context);
  GKSystem *system = context->getSystem();

  PetscReal normY;
  VecNorm(Y,NORM_2,&normY);

  if (normY < 1e-15) {

    VecZeroEntries(F);
    VecAXPBY(F,context->getShift(),0,Y);

  } else {

    Real epsilon;
    if (context->isLinear()) epsilon = 1.0;
    else                     epsilon = context->getEpsilonJFNK() / normY;

    context->copyStateFromPetsc(context->m_Yref,system);
    context->addStateFromPetsc(Y,system,epsilon);
    system->computeIFunctionImEx(context->getTime());
    context->copyRHSToPetsc(F,system);
    VecAXPY(F,-1.0,context->m_Fref);
    VecAXPBY(F,(PetscReal)context->getShift(),(-1.0/epsilon),Y);
  }

  return(0);
}

static PetscErrorCode PreTimeStep(TS ts)
{
  PetscTimeIntegrator<GKSystem> *context;
  TSGetApplicationContext(ts,&context);

  int cur_step;
  Real cur_time, dt;

  TSGetTime(ts,&cur_time);
  TSGetTimeStep(ts,&dt);
  TSGetTimeStepNumber(ts,&cur_step);

  if (context->getVerbosity() >= 1) {
    if (procID()==0) cout << endl << "Step " << cur_step+1 << endl;
  }
  return(0);
}

static PetscErrorCode PostTimeStep(TS ts)
{
  PetscTimeIntegrator<GKSystem> *context;
  TSGetApplicationContext(ts,&context);
  GKSystem *system = context->getSystem();

  int cur_step, 
      verbosity = context->getVerbosity(),
      plot_interval = context->getPlotInterval();
  Real cur_time, dt;

  TSGetTime           (ts,&cur_time);
  TSGetTimeStep       (ts,&dt);
  TSGetTimeStepNumber (ts,&cur_step);

  context->setTime(cur_time);
  context->setTimeStep(dt);
  context->setTimeStepNumber(cur_step);

  system->printTimeStep(cur_step,dt);

  if (verbosity >= 1) {
    system->printDiagnostics();
    if(procID()==0) cout << "Step " << cur_step << " completed, simulation time is "
                         << cur_time << endl << "----" << endl;
  }

  if ( (cur_step % plot_interval)==0 ) {
    context->writePlotFile();
    context->setLastPlot(cur_step);
  }

  context->writeHistFile(false);

  if ( (cur_step % context->getChkptInterval())==0 ) {
    context->writeCheckpointFile();
    context->setLastChkpt(cur_step);
  }

  return(0);
}

static PetscErrorCode TSMonitor(TS ts,PetscInt step,PetscReal time,Vec u,void *ctx)
{
  PetscPrintf(PETSC_COMM_WORLD,"  ** TS iter = %D, Time %g **\n",step,(double)time);
}

static PetscErrorCode SNESMonitor(SNES snes, PetscInt its,PetscReal fnorm,void *ctx)
{
  PetscPrintf(PETSC_COMM_WORLD,"  **  SNES iter = %D, SNES Function norm %g **\n",its,(double)fnorm);
  return(0);
}

static PetscErrorCode KSPMonitor(KSP ksp, PetscInt n,PetscReal rnorm,void *ctxt)
{
  PetscPrintf(PETSC_COMM_WORLD,"  **    KSP iter = %D, KSP Relative norm %g **\n",n,(double)rnorm);
  return(0);
}

template <class System>
inline void PetscTimeIntegrator<System>::writeHistFile(bool flag)
{
  m_system->writeFieldHistory(m_cur_step, m_cur_time,flag);
}

template <class System> 
inline void PetscTimeIntegrator<System>::writeCheckpointFile()
{
#ifdef CH_USE_HDF5
   char iter_str[100];

   sprintf( iter_str, "%s%04d.%dd.hdf5",
            m_checkpoint_prefix.c_str(), m_cur_step, SpaceDim );

   HDF5Handle handle( iter_str, HDF5Handle::CREATE );
   m_system->writeCheckpointFile( handle, m_cur_step, m_cur_time, m_dt );
   handle.close();
#else
   MayDay::Error( "restart only defined with hdf5" );
#endif
}

template <class System> 
inline void PetscTimeIntegrator<System>::writePlotFile()
{
#ifdef CH_USE_HDF5
   ParmParse ppsim( "simulation" );
   parseParametersSimulation( ppsim );

   char iter_str[100];
   if ( ppsim.contains( "restart_file" )&& m_plottedonce == 0 ) {
      sprintf( iter_str, "restart.%s", m_plot_prefix.c_str());
      m_plottedonce = 1;
   } else {
      sprintf( iter_str, "%s", m_plot_prefix.c_str());
   }
   m_system->writePlotFile( iter_str, m_cur_step, m_cur_time);
#else
   MayDay::Error( "plotting only defined with hdf5" );
#endif
}

template <class System>
PetscTimeIntegrator<System>::PetscTimeIntegrator( ParmParse &a_pp, bool usePetsc)
  : m_cur_step(0),
    m_max_step(0),
    m_cur_time(0.0),
    m_max_time(0.0),
    m_dt(-1.0),
    m_verbosity(0),
    m_checkpoint_interval(0),
    m_last_checkpoint(0),
    m_checkpoint_prefix( "chk" ),
    m_plot_interval(0),
    m_last_plot(0),
    m_plot_prefix( "plt" ),
    m_plottedonce(0),
    m_system(NULL),
    m_local_size(0),
    m_ts_type("rk"),
    m_ts_subtype("4"),
    m_ts_adapt_type("none"),
    m_is_linear("true"),
    m_epsilon_jfnk(1e-6),
    m_ts_monitor(false),
    m_ts_adapt_monitor(false),
    m_snes_monitor(false),
    m_ksp_monitor(false),
    m_snes_atol(1e-6),
    m_snes_rtol(1e-6),
    m_snes_stol(1e-14),
    m_ksp_atol(1e-6),
    m_ksp_rtol(1e-6),
    m_ts_arkimex_fully_implicit(false)
{
  ParmParse ppsim( "simulation" );
  parseParametersSimulation( ppsim );
  ParmParse pppet( "petsc" );
  parseParametersPetsc( pppet );

  m_system = new System(a_pp, usePetsc);
  m_system->initialize(m_cur_step);
  m_system->printDiagnostics();
  if ( m_plot_interval >= 0 ) {
    writePlotFile();
    m_last_plot = m_cur_step;
  }
  writeHistFile(true);
  m_is_linear = m_system->isLinear();
  
  if (!procID()) {
    cout << "Using PETSc time integrator: " << m_ts_type;
    cout << " (" << m_ts_subtype << ")\n";
  }

  /* create and initialize the PETSc solution vector */
  m_local_size = m_system->getVectorSize();
  VecCreate(MPI_COMM_WORLD,&m_Y);
  VecSetSizes(m_Y,m_local_size,PETSC_DECIDE);
  VecSetUp(m_Y);

  /* set up reference vectors */
  VecDuplicate(m_Y,&m_Yref);
  VecDuplicate(m_Y,&m_Fref);

  /* copy initial solution to PETSc vector */
  copyStateToPetsc(m_Y,m_system);

  /* create and initialize the PETSc time integration object */
  TSCreate(MPI_COMM_WORLD,&m_ts);
  TSSetDuration(m_ts,m_max_step,m_max_time);
  TSSetInitialTimeStep(m_ts,0.0,m_dt);
  TSSetExactFinalTime(m_ts,TS_EXACTFINALTIME_MATCHSTEP);
  TSSetSolution(m_ts,m_Y);
  TSSetApplicationContext(m_ts,this);

  /* Set time step adapter */
  TSAdapt adapt; TSGetAdapt(m_ts,&adapt);
  TSAdaptSetType(adapt,m_ts_adapt_type.c_str());

  TSSetType(m_ts,m_ts_type.c_str());
  if (!strcmp(m_ts_type.c_str(),TSRK))            TSRKSetType(m_ts,m_ts_subtype.c_str());
  else if (!strcmp(m_ts_type.c_str(),TSARKIMEX))  TSARKIMEXSetType(m_ts,m_ts_subtype.c_str());

  if (!strcmp(m_ts_type.c_str(),TSARKIMEX)) {

    TSSetRHSFunction(m_ts,PETSC_NULL,RHSFunctionIMEX,this);
    TSSetIFunction  (m_ts,PETSC_NULL,IFunctionIMEX  ,this);

    MatCreateShell(MPI_COMM_WORLD,m_local_size,m_local_size,PETSC_DETERMINE,PETSC_DETERMINE,this,&m_Jac);
    MatShellSetOperation(m_Jac,MATOP_MULT,(void (*)(void))JacobianFunctionIMEX);
    MatSetUp(m_Jac);
    TSSetIJacobian(m_ts,m_Jac,m_Jac,IJacobianIMEX,this);
    
    SNES  snes; TSGetSNES(m_ts,&snes);
    KSP   ksp;  SNESGetKSP(snes,&ksp);
    PC    pc;   KSPGetPC(ksp,&pc);
    PCSetType(pc,PCNONE);

    if (m_snes_monitor) SNESMonitorSet(snes,SNESMonitor,NULL,NULL);
    if (m_ksp_monitor)  KSPMonitorSet(ksp,KSPMonitor,NULL,NULL);

    if (m_ts_arkimex_fully_implicit) TSARKIMEXSetFullyImplicit(m_ts,PETSC_TRUE);

  } else {

    TSSetRHSFunction(m_ts,PETSC_NULL,RHSFunction,this);

  }

  if (m_ts_monitor) TSMonitorSet(m_ts,TSMonitor,NULL,NULL);
  if (m_ts_adapt_monitor) TSAdaptSetMonitor(adapt,PETSC_TRUE);

  /* set pre- and post-step functions */
  TSSetPreStep(m_ts,PreTimeStep);
  TSSetPostStep(m_ts,PostTimeStep);

  TSSetUp(m_ts);
}

template <class System>
void PetscTimeIntegrator<System>::copyStateToPetsc( Vec& a_Y, System* a_System )
{
  PetscErrorCode  ierr;
  PetscScalar     *Yarr;

  ierr = VecGetArray(a_Y,&Yarr);
  a_System->copyStateToArray((Real*)Yarr);
  ierr = VecRestoreArray(a_Y,&Yarr);
}

template <class System>
void PetscTimeIntegrator<System>::copyStateFromPetsc( Vec& a_Y, System* a_System )
{
  PetscErrorCode    ierr;
  const PetscScalar *Yarr;

  ierr = VecGetArrayRead(a_Y,&Yarr);
  a_System->copyStateFromArray((Real*)Yarr);
  ierr = VecRestoreArrayRead(a_Y,&Yarr);
}

template <class System>
void PetscTimeIntegrator<System>::addStateFromPetsc( Vec& a_Y, System* a_System, Real a_scale )
{
  PetscErrorCode    ierr;
  const PetscScalar *Yarr;

  ierr = VecGetArrayRead(a_Y,&Yarr);
  a_System->addStateFromArray((Real*)Yarr,a_scale);
  ierr = VecRestoreArrayRead(a_Y,&Yarr);
}

template <class System>
void PetscTimeIntegrator<System>::copyRHSToPetsc( Vec& a_Y, System* a_System )
{
  PetscErrorCode  ierr;
  PetscScalar     *Yarr;

  ierr = VecGetArray(a_Y,&Yarr);
  a_System->copyRHSToArray((Real*)Yarr);
  ierr = VecRestoreArray(a_Y,&Yarr);
}

template <class System>
void PetscTimeIntegrator<System>::copyRHSFromPetsc( Vec& a_Y, System* a_System )
{
  PetscErrorCode    ierr;
  const PetscScalar *Yarr;

  ierr = VecGetArrayRead(a_Y,&Yarr);
  a_System->copyRHSFromArray((Real*)Yarr);
  ierr = VecRestoreArrayRead(a_Y,&Yarr);
}

template <class System>
void PetscTimeIntegrator<System>::parseParametersSimulation( ParmParse& a_ppsim )
{
   // This determines the amount of diagnositic output generated
   a_ppsim.query( "verbosity", m_verbosity );
   CH_assert( m_verbosity >= 0 );

   // Stop after this number of steps
   a_ppsim.query( "max_step", m_max_step );
   CH_assert( m_max_step >= 0 );

   // Stop when the simulation time get here
   a_ppsim.query( "max_time", m_max_time );
   CH_assert( m_max_time >= 0.0 );

   a_ppsim.query( "fixed_dt", m_dt );
   CH_assert( m_dt>0.0 );

   // Set up checkpointing
   a_ppsim.query( "checkpoint_interval", m_checkpoint_interval );
   a_ppsim.query( "checkpoint_prefix", m_checkpoint_prefix );

   // Set up plot file writing
   a_ppsim.query( "plot_interval", m_plot_interval );
   a_ppsim.query( "plot_prefix", m_plot_prefix );
}

template <class System>
void PetscTimeIntegrator<System>::parseParametersPetsc( ParmParse& a_pppet )
{
   // This determines the amount of diagnositic output generated
   a_pppet.query("ts_type", m_ts_type );
   a_pppet.query("ts_subtype", m_ts_subtype );
   a_pppet.query("ts_adapt_type", m_ts_adapt_type );
   if (!strcmp(m_ts_type.c_str(),TSARKIMEX)) 
     a_pppet.query("ts_arkimex_fully_implicit", m_ts_arkimex_fully_implicit);
   a_pppet.query("ts_adapt_type", m_ts_adapt_type );
   a_pppet.query("epsilon_epsilon", m_epsilon_jfnk );
   a_pppet.query("ts_monitor", m_ts_monitor );
   a_pppet.query("ts_adapt_monitor", m_ts_monitor );
   a_pppet.query("snes_monitor", m_snes_monitor );
   a_pppet.query("ksp_monitor", m_ksp_monitor );
   a_pppet.query("ksp_atol", m_ksp_atol );
   a_pppet.query("ksp_rtol", m_ksp_rtol );
   a_pppet.query("snes_atol", m_snes_atol );
   a_pppet.query("snes_rtol", m_snes_rtol );
   a_pppet.query("snes_stol", m_snes_stol );
}

template <class System>
void PetscTimeIntegrator<System>::finalize()
{
  /* Get the final solution and copy from PETSc vector to system */
  TSGetSolution(m_ts,&m_Y);
  copyStateFromPetsc(m_Y,m_system);

  TSGetTime(m_ts,&m_cur_time);
  TSGetTimeStep(m_ts,&m_dt);
  TSGetTimeStepNumber(m_ts,&m_cur_step);

  if ( (m_plot_interval >= 0) && (m_last_plot!=m_cur_step) ) {
     writePlotFile();
  }

  if ( m_last_checkpoint!=m_cur_step ) {
     writeCheckpointFile();
  }

  if (!procID()) {
    cout << "PETSc time integration completed: ";
    cout << "final time=" << m_cur_time << ", ";
    cout << "steps=" << m_cur_step <<".\n";
  }
}

template <class System>
PetscTimeIntegrator<System>::~PetscTimeIntegrator()
{
  TSDestroy (&m_ts);
  MatDestroy(&m_Jac);
  VecDestroy(&m_Y);
  VecDestroy(&m_Yref);
  VecDestroy(&m_Fref);
}

#include "NamespaceFooter.H"

#endif /*_PETSCTIMEINTEGRATOR_H_ */
#endif /* with_petsc */