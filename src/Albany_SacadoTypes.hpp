// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef ALBANY_SACADO_TYPES_HPP
#define ALBANY_SACADO_TYPES_HPP

// Get all Albany configuration macros
#include "Albany_config.h"

// Get Albany's RealType
#include "Albany_ScalarOrdinalTypes.hpp"

// Include all of our AD types
#include "Sacado.hpp"
#include "Sacado_CacheFad_DFad.hpp"
#include "Sacado_ELRCacheFad_DFad.hpp"
#include "Sacado_ELRFad_DFad.hpp"
#include "Sacado_ELRFad_SFad.hpp"
#include "Sacado_ELRFad_SLFad.hpp"
#include "Sacado_Fad_DFad.hpp"
#include "Sacado_Fad_SFad.hpp"
#include "Sacado_Fad_SLFad.hpp"
#include "Sacado_MathFunctions.hpp"

// Include ScalarParameterLibrary to specialize its traits
#include "Sacado_ScalarParameterLibrary.hpp"
#include "Sacado_ScalarParameterVector.hpp"

// ******************************************************************
// Definition of Sacado::ParameterLibrary traits
// ******************************************************************

// Switch between dynamic and static FAD types
#if defined(ALBANY_FAD_TYPE_SFAD)
typedef Sacado::Fad::SFad<RealType, ALBANY_SFAD_SIZE> FadType;
#elif defined(ALBANY_FAD_TYPE_SLFAD)
typedef Sacado::Fad::SLFad<RealType, ALBANY_SLFAD_SIZE>     FadType;
#else
typedef Sacado::Fad::DFad<RealType> FadType;
#endif

#if defined(ALBANY_TAN_FAD_TYPE_SFAD)
typedef Sacado::Fad::SFad<RealType, ALBANY_TAN_SFAD_SIZE> TanFadType;
#elif defined(ALBANY_TAN_FAD_TYPE_SLFAD)
typedef Sacado::Fad::SLFad<RealType, ALBANY_TAN_SLFAD_SIZE> TanFadType;
#else
typedef Sacado::Fad::DFad<RealType> TanFadType;
#endif

struct SPL_Traits
{
  template <class T>
  struct apply
  {
    typedef typename T::ScalarT type;
  };
};

// Synonym for the ScalarParameterLibrary/Vector on our traits
typedef Sacado::ScalarParameterLibrary<SPL_Traits> ParamLib;
typedef Sacado::ScalarParameterVector<SPL_Traits>  ParamVec;

namespace Albany {

// Function to get the underlying value out of a scalar type
template <typename T>
typename Sacado::ScalarType<T>::type KOKKOS_INLINE_FUNCTION
ADValue(const T& x)
{
  return Sacado::ScalarValue<T>::eval(x);
}

// Get the strongest type between ST1 and ST2, meaning the type that can
// be constructed from the other. If no conversion is possible, return void.
template <typename ST1, typename ST2>
struct StrongestScalarType
{
 private:
  static constexpr bool st2_from_st1 = std::is_constructible<ST2, ST1>::value;
  static constexpr bool st1_from_st2 = std::is_constructible<ST1, ST2>::value;

 public:
  using type = typename std::conditional<st2_from_st1, ST2, typename std::conditional<st1_from_st2, ST1, void>::type>::type;
};

}  // namespace Albany

#endif  // ALBANY_SACADO_TYPES_HPP
