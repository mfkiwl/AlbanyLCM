// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include <MiniTensor.h>

#include <typeinfo>

#include "Albany_Macros.hpp"
#include "Phalanx_DataLayout.hpp"
namespace LCM {

template <typename EvalT, typename Traits>
LinearElasticModel<EvalT, Traits>::LinearElasticModel(Teuchos::ParameterList* p, const Teuchos::RCP<Albany::Layouts>& dl)
    : LCM::ConstitutiveModel<EvalT, Traits>(p, dl)
{
  // define the dependent fields
  this->dep_field_map_.insert(std::make_pair("Strain", dl->qp_tensor));
  this->dep_field_map_.insert(std::make_pair("Poissons Ratio", dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Elastic Modulus", dl->qp_scalar));

  // define the evaluated fields
  std::string cauchy = (*field_name_map_)["Cauchy_Stress"];
  this->eval_field_map_.insert(std::make_pair(cauchy, dl->qp_tensor));

  // define the state variables
  this->num_state_variables_++;
  this->state_var_names_.push_back(cauchy);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(true);
}
template <typename EvalT, typename Traits>
void
LinearElasticModel<EvalT, Traits>::computeState(typename Traits::EvalData workset, DepFieldMap dep_fields, FieldMap eval_fields)
{
  bool print = false;
  // if (typeid(ScalarT) == typeid(RealType)) print = true;
  // std::cout.precision(15);

  // extract dependent MDFields
  auto strain          = *dep_fields["Strain"];
  auto poissons_ratio  = *dep_fields["Poissons Ratio"];
  auto elastic_modulus = *dep_fields["Elastic Modulus"];
  // extract evaluated MDFields
  std::string cauchy = (*field_name_map_)["Cauchy_Stress"];
  auto        stress = *eval_fields[cauchy];
  ScalarT     lambda;
  ScalarT     mu;

  minitensor::Tensor<ScalarT> eps(num_dims_), sigma(num_dims_);
  minitensor::Tensor<ScalarT> I(minitensor::eye<ScalarT>(num_dims_));

  if (print) {
    std::cout << "========" << std::endl;
  }

  for (int cell(0); cell < workset.numCells; ++cell) {
    for (int pt(0); pt < num_pts_; ++pt) {
      lambda = (elastic_modulus(cell, pt) * poissons_ratio(cell, pt)) / ((1 + poissons_ratio(cell, pt)) * (1 - 2 * poissons_ratio(cell, pt)));
      mu     = elastic_modulus(cell, pt) / (2 * (1 + poissons_ratio(cell, pt)));

      eps.fill(strain, cell, pt, 0, 0);

      sigma = 2.0 * mu * eps + lambda * minitensor::trace(eps) * I;

      if (print) {
        std::cout << "E      : " << elastic_modulus(cell, pt) << std::endl;
        std::cout << "nu     : " << poissons_ratio(cell, pt) << std::endl;
        std::cout << "lambda : " << lambda << std::endl;
        std::cout << "mu     : " << mu << std::endl;
        std::cout << "eps    :\n" << eps << std::endl;
        std::cout << "sigma  :\n" << sigma << std::endl;
      }

      for (int i = 0; i < num_dims_; ++i) {
        for (int j = 0; j < num_dims_; ++j) {
          stress(cell, pt, i, j) = sigma(i, j);
        }
      }
    }
  }

  if (print) std::cout << "// adjustment for thermal expansion" << std::endl;
  if (have_temperature_) {
    for (int cell(0); cell < workset.numCells; ++cell) {
      for (int pt(0); pt < num_pts_; ++pt) {
        sigma.fill(stress, cell, pt, 0, 0);
        ScalarT three_kappa = elastic_modulus(cell, pt) / (1.0 - 2.0 * poissons_ratio(cell, pt));
        sigma -= three_kappa * expansion_coeff_ * (temperature_(cell, pt) - ref_temperature_) * I;

        if (print) {
          std::cout << "temp   : " << temperature_(cell, pt) << std::endl;
          std::cout << "ref T  : " << ref_temperature_ << std::endl;
          std::cout << "delta T: " << temperature_(cell, pt) - ref_temperature_ << std::endl;
          std::cout << "alpha  : " << expansion_coeff_ << std::endl;
          std::cout << "sigma  :\n" << sigma << std::endl;
        }

        for (int i = 0; i < num_dims_; ++i) {
          for (int j = 0; j < num_dims_; ++j) {
            stress(cell, pt, i, j) = sigma(i, j);
          }
        }
      }
    }
  }
}
}  // namespace LCM
