// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.
#ifndef AADAPT_RC_DATATYPES_IMPL
#define AADAPT_RC_DATATYPES_IMPL

#include "AAdapt_RC_DataTypes.hpp"

namespace AAdapt {
namespace rc {

/*! Some macros for ETI. For internal use in the AAdapt::rc namespace. Include
 *  only in .cpp files.
 */

/*! aadapt_rc_apply_to_all_ad_types(macro, arg2) applies macro(Type, arg2) to
 *  every AD type Albany_DataTypes.hpp defines. Type is RealType, FadType, etc,
 *  and arg2 is a user's argument.
 */
#if defined(ALBANY_FADTYPE_NOTEQUAL_TANFADTYPE)
#define aadapt_rc_apply_to_all_ad_types(macro, arg2) macro(RealType, arg2) macro(FadType, arg2) macro(TanFadType, arg2)
#else
#define aadapt_rc_apply_to_all_ad_types(macro, arg2) macro(RealType, arg2) macro(FadType, arg2)
#endif

/*! aadapt_rc_apply_to_all_eval_types(macro) applies a macro to every evaluation
 *  type PHAL::AlbanyTraits defines.
 *
 * Note that we do not need to ETI PHAL::AlbanyTraits::Residual, as it is
 * already a specialization in AAdapt_RC_Manager.cpp (Indeed - doing so will
 * cause an error) GAH If this changes - need
 * "macro(PHAL::AlbanyTraits::Residual)                   \"
 */
#define aadapt_rc_apply_to_all_eval_types(macro) macro(PHAL::AlbanyTraits::Jacobian)

/*! Perform ETI for a class \code template<int rank> Class \endcode.
 */
#define aadapt_rc_eti_class(Class) \
  template class Class<0>;         \
  template class Class<1>;         \
  template class Class<2>;
/*! Apply \code aadapt_rc_apply_to_all_ad_types(eti, rank) \endcode to each \c
 *  rank.
 */
#define aadapt_rc_apply_to_all_ad_types_all_ranks(macro) \
  aadapt_rc_apply_to_all_ad_types(macro, 0) aadapt_rc_apply_to_all_ad_types(macro, 1) aadapt_rc_apply_to_all_ad_types(macro, 2)

}  // namespace rc
}  // namespace AAdapt

#endif  // AADAPT_RC_DATATYPES_IMPL
