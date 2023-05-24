// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.
#if defined(ALBANY_TIMER)
#include <chrono>
#endif

#include "Albany_Macros.hpp"
#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Phalanx_DataLayout.hpp"

namespace PHAL {

//*****
template <typename EvalT, typename Traits, typename ScalarT>
DOFVecGradInterpolationBase<EvalT, Traits, ScalarT>::DOFVecGradInterpolationBase(Teuchos::ParameterList const& p, const Teuchos::RCP<Albany::Layouts>& dl)
    : val_node(p.get<std::string>("Variable Name"), dl->node_vector),
      GradBF(p.get<std::string>("Gradient BF Name"), dl->node_qp_gradient),
      grad_val_qp(p.get<std::string>("Gradient Variable Name"), dl->qp_vecgradient)
{
  this->addDependentField(val_node.fieldTag());
  this->addDependentField(GradBF.fieldTag());
  this->addEvaluatedField(grad_val_qp);

  this->setName("DOFVecGradInterpolationBase" + PHX::print<EvalT>());

  std::vector<PHX::DataLayout::size_type> dims;
  GradBF.fieldTag().dataLayout().dimensions(dims);
  numNodes = dims[1];
  numQPs   = dims[2];
  numDims  = dims[3];

  val_node.fieldTag().dataLayout().dimensions(dims);
  vecDim = dims[2];
}

//*****
template <typename EvalT, typename Traits, typename ScalarT>
void
DOFVecGradInterpolationBase<EvalT, Traits, ScalarT>::postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(val_node, fm);
  this->utils.setFieldData(GradBF, fm);
  this->utils.setFieldData(grad_val_qp, fm);
  d.fill_field_dependencies(this->dependentFields(), this->evaluatedFields());
}

//****
// KOKKOS functor Residual

template <typename EvalT, typename Traits, typename ScalarT>
KOKKOS_INLINE_FUNCTION void
DOFVecGradInterpolationBase<EvalT, Traits, ScalarT>::operator()(const DOFVecGradInterpolationBase_Residual_Tag& tag, const int& cell) const
{
  for (int qp = 0; qp < numQPs; ++qp)
    for (int i = 0; i < vecDim; i++)
      for (int dim = 0; dim < numDims; dim++) grad_val_qp(cell, qp, i, dim) = 0.0;

  for (int qp = 0; qp < numQPs; ++qp) {
    for (int i = 0; i < vecDim; i++) {
      for (int dim = 0; dim < numDims; dim++) {
        // For node==0, overwrite. Then += for 1 to numNodes.
        grad_val_qp(cell, qp, i, dim) = val_node(cell, 0, i) * GradBF(cell, 0, qp, dim);
        for (int node = 1; node < numNodes; ++node) {
          grad_val_qp(cell, qp, i, dim) += val_node(cell, node, i) * GradBF(cell, node, qp, dim);
        }
      }
    }
  }
}

// *********************************************************************************
template <typename EvalT, typename Traits, typename ScalarT>
void
DOFVecGradInterpolationBase<EvalT, Traits, ScalarT>::evaluateFields(typename Traits::EvalData workset)
{
#if defined(ALBANY_TIMER)
  PHX::Device::fence();
  auto start = std::chrono::high_resolution_clock::now();
#endif
  // Kokkos::deep_copy(grad_val_qp.get_kokkos_view(), 0.0);
  Kokkos::parallel_for(DOFVecGradInterpolationBase_Residual_Policy(0, workset.numCells), *this);

#if defined(ALBANY_TIMER)
  PHX::Device::fence();
  auto      elapsed      = std::chrono::high_resolution_clock::now() - start;
  long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  long long millisec     = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::cout << "DOFVecGradInterpolationBase Residual time = " << millisec << "  " << microseconds << std::endl;
#endif
}

// Specialization for Jacobian evaluation taking advantage of known sparsity
//*****
// Kokkos functor Jacobian
#ifndef ALBANY_MESH_DEPENDS_ON_SOLUTION
template <typename Traits>
KOKKOS_INLINE_FUNCTION void
FastSolutionVecGradInterpolationBase<PHAL::AlbanyTraits::Jacobian, Traits, typename PHAL::AlbanyTraits::Jacobian::ScalarT>::operator()(
    const FastSolutionVecGradInterpolationBase_Jacobian_Tag& tag,
    const int&                                               cell) const
{
  for (int qp = 0; qp < this->numQPs; ++qp) {
    for (int i = 0; i < this->vecDim; i++) {
      for (int dim = 0; dim < this->numDims; dim++) {
        // For node==0, overwrite. Then += for 1 to numNodes.
        this->grad_val_qp(cell, qp, i, dim)                            = ScalarT(num_dof, this->val_node(cell, 0, i).val() * this->GradBF(cell, 0, qp, dim));
        (this->grad_val_qp(cell, qp, i, dim)).fastAccessDx(offset + i) = this->val_node(cell, 0, i).fastAccessDx(offset + i) * this->GradBF(cell, 0, qp, dim);
        for (int node = 1; node < this->numNodes; ++node) {
          (this->grad_val_qp(cell, qp, i, dim)).val() += this->val_node(cell, node, i).val() * this->GradBF(cell, node, qp, dim);
          (this->grad_val_qp(cell, qp, i, dim)).fastAccessDx(neq * node + offset + i) +=
              this->val_node(cell, node, i).fastAccessDx(neq * node + offset + i) * this->GradBF(cell, node, qp, dim);
        }
      }
    }
  }
}
//*****
template <typename Traits>
void
FastSolutionVecGradInterpolationBase<PHAL::AlbanyTraits::Jacobian, Traits, typename PHAL::AlbanyTraits::Jacobian::ScalarT>::evaluateFields(
    typename Traits::EvalData workset)
{
#if defined(ALBANY_TIMER)
  auto start = std::chrono::high_resolution_clock::now();
#endif

  num_dof = this->val_node(0, 0, 0).size();
  neq     = workset.wsElNodeEqID.extent(2);

  Kokkos::parallel_for(FastSolutionVecGradInterpolationBase_Jacobian_Policy(0, workset.numCells), *this);

#if defined(ALBANY_TIMER)
  PHX::Device::fence();
  auto      elapsed      = std::chrono::high_resolution_clock::now() - start;
  long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  long long millisec     = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
  std::cout << "FastSolutionVecGradInterpolationBase Jacobian time = " << millisec << "  " << microseconds << std::endl;
#endif
}
#endif  // ALBANY_MESH_DEPENDS_ON_SOLUTION

}  // Namespace PHAL
