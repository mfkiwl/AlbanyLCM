// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#if !defined(LCM_SchwarzCoupledJacobian_hpp)
#define LCM_SchwarzCoupledJacobian_hpp

#include "Albany_DataTypes.hpp"
#include "Schwarz_BoundaryJacobian.hpp"
#include "Teuchos_Comm.hpp"
#include "Thyra_BlockedLinearOpBase.hpp"
#include "Thyra_PhysicallyBlockedLinearOpBase.hpp"

namespace LCM {

///
/// A class that evaluates the Jacobian of a
/// LCM coupled Schwarz problem
///

class Schwarz_CoupledJacobian
{
 public:
  Schwarz_CoupledJacobian(Teuchos::RCP<Teuchos_Comm const> const& comm);

  ~Schwarz_CoupledJacobian();

  Teuchos::RCP<Thyra::LinearOpBase<ST>>
  getThyraCoupledJacobian(Teuchos::Array<Teuchos::RCP<Thyra_LinearOp>> jacs, Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application>> const& ca) const;

 private:
  Teuchos::RCP<Teuchos_Comm const> comm_;
};

}  // namespace LCM
#endif  // LCM_SchwarzCoupledJacobian_hpp
