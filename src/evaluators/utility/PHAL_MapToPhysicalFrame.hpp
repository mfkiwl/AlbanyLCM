// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef PHAL_MAPTOPHYSICALFRAME_HPP
#define PHAL_MAPTOPHYSICALFRAME_HPP

#include "Albany_Layouts.hpp"
#include "Albany_ScalarOrdinalTypes.hpp"
#include "Intrepid2_CellTools.hpp"
#include "Intrepid2_Cubature.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_Utilities.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_MDField.hpp"
#include "Phalanx_config.hpp"

namespace PHAL {
/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates coordinates at vertices
    to quad points.

*/

template <typename EvalT, typename Traits>
class MapToPhysicalFrame : public PHX::EvaluatorWithBaseImpl<Traits>, public PHX::EvaluatorDerived<EvalT, Traits>
{
 public:
  MapToPhysicalFrame(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl);

  void
  postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& vm);

  void
  evaluateFields(typename Traits::EvalData d);

 private:
  typedef typename EvalT::ScalarT     ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  int numQPs, numDim;

  // Input:
  Teuchos::RCP<Intrepid2::Basis<PHX::Device, RealType, RealType>> intrepidBasis;
  //! Values at vertices
  PHX::MDField<const MeshScalarT, Cell, Vertex, Dim> coords_vertices;
  Teuchos::RCP<Intrepid2::Cubature<PHX::Device>>     cubature;
  Teuchos::RCP<shards::CellTopology>                 cellType;

  Kokkos::DynRankView<RealType, PHX::Device> refPoints;
  Kokkos::DynRankView<RealType, PHX::Device> refWeights;

  // Output:
  //! Values at quadrature points
  PHX::MDField<MeshScalarT, Cell, QuadPoint, Dim> coords_qp;
};
}  // namespace PHAL

#endif
