// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#if !defined(LCM_ACEpermafrost_hpp)
#define LCM_ACEpermafrost_hpp

#include "ParallelConstitutiveModel.hpp"

namespace LCM {

template <typename EvalT, typename Traits>
struct ACEpermafrostMiniKernel : public ParallelKernel<EvalT, Traits>
{
  ///
  /// Constructor
  ///
  ACEpermafrostMiniKernel(ConstitutiveModel<EvalT, Traits>& model, Teuchos::ParameterList* p, Teuchos::RCP<Albany::Layouts> const& dl);

  ///
  /// No copy constructor
  ///
  ACEpermafrostMiniKernel(ACEpermafrostMiniKernel const&) = delete;

  ///
  /// No copy assignment
  ///
  ACEpermafrostMiniKernel&
  operator=(ACEpermafrostMiniKernel const&) = delete;

  using ScalarT          = typename EvalT::ScalarT;
  using ScalarField      = PHX::MDField<ScalarT>;
  using ConstScalarField = PHX::MDField<ScalarT const>;
  using BaseKernel       = ParallelKernel<EvalT, Traits>;
  using Workset          = typename BaseKernel::Workset;

  using BaseKernel::field_name_map_;
  using BaseKernel::num_dims_;
  using BaseKernel::num_pts_;

  // optional temperature support
  using BaseKernel::expansion_coeff_;
  using BaseKernel::have_temperature_;
  using BaseKernel::ref_temperature_;

  using BaseKernel::addStateVariable;
  using BaseKernel::setDependentField;
  using BaseKernel::setEvaluatedField;

  /// Pointer to NOX status test, allows the material model to force
  /// a global load step reduction
  using BaseKernel::nox_status_test_;

  // Input constant MDFields
  ConstScalarField def_grad_;
  ConstScalarField delta_time_;
  ConstScalarField elastic_modulus_;
  ConstScalarField hardening_modulus_;
  ConstScalarField J_;
  ConstScalarField poissons_ratio_;
  ConstScalarField yield_strength_;
  ConstScalarField temperature_;

  // Output MDFields
  ScalarField bluff_salinity_;
  ScalarField density_;
  ScalarField heat_capacity_;
  ScalarField ice_saturation_;
  ScalarField thermal_cond_;
  ScalarField thermal_inertia_;
  ScalarField water_saturation_;
  ScalarField porosity_;
  ScalarField tdot_;
  ScalarField failed_;
  ScalarField exposure_time_;

  // Mechanical MDFields
  ScalarField eqps_;
  ScalarField Fp_;
  ScalarField stress_;
  ScalarField yield_surf_;

  // Workspace arrays
  Albany::MDArray Fp_old_;
  Albany::MDArray eqps_old_;
  Albany::MDArray T_old_;
  Albany::MDArray ice_saturation_old_;

  bool                       have_cell_boundary_indicator_{false};
  Teuchos::ArrayRCP<double*> cell_boundary_indicator_;

  // Baseline constants
  RealType ice_density_{0.0};
  RealType water_density_{0.0};
  RealType soil_density_{0.0};
  RealType ice_thermal_cond_{0.0};
  RealType water_thermal_cond_{0.0};
  RealType soil_thermal_cond_{0.0};
  RealType ice_heat_capacity_{0.0};
  RealType water_heat_capacity_{0.0};
  RealType soil_heat_capacity_{0.0};
  RealType ice_saturation_init_{0.0};
  RealType ice_saturation_max_{0.0};
  RealType water_saturation_min_{0.0};
  RealType salinity_base_{0.0};
  RealType salt_enhanced_D_{0.0};
  RealType freeze_curve_width_{1.0};
  RealType f_shift_{0.25};
  RealType latent_heat_{0.0};
  RealType porosity0_{0.0};
  RealType erosion_rate_{0.0};
  RealType element_size_{0.0};
  RealType critical_stress_{0.0};
  RealType critical_angle_{0.0};
  RealType soil_yield_strength_{0.0};

  // Saturation hardening constraints
  RealType sat_mod_{0.0};
  RealType sat_exp_{0.0};

  // Params with depth:
  std::vector<RealType> z_above_mean_sea_level_;
  std::vector<RealType> salinity_;
  std::vector<RealType> ocean_salinity_;
  std::vector<RealType> porosity_from_file_;
  std::vector<RealType> sand_from_file_;  // sand fraction
  std::vector<RealType> clay_from_file_;  // clay fraction
  std::vector<RealType> silt_from_file_;  // silt fraction
  std::vector<RealType> peat_from_file_;  // peat fraction

  // Sea level arrays
  std::vector<RealType> time_;
  std::vector<RealType> sea_level_;
  RealType              current_time_{0.0};

  std::string block_name_{""};

  void
  init(Workset& workset, FieldMap<ScalarT const>& input_fields, FieldMap<ScalarT>& output_fields);

  KOKKOS_INLINE_FUNCTION
  void
  operator()(int cell, int pt) const;
};

template <typename EvalT, typename Traits>
class ACEpermafrost : public LCM::ParallelConstitutiveModel<EvalT, Traits, ACEpermafrostMiniKernel<EvalT, Traits>>
{
 public:
  ACEpermafrost(Teuchos::ParameterList* p, const Teuchos::RCP<Albany::Layouts>& dl);
};
}  // namespace LCM
#endif  // LCM_ACEpermafrost_hpp
