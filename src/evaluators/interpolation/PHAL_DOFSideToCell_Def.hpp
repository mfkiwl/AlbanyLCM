// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_DiscretizationUtils.hpp"
#include "PHAL_DOFSideToCell.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Shards_CellTopology.hpp"
#include "Teuchos_ParameterList.hpp"

namespace PHAL {

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
DOFSideToCellBase<EvalT, Traits, ScalarT>::DOFSideToCellBase(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
    : sideSetName(p.get<std::string>("Side Set Name"))
{
  ALBANY_PANIC(dl->side_layouts.find(sideSetName) == dl->side_layouts.end(), "Error! Layout for side set " << sideSetName << " not found.\n");

  Teuchos::RCP<Albany::Layouts> dl_side    = dl->side_layouts.at(sideSetName);
  std::string                   layout_str = p.get<std::string>("Data Layout");

  if (layout_str == "Cell Scalar") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->cell_scalar2);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->cell_scalar2);

    layout = CELL_SCALAR;
  } else if (layout_str == "Cell Vector") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->cell_vector);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->cell_vector);

    layout = CELL_VECTOR;
  } else if (layout_str == "Cell Tensor") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->cell_tensor);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->cell_tensor);

    layout = CELL_TENSOR;
  } else if (layout_str == "Node Scalar") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->node_scalar);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->node_scalar);

    layout = NODE_SCALAR;
  } else if (layout_str == "Node Vector") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->node_vector);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->node_vector);

    layout = NODE_VECTOR;
  } else if (layout_str == "Node Tensor") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->node_tensor);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->node_tensor);

    layout = NODE_TENSOR;
  } else if (layout_str == "Vertex Vector") {
    val_cell = decltype(val_cell)(p.get<std::string>("Cell Variable Name"), dl->vertices_vector);
    val_side = decltype(val_side)(p.get<std::string>("Side Variable Name"), dl_side->vertices_vector);

    layout = VERTEX_VECTOR;
  } else {
    ALBANY_ABORT("Error! Invalid field layout.\n");
  }

  Teuchos::RCP<shards::CellTopology> cellType;
  cellType = p.get<Teuchos::RCP<shards::CellTopology>>("Cell Type");

  int sideDim  = cellType->getDimension() - 1;
  int numSides = cellType->getSideCount();

  if (layout == NODE_SCALAR || layout == NODE_VECTOR || layout == NODE_TENSOR || layout == VERTEX_VECTOR) {
    sideNodes.resize(numSides);
    for (int side = 0; side < numSides; ++side) {
      // Need to get the subcell exact count, since different sides may have
      // different number of nodes (e.g., Wedge)
      int thisSideNodes = cellType->getNodeCount(sideDim, side);
      sideNodes[side].resize(thisSideNodes);
      for (int node = 0; node < thisSideNodes; ++node) {
        sideNodes[side][node] = cellType->getNodeMap(sideDim, side, node);
      }
    }
  }

  this->addDependentField(val_side.fieldTag());
  this->addEvaluatedField(val_cell);
  this->setName("DOFSideToCell");
}

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
void
DOFSideToCellBase<EvalT, Traits, ScalarT>::postRegistrationSetup(typename Traits::SetupData /* d */, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(val_cell, fm);
  this->utils.setFieldData(val_side, fm);

  val_side.dimensions(dims);
}

//**********************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
void
DOFSideToCellBase<EvalT, Traits, ScalarT>::evaluateFields(typename Traits::EvalData workset)
{
  if (workset.sideSets->find(sideSetName) == workset.sideSets->end()) return;

  std::vector<Albany::SideStruct> const& sideSet = workset.sideSets->at(sideSetName);
  for (auto const& it_side : sideSet) {
    // Get the local data of side and cell
    int const cell = it_side.elem_LID;
    int const side = it_side.side_local_id;

    switch (layout) {
      case CELL_SCALAR: val_cell(cell) = val_side(cell, side); break;

      case CELL_VECTOR:
        for (int i = 0; i < dims[2]; ++i) val_cell(cell, i) = val_side(cell, side, i);
        break;

      case CELL_TENSOR:
        for (int i = 0; i < dims[2]; ++i)
          for (int j = 0; j < dims[3]; ++j) val_cell(cell, i, j) = val_side(cell, side, i, j);
        break;

      case NODE_SCALAR:
        for (int node = 0; node < dims[2]; ++node) val_cell(cell, sideNodes[side][node]) = val_side(cell, side, node);
        break;

      case NODE_VECTOR:
      case VERTEX_VECTOR:
        for (int node = 0; node < dims[2]; ++node)
          for (int i = 0; i < dims[3]; ++i) val_cell(cell, sideNodes[side][node], i) = val_side(cell, side, node, i);
        break;
      case NODE_TENSOR:
        for (int node = 0; node < dims[2]; ++node)
          for (int i = 0; i < dims[3]; ++i)
            for (int j = 0; j < dims[4]; ++j) val_cell(cell, sideNodes[side][node], i, j) = val_side(cell, side, node, i, j);
        break;
      default:
        ALBANY_ABORT(
            "Error! Invalid layout (this error should have happened earlier "
            "though).\n");
    }
  }
}

}  // Namespace PHAL
