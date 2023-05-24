// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Schwarz_PiroObserver.hpp"

#include "PHAL_AlbanyTraits.hpp"
#include "Teuchos_ScalarTraits.hpp"

namespace LCM {

Schwarz_PiroObserver::Schwarz_PiroObserver(Teuchos::RCP<SchwarzCoupled> const& cs_model)
{
  apps_     = cs_model->getApps();
  n_models_ = apps_.size();
  impl_     = Teuchos::rcp(new ObserverImpl(apps_));
}

void
Schwarz_PiroObserver::observeSolution(Thyra::VectorBase<ST> const& solution)
{
  this->observeSolutionImpl(solution, Teuchos::ScalarTraits<ST>::zero());
}

void
Schwarz_PiroObserver::observeSolution(Thyra::VectorBase<ST> const& solution, ST const stamp)
{
  this->observeSolutionImpl(solution, stamp);
}

void
Schwarz_PiroObserver::observeSolution(Thyra::VectorBase<ST> const& solution, Thyra::VectorBase<ST> const& solution_dot, ST const stamp)
{
  this->observeSolutionImpl(solution, solution_dot, stamp);
}

namespace {  // anonymous

Teuchos::Array<Teuchos::RCP<Thyra_Vector const>>
arrayVecsFromVec(Thyra::VectorBase<double> const& v, int n_models)
{
  Teuchos::RCP<Thyra::ProductVectorBase<ST> const> const v_nonowning_rcp =
      Teuchos::rcp_dynamic_cast<const Thyra::ProductVectorBase<ST>>(Teuchos::rcpFromRef(v));

  // Create a Teuchos array of the vs for each model.
  Teuchos::Array<Teuchos::RCP<Thyra_Vector const>> vs(n_models);

  for (int m = 0; m < n_models; ++m) {
    // Get each Thyra vector
    vs[m] = Teuchos::rcp_dynamic_cast<Thyra_Vector const>(v_nonowning_rcp->getVectorBlock(m), true);
  }
  return vs;
}

}  // anonymous namespace

void
Schwarz_PiroObserver::observeSolutionImpl(Thyra::VectorBase<ST> const& solution, ST const default_stamp)
{
  Teuchos::Array<Teuchos::RCP<Thyra_Vector const>> solutions = arrayVecsFromVec(solution, n_models_);

  Teuchos::Array<Teuchos::RCP<Thyra_Vector const>> null_array;
  null_array.resize(n_models_);
  for (int m = 0; m < n_models_; m++) {
    null_array[m] = Teuchos::null;
  }

  impl_->observeSolution(default_stamp, solutions, null_array);
}

void
Schwarz_PiroObserver::observeSolutionImpl(Thyra::VectorBase<ST> const& solution, Thyra::VectorBase<ST> const& solution_dot, ST const default_stamp)
{
  Teuchos::Array<Teuchos::RCP<Thyra_Vector const>> solutions = arrayVecsFromVec(solution, n_models_);

  Teuchos::Array<Teuchos::RCP<Thyra_Vector const>> solutions_dot = arrayVecsFromVec(solution_dot, n_models_);

  impl_->observeSolution(default_stamp, solutions, solutions_dot);
}

}  // namespace LCM
