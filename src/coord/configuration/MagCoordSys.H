#ifndef MAGCOORDSYS_H
#define MAGCOORDSYS_H

#include "MagBlockCoordSys.H"
#include "MultiBlockCoordSys.H"
#include "PotentialBC.H"

#include "NamespaceHeader.H"

class MagCoordSys : public MultiBlockCoordSys
{
public:

  /**
     \name Constructors, destructor and defines
  */
  /*@{*/

  ///
  /** Default constructor.
  */
   MagCoordSys() {;}

  ///
  /** Destructor.
   */
  virtual ~MagCoordSys();

  /*@}*/

   /// Return the mapping which corresponds to the block with the given index.
   virtual const MagBlockCoordSys* getCoordSys(int a_n) const {return (const MagBlockCoordSys*)m_coordSysVect[a_n];}

   /// Return the mapping which corresponds to the given box.
   /// \param a_box The Box for which the coordinate system is retrieved. This
   ///              Box must be contained completely within a block or this
   ///              method will fail.
   virtual const MagBlockCoordSys* getCoordSys(const Box& a_box) const
     { return (const MagBlockCoordSys*)m_coordSysVect[whichBlock(a_box)]; }

   void defineCoordSystemsAndBoundaries( Vector<MagBlockCoordSys *>& geom_vec);

   virtual void defineBoundaries() = 0;

   /// general block mapping conversion function
   /** Given a_xiSrc in mapped space of block a_nSrc,
       and block index a_nDst,
       returns coordinates in mapped space of block a_nDst.
   */
   virtual RealVect blockRemappingGeneral(int a_nDst,
                                          const RealVect& a_xiSrc,
                                          int a_nSrc) const;

   virtual IntVect getDecomposition(int block) const = 0;

   virtual void postProcessMetricData( LevelData<FArrayBox>& data ) const {;}

   virtual void postProcessMetricData( LevelData<FluxBox>& data ) const {;}

   virtual const PotentialBC& getDivergenceCleaningBC() const = 0;

   virtual void definePotentialBC( PotentialBC& bc ) const {};

};



#include "NamespaceFooter.H"

#endif
