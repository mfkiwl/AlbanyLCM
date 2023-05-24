// Albany 3.0: Copyright 2016 National Technology & Engineering Solutions of
// Sandia, LLC (NTESS). This Software is released under the BSD license detailed
// in the file license.txt in the top-level Albany directory.

#ifndef MOERTEL_UTILST_HPP
#define MOERTEL_UTILST_HPP

#include <ctime>
#include <iostream>

#include "Tpetra_CrsMatrix.hpp"

/*!
\brief MoertelT: namespace of the Moertel package
*/
namespace MoertelT {

// forward declarations
SEGMENT_TEMPLATE_STATEMENT
class SegmentT;

MOERTEL_TEMPLATE_STATEMENT
class NodeT;

/*!
\brief Solve dense 3x3 system of equations

Ax=b

*/
template <class LO, class ST>
bool
solve33T(double const A[][3], double* x, double const* b);

/*!
\brief Add matrices A+B

Perform B = scalarB * B + scalarA * A ^ transposeA
If scalarB is 0.0, then B = scalarA * A ^ transposeA isperformed.

This is a modified version of E-petraExt's MatrixMatrixAdd.
FillComplete() must not be called on B upon entry,
FillComplete() will not be called on B upon exit by this method.

\param A : Matrix A to add to B
\param transposeA : flag indicating whether A*T shall be added
\param scalarA : scalar factor for A
\param B : Matrix B to be added to
\param scalarB : scalar factor for B
\return Zero upon success
*/
template <class ST, class LO, class GO, class N>
int
MatrixMatrixAdd(const Tpetra::CrsMatrix<ST, LO, GO, N>& A, bool transposeA, double scalarA, Tpetra::CrsMatrix<ST, LO, GO, N>& B, double scalarB);

/*!
\brief Multiply matrices A*B

matrices A and B are mutliplied and the result is allocated and returned.
The user is responsible for freeing the returned result.

\param A : Matrix A to multiply
\param transA : flag indicating whether A*T shall be used
\param B : Matrix B to multiply
\param transB : flag indicating whether B*T shall be used
\return Result upon success and NULL upon failure
*/
template <class ST, class LO, class GO, class N>
Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>
MatMatMult(const Tpetra::CrsMatrix<ST, LO, GO, N>& A, bool transA, const Tpetra::CrsMatrix<ST, LO, GO, N>& B, bool transB, int outlevel);

/*!
\brief Allocate and return a matrix padded with val on the diagonal.
       FillComplete() is NOT called on exit.
*/
template <class ST, class LO, class GO, class N>
Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>
PaddedMatrix(const Tpetra::Map<LO, GO, N>& rowmap, double val, int const numentriesperrow);

/*!
\brief Strip out values from a matrix below a certain tolerance

Allocates and returns a new matrix and copies A to it where entries
with an absoute value smaller then eps are negelected.
The method calls FillComplete(A.OperatorDomainMap(),A.OperatorRangeMap())
on the result.

\param A : Matrix A to strip
\param eps : tolerance
\return The new matrix upon success, NULL otherwise
*/
template <class ST, class LO, class GO, class N>
Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>
StripZeros(const Tpetra::CrsMatrix<ST, LO, GO, N>& A, double eps);

/*!
\brief split a matrix into a 2x2 block system where the rowmap of one of the
blocks is given

Splits a given matrix into a 2x2 block system where the rowmap of one of the
blocks is given on input. Blocks A11 and A22 are assumed to be square. All
values on entry have to be Teuchos::null except the given rowmap and matrix A.
Note that either A11rowmap or A22rowmap or both have to be nonzero. In case
both rowmaps are supplied they have to be an exact and nonoverlapping split of
A->RowMap(). Matrix blocks are FillComplete() on exit.

\param A         : Matrix A on input
\param A11rowmap : rowmap of A11 or null
\param A22rowmap : rowmap of A22 or null
\param A11       : on exit matrix block A11
\param A12       : on exit matrix block A12
\param A21       : on exit matrix block A21
\param A22       : on exit matrix block A22
*/
template <class ST, class LO, class GO, class N>
bool
SplitMatrix2x2(
    Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>  A,
    Teuchos::RCP<Tpetra::Map<LO, GO, N>>&           A11rowmap,
    Teuchos::RCP<Tpetra::Map<LO, GO, N>>&           A22rowmap,
    Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>& A11,
    Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>& A12,
    Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>& A21,
    Teuchos::RCP<Tpetra::CrsMatrix<ST, LO, GO, N>>& A22);

/*!
\brief split a rowmap of matrix A

splits A->RowMap() into 2 maps and returns them, where one of the rowmaps has
to be given on input

\param Amap      : Map to split on input
\param Agiven    : on entry submap that is given and part of Amap
\return the remainder map of Amap that is not overlapping with Agiven
*/
template <class LO, class GO, class N>
Teuchos::RCP<Tpetra::Map<LO, GO, N>>
SplitMap(const Tpetra::Map<LO, GO, N>& Amap, const Tpetra::Map<LO, GO, N>& Agiven);

/*!
\brief split a vector into 2 non-overlapping pieces

*/
template <class ST, class LO, class GO, class N>
bool
SplitVector(
    const Tpetra::Vector<ST, LO, GO, N>&               x,
    const Tpetra::Map<LO, GO, N>&                      x1map,
    const Teuchos::RCP<Tpetra::Vector<ST, LO, GO, N>>& x1,
    const Tpetra::Map<LO, GO, N>&                      x2map,
    const Teuchos::RCP<Tpetra::Vector<ST, LO, GO, N>>& x2);

/*!
\brief merge results from 2 vectors into one (assumes matching submaps)

*/
template <class ST, class LO, class GO, class N>
bool
MergeVector(const Tpetra::Vector<ST, LO, GO, N>& x1, const Tpetra::Vector<ST, LO, GO, N>& x2, Tpetra::Vector<ST, LO, GO, N>& xresult);

/*!
\brief Print matrix to file

Prints an E-petra_CrsMatrix to file in serial and parallel.
Will create several files with process id appended to the name in parallel.
Index base can either be 0 or 1.
The first row of the file gives the global size of the range and domain map,
the sond row gives the local size of the row- and column map.

\param name : Name of file without appendix, appendix will be .mtx
\param A : Matrix to print
\param ibase : Index base, should be either 1 or 0
*/
template <class ST, class LO, class GO, class N>
bool
Print_Matrix(std::string name, const Tpetra::CrsMatrix<ST, LO, GO, N>& A, int ibase);

/*!
\brief Print graph to file

Prints an Tpetra_CrsGraph to file in serial and parallel.
Will create several files with process id appended to the name in parallel.
Index base can either be 0 or 1.
The first row of the file gives the global size of the range and domain map,
the second row gives the local size of the row- and column map.

\param name : Name of file without appendix, appendix will be .mtx
\param A : Graph to print
\param ibase : Index base, should be either 1 or 0
*/
template <class LO, class GO, class N>
bool
Print_Graph(std::string name, const Tpetra::CrsGraph<LO, GO, N>& A, int ibase);

/*!
\brief Print vector to file

Prints a Tpetra_Vector to file in serial and parallel.
Will create several files with process id appended to the name in parallel.
Index base can either be 0 or 1.

\param name : Name of file without appendix, appendix will be .vec
\param v : Vector to print
\param ibase : Index base, should be either 1 or 0
*/
template <class ST, class LO, class GO, class N>
bool
Print_Vector(std::string name, const Tpetra::Vector<ST, LO, GO, N>& v, int ibase);

//! Error reporting method
int
ReportError(std::string conststream& Message);

}  // namespace MoertelT

#ifndef HAVE_MOERTEL_EXPLICIT_INSTANTIATION
#include "Moertel_UtilsT_Def.hpp"
#endif

#endif  // MOERTEL_UTILS_H
