// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef ALBANY_UTILS_HPP
#define ALBANY_UTILS_HPP

// Get Albany configuration macros
#include <sstream>

#include "Albany_CommUtils.hpp"
#include "Albany_Macros.hpp"
#include "Albany_StateManager.hpp"
#include "Albany_ThyraTypes.hpp"
#include "Albany_TpetraTypes.hpp"
#include "Albany_config.h"
#include "Teuchos_RCP.hpp"

namespace Albany {

//! Print ascii art and version information
void
PrintHeader(std::ostream& os);

// Helper function which replaces the diagonal of a matrix
void
ReplaceDiagonalEntries(const Teuchos::RCP<Tpetra_CrsMatrix>& matrix, const Teuchos::RCP<Tpetra_Vector>& diag);

// Helper function which computes absolute values of the rowsum
// of a matrix, takes its inverse, and puts it in a vector.
void
InvAbsRowSum(Teuchos::RCP<Tpetra_Vector>& invAbsRowSumsTpetra, const Teuchos::RCP<Tpetra_CrsMatrix> matrix);

// Helper function which computes absolute values of the rowsum
// of a matrix, and puts it in a vector.
void
AbsRowSum(Teuchos::RCP<Tpetra_Vector>& absRowSumsTpetra, const Teuchos::RCP<Tpetra_CrsMatrix> matrix);

// Helper function which replaces the diagonal of a matrix
void
ReplaceDiagonalEntries(const Teuchos::RCP<Tpetra_CrsMatrix>& matrix, const Teuchos::RCP<Tpetra_Vector>& diag);

//! Utility to make a string out of a string + int with a delimiter:
//! strint("dog",2,' ') = "dog 2"
//! The default delimiter is ' '. Potential delimiters include '_' - "dog_2"
std::string
strint(std::string const s, int const i, char const delim = ' ');

//! Returns true of the given string is a valid initialization string of the
//! format "initial value 1.54"
bool
isValidInitString(std::string const& initString);

//! Converts a double to an initialization string:  doubleToInitString(1.54) =
//! "initial value 1.54"
std::string
doubleToInitString(double val);

//! Converts an init string to a double:  initStringToDouble("initial value
//! 1.54") = 1.54
double
initStringToDouble(std::string const& initString);

//! Splits a std::string on a delimiter
void
splitStringOnDelim(std::string const& s, char delim, std::vector<std::string>& elems);

/// Get file name extension
std::string
getFileExtension(std::string const& filename);

//! Nicely prints out a Thyra Vector
void
printThyraVector(std::ostream& os, Teuchos::RCP<Thyra_Vector const> const& vec);
void
printThyraVector(std::ostream& os, const Teuchos::Array<std::string>& names, Teuchos::RCP<Thyra_Vector const> const& vec);

//! Inlined product version
inline void
printThyraVector(std::ostream& os, const Teuchos::RCP<const Thyra_ProductVector>& vec)
{
  for (int i = 0; i < vec->productSpace()->numBlocks(); ++i) {
    printThyraVector(os, vec->getVectorBlock(i));
  }
}

//! Nicely prints out a Thyra MultiVector
void
printThyraMultiVector(std::ostream& os, const Teuchos::RCP<const Thyra_MultiVector>& vec);
void
printThyraMultiVector(
    std::ostream&                                                    os,
    const Teuchos::Array<Teuchos::RCP<Teuchos::Array<std::string>>>& names,
    const Teuchos::RCP<const Thyra_MultiVector>&                     vec);

//! Inlined product version
inline void
printThyraVector(std::ostream& os, const Teuchos::RCP<const Thyra_ProductMultiVector>& vec)
{
  for (int i = 0; i < vec->productSpace()->numBlocks(); ++i) {
    printThyraMultiVector(os, vec->getMultiVectorBlock(i));
  }
}

/// Write to matrix market format a vector, matrix or map.
template <typename LinearAlgebraObjectType>
void
writeMatrixMarket(const Teuchos::RCP<LinearAlgebraObjectType>& A, std::string const& prefix, int const counter = -1);

template <typename LinearAlgebraObjectType>
void
writeMatrixMarket(const Teuchos::Array<Teuchos::RCP<LinearAlgebraObjectType>>& x, std::string const& prefix, int const counter = -1)
{
  for (auto i = 0; i < x.size(); ++i) {
    std::ostringstream oss;

    oss << prefix << '-' << std::setfill('0') << std::setw(2) << i;

    std::string const& new_prefix = oss.str();

    writeMatrixMarket(x[i].getConst(), new_prefix, counter);
  }
}

// Parses and stores command-line arguments
struct CmdLineArgs
{
  std::string yaml_filename;
  std::string yaml_filename2;
  std::string yaml_filename3;
  bool        has_first_yaml_file;
  bool        has_second_yaml_file;
  bool        has_third_yaml_file;
  bool        vtune;

  CmdLineArgs(
      std::string const& default_yaml_filename  = "input.yaml",
      std::string const& default_yaml_filename2 = "",
      std::string const& default_yaml_filename3 = "");
  void
  parse_cmdline(int argc, char** argv, std::ostream& os);
};

// Connect executable to vtune for profiling
void
connect_vtune(int const p_rank);

// Do a nice stack trace for debugging
void
do_stack_trace();

// Check returns codes and throw Teuchos exceptions
// Useful for silencing compiler warnings about unused return codes
void
safe_fscanf(int nitems, FILE* file, char const* format, ...);
void
safe_sscanf(int nitems, char const* str, char const* format, ...);
void
safe_fgets(char* str, int size, FILE* stream);
void
safe_system(char const* str);

void
assert_fail(std::string const& msg) __attribute__((noreturn));

int
getProcRank();

}  // end namespace Albany

#endif  // ALBANY_UTILS_HPP
