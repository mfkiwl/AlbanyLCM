// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#include "Albany_Macros.hpp"
#include "Intrepid2_FunctionSpaceTools.hpp"
#include "LCM/evaluators/SetField.hpp"
#include "Phalanx_DataLayout.hpp"

namespace LCM {

template <typename EvalT, typename Traits>
SetField<EvalT, Traits>::SetField(Teuchos::ParameterList const& p)
    : evaluatedFieldName(p.get<std::string>("Evaluated Field Name")),
      evaluatedField(p.get<std::string>("Evaluated Field Name"), p.get<Teuchos::RCP<PHX::DataLayout>>("Evaluated Field Data Layout")),
      fieldValues(p.get<Teuchos::ArrayRCP<ScalarT>>("Field Values"))
{
  // Get the dimensions of the data layout for the field that is to be set
  p.get<Teuchos::RCP<PHX::DataLayout>>("Evaluated Field Data Layout")->dimensions(evaluatedFieldDimensions);

  // Register the field to be set as an evaluated field
  this->addEvaluatedField(evaluatedField);
  this->setName("SetField" + PHX::print<EvalT>());
}

template <typename EvalT, typename Traits>
void
SetField<EvalT, Traits>::postRegistrationSetup(typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(evaluatedField, fm);
}

template <typename EvalT, typename Traits>
void
SetField<EvalT, Traits>::evaluateFields(typename Traits::EvalData workset)
{
  unsigned int numDimensions = evaluatedFieldDimensions.size();

  ALBANY_PANIC(numDimensions < 1, "SetField::evaluateFields(), unsupported field type.");
  int dim1 = evaluatedFieldDimensions[0];

  if (numDimensions == 1) {
    for (int i = 0; i < dim1; ++i) {
      evaluatedField(i) = fieldValues[i];
    }
  } else if (numDimensions == 2) {
    int dim2 = evaluatedFieldDimensions[1];
    ALBANY_PANIC(fieldValues.size() != dim1 * dim2, "SetField::evaluateFields(), inconsistent data sizes.");
    for (int i = 0; i < dim1; ++i) {
      for (int j = 0; j < dim2; ++j) {
        evaluatedField(i, j) = fieldValues[i * dim2 + j];
      }
    }
  } else if (numDimensions == 3) {
    int dim2 = evaluatedFieldDimensions[1];
    int dim3 = evaluatedFieldDimensions[2];
    ALBANY_PANIC(fieldValues.size() != dim1 * dim2 * dim3, "SetField::evaluateFields(), inconsistent data sizes.");
    for (int i = 0; i < dim1; ++i) {
      for (int j = 0; j < dim2; ++j) {
        for (int m = 0; m < dim3; ++m) {
          evaluatedField(i, j, m) = fieldValues[i * dim2 * dim3 + j * dim3 + m];
        }
      }
    }
  } else if (numDimensions == 4) {
    int dim3 = evaluatedFieldDimensions[2];
    int dim2 = evaluatedFieldDimensions[1];
    int dim4 = evaluatedFieldDimensions[3];
    ALBANY_PANIC(fieldValues.size() != dim1 * dim2 * dim3 * dim4, "SetField::evaluateFields(), inconsistent data sizes.");
    for (int i = 0; i < dim1; ++i) {
      for (int j = 0; j < dim2; ++j) {
        for (int m = 0; m < dim3; ++m) {
          for (int n = 0; n < dim4; ++n) {
            evaluatedField(i, j, m, n) = fieldValues[i * dim2 * dim3 * dim4 + j * dim3 * dim4 + m * dim4 + n];
          }
        }
      }
    }
  } else {
    ALBANY_PANIC(numDimensions > 4, "SetField::evaluateFields(), unsupported data type.");
  }
}
}  // namespace LCM
