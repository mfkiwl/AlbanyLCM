// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_Macros.hpp"
#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Teuchos_VerboseObject.hpp"
// uncomment the following line if you want debug output to be printed to screen

namespace PHAL {

template <typename EvalT, typename Traits>
ComputeBasisFunctionsSide<EvalT, Traits>::ComputeBasisFunctionsSide(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
{
  // Get side set name and side set layouts
  sideSetName = p.get<std::string>("Side Set Name");
  ALBANY_PANIC(dl->side_layouts.find(sideSetName) == dl->side_layouts.end(), "Error! Layouts for side set '" << sideSetName << "' not found.\n");
  Teuchos::RCP<Albany::Layouts> dl_side = dl->side_layouts.at(sideSetName);

  // Build output fields
  sideCoordVec = decltype(sideCoordVec)(p.get<std::string>("Side Coordinate Vector Name"), dl_side->vertices_vector);
  tangents     = decltype(tangents)(p.get<std::string>("Tangents Name"), dl_side->qp_tensor_cd_sd);
  metric       = decltype(metric)(p.get<std::string>("Metric Name"), dl_side->qp_tensor);
  w_measure    = decltype(w_measure)(p.get<std::string>("Weighted Measure Name"), dl_side->qp_scalar);
  inv_metric   = decltype(inv_metric)(p.get<std::string>("Inverse Metric Name"), dl_side->qp_tensor);
  metric_det   = decltype(metric_det)(p.get<std::string>("Metric Determinant Name"), dl_side->qp_scalar);
  BF           = decltype(BF)(p.get<std::string>("BF Name"), dl_side->node_qp_scalar);
  GradBF       = decltype(GradBF)(p.get<std::string>("Gradient BF Name"), dl_side->node_qp_gradient);

  this->addDependentField(sideCoordVec);
  this->addEvaluatedField(tangents);
  this->addEvaluatedField(metric);
  this->addEvaluatedField(metric_det);
  this->addEvaluatedField(w_measure);
  this->addEvaluatedField(inv_metric);
  this->addEvaluatedField(BF);
  this->addEvaluatedField(GradBF);

  compute_normals = p.isParameter("Side Normal Name");
  if (compute_normals) {
    normals  = decltype(normals)(p.get<std::string>("Side Normal Name"), dl_side->qp_vector_spacedim);
    coordVec = decltype(coordVec)(p.get<std::string>("Coordinate Vector Name"), dl->vertices_vector);
    numNodes = dl->node_gradient->extent(1);
    this->addEvaluatedField(normals);
    this->addDependentField(coordVec);
  }

  cellType = p.get<Teuchos::RCP<shards::CellTopology>>("Cell Type");

  // Get Dimensions
  int numCells = dl_side->node_qp_gradient->extent(0);
  numSides     = dl_side->node_qp_gradient->extent(1);
  numSideNodes = dl_side->node_qp_gradient->extent(2);
  numSideQPs   = dl_side->node_qp_gradient->extent(3);
  numCellDims  = dl_side->vertices_vector->extent(3);  // Vertices vector always has the ambient space dimension
  numSideDims  = numCellDims - 1;

  cubature      = p.get<Teuchos::RCP<Intrepid2::Cubature<PHX::Device>>>("Cubature Side");
  intrepidBasis = p.get<Teuchos::RCP<Intrepid2::Basis<PHX::Device, RealType, RealType>>>("Intrepid Basis Side");

  this->setName("ComputeBasisFunctionsSide" + PHX::print<EvalT>());
}

//**********************************************************************
template <typename EvalT, typename Traits>
void
ComputeBasisFunctionsSide<EvalT, Traits>::postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(sideCoordVec, fm);
  this->utils.setFieldData(tangents, fm);
  this->utils.setFieldData(metric, fm);
  this->utils.setFieldData(metric_det, fm);
  this->utils.setFieldData(w_measure, fm);
  this->utils.setFieldData(inv_metric, fm);
  this->utils.setFieldData(BF, fm);
  this->utils.setFieldData(GradBF, fm);

  if (compute_normals) {
    this->utils.setFieldData(normals, fm);
    this->utils.setFieldData(coordVec, fm);
  }

  // Allocate Temporary Kokkos Views
  cub_points         = Kokkos::DynRankView<RealType, PHX::Device>("XXX", numSideQPs, numSideDims);
  cub_weights        = Kokkos::DynRankView<RealType, PHX::Device>("XXX", numSideQPs);
  val_at_cub_points  = Kokkos::DynRankView<RealType, PHX::Device>("XXX", numSideNodes, numSideQPs);
  grad_at_cub_points = Kokkos::DynRankView<RealType, PHX::Device>("XXX", numSideNodes, numSideQPs, numSideDims);
  cub_weights        = Kokkos::DynRankView<RealType, PHX::Device>("XXX", numSideQPs);

  // Pre-Calculate reference element quantitites
  cubature->getCubature(cub_points, cub_weights);

  intrepidBasis->getValues(val_at_cub_points, cub_points, Intrepid2::OPERATOR_VALUE);
  intrepidBasis->getValues(grad_at_cub_points, cub_points, Intrepid2::OPERATOR_GRAD);

  // BF does not depend on the current element, so we fill it now
  std::vector<PHX::DataLayout::size_type> dims;
  BF.fieldTag().dataLayout().dimensions(dims);
  int numCells = dims[0];
  for (int cell = 0; cell < numCells; ++cell) {
    for (int side = 0; side < numSides; ++side) {
      for (int node = 0; node < numSideNodes; ++node) {
        for (int qp = 0; qp < numSideQPs; ++qp) {
          BF(cell, side, node, qp) = val_at_cub_points(node, qp);
        }
      }
    }
  }

  cellsOnSides.resize(numSides);
  numCellsOnSide.resize(numSides, 0);
  for (int i = 0; i < numSides; i++) cellsOnSides[i] = Kokkos::DynRankView<int, PHX::Device>("cellOnSide_i", numCells);

  d.fill_field_dependencies(this->dependentFields(), this->evaluatedFields());
}

//**********************************************************************
template <typename EvalT, typename Traits>
void
ComputeBasisFunctionsSide<EvalT, Traits>::evaluateFields(typename Traits::EvalData workset)
{
  // TODO: use Intrepid routines as much as possible
  if (workset.sideSets->find(sideSetName) == workset.sideSets->end()) return;

  numCellsOnSide.assign(numSides, 0);
  std::vector<Albany::SideStruct> const& sideSet = workset.sideSets->at(sideSetName);
  for (auto const& it_side : sideSet) {
    // Get the local data of side and cell
    int const cell = it_side.elem_LID;
    int const side = it_side.side_local_id;

    cellsOnSides[side](numCellsOnSide[side]++) = cell;

    // Computing tangents (the basis for the manifold)
    for (int itan = 0; itan < numSideDims; ++itan) {
      for (int icoor = 0; icoor < numCellDims; ++icoor) {
        for (int qp = 0; qp < numSideQPs; ++qp) {
          tangents(cell, side, qp, icoor, itan) = 0.;
          for (int node = 0; node < numSideNodes; ++node) {
            tangents(cell, side, qp, icoor, itan) += sideCoordVec(cell, side, node, icoor) * grad_at_cub_points(node, qp, itan);
          }
        }
      }
    }
    // Computing the metric
    for (int qp = 0; qp < numSideQPs; ++qp) {
      for (int idim = 0; idim < numSideDims; ++idim) {
        // Diagonal
        metric(cell, side, qp, idim, idim) = 0.;
        for (int coor = 0; coor < numCellDims; ++coor) {
          metric(cell, side, qp, idim, idim) += tangents(cell, side, qp, coor, idim) * tangents(cell, side, qp, coor, idim);  // g = J'*J
        }

        // Extra-diagonal
        for (int jdim = idim + 1; jdim < numSideDims; ++jdim) {
          metric(cell, side, qp, idim, jdim) = 0.;
          for (int coor = 0; coor < numCellDims; ++coor) {
            metric(cell, side, qp, idim, jdim) += tangents(cell, side, qp, coor, idim) * tangents(cell, side, qp, coor, jdim);  // g = J'*J
          }
          metric(cell, side, qp, jdim, idim) = metric(cell, side, qp, idim, jdim);
        }
      }
    }

    // Computing the metric determinant, the weighted measure and the inverse of
    // the metric
    switch (numSideDims) {
      case 1:
        for (int qp = 0; qp < numSideQPs; ++qp) {
          metric_det(cell, side, qp)       = metric(cell, side, qp, 0, 0);
          w_measure(cell, side, qp)        = cub_weights(qp) * std::sqrt(metric(cell, side, qp, 0, 0));
          inv_metric(cell, side, qp, 0, 0) = 1. / metric(cell, side, qp, 0, 0);
        }
        break;
      case 2:
        for (int qp = 0; qp < numSideQPs; ++qp) {
          metric_det(cell, side, qp) =
              metric(cell, side, qp, 0, 0) * metric(cell, side, qp, 1, 1) - metric(cell, side, qp, 0, 1) * metric(cell, side, qp, 1, 0);
          w_measure(cell, side, qp)        = cub_weights(qp) * std::sqrt(metric_det(cell, side, qp));
          inv_metric(cell, side, qp, 0, 0) = metric(cell, side, qp, 1, 1) / metric_det(cell, side, qp);
          inv_metric(cell, side, qp, 1, 1) = metric(cell, side, qp, 0, 0) / metric_det(cell, side, qp);
          inv_metric(cell, side, qp, 0, 1) = inv_metric(cell, side, qp, 1, 0) = -metric(cell, side, qp, 0, 1) / metric_det(cell, side, qp);
        }
        break;
      default: ALBANY_ABORT("Error! The dimension of the side should be 1 or 2.\n");
    }

    for (int node = 0; node < numSideNodes; ++node) {
      for (int qp = 0; qp < numSideQPs; ++qp) {
        for (int ider = 0; ider < numSideDims; ++ider) {
          GradBF(cell, side, node, qp, ider) = 0;
          for (int jder = 0; jder < numSideDims; ++jder)
            GradBF(cell, side, node, qp, ider) += inv_metric(cell, side, qp, ider, jder) * grad_at_cub_points(node, qp, jder);
        }
      }
    }
  }

  if (compute_normals) {
    for (int side = 0; side < numSides; ++side) {
      int numCells_ = numCellsOnSide[side];
      if (numCells_ == 0) continue;

      Kokkos::DynRankView<MeshScalarT, PHX::Device> normal_lengths =
          Kokkos::createDynRankView(sideCoordVec.get_view(), "normal_lengths", numCells_, numSideQPs);
      Kokkos::DynRankView<MeshScalarT, PHX::Device> normals_view =
          Kokkos::createDynRankView(sideCoordVec.get_view(), "normals", numCells_, numSideQPs, numCellDims);
      Kokkos::DynRankView<MeshScalarT, PHX::Device> jacobian_side =
          Kokkos::createDynRankView(sideCoordVec.get_view(), "jacobian_side", numCells_, numSideQPs, numCellDims, numCellDims);
      Kokkos::DynRankView<MeshScalarT, PHX::Device> physPointsSide =
          Kokkos::createDynRankView(sideCoordVec.get_view(), "physPointsSide", numCells_, numSideQPs, numCellDims);
      Kokkos::DynRankView<RealType, PHX::Device>    refPointsSide("refPointsSide", numSideQPs, numCellDims);
      Kokkos::DynRankView<MeshScalarT, PHX::Device> physPointsCell = Kokkos::createDynRankView(coordVec.get_view(), "XXX", numCells_, numNodes, numCellDims);
      Kokkos::DynRankView<int, PHX::Device>         cellVec        = cellsOnSides[side];

      for (std::size_t node = 0; node < numNodes; ++node)
        for (std::size_t dim = 0; dim < numCellDims; ++dim)
          for (std::size_t iCell = 0; iCell < numCells_; ++iCell) physPointsCell(iCell, node, dim) = coordVec(cellVec(iCell), node, dim);

      // Map side cubature points to the reference parent cell based on the
      // appropriate side (elem_side)
      Intrepid2::CellTools<PHX::Device>::mapToReferenceSubcell(refPointsSide, cub_points, numSideDims, side, *cellType);

      // Calculate side geometry
      Intrepid2::CellTools<PHX::Device>::setJacobian(jacobian_side, refPointsSide, physPointsCell, *cellType);

      // for this side in the reference cell, get the components of the normal
      // direction vector
      Intrepid2::CellTools<PHX::Device>::getPhysicalSideNormals(normals_view, jacobian_side, side, *cellType);

      // scale normals (unity)
      Intrepid2::RealSpaceTools<PHX::Device>::vectorNorm(normal_lengths, normals_view, Intrepid2::NORM_TWO);
      Intrepid2::FunctionSpaceTools<PHX::Device>::scalarMultiplyDataData(normals_view, normal_lengths, normals_view, true);

      for (int icoor = 0; icoor < numCellDims; ++icoor)
        for (int qp = 0; qp < numSideQPs; ++qp)
          for (std::size_t iCell = 0; iCell < numCells_; ++iCell) normals(cellVec(iCell), side, qp, icoor) = normals_view(iCell, qp, icoor);
    }
  }
}

}  // Namespace PHAL
