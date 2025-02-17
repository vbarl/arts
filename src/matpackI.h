/* Copyright (C) 2001-2012 Stefan Buehler <sbuehler@ltu.se>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
   USA. */

/**
  Implementation of Matrix, Vector, and such stuff.

  A VectorView consists of the data, which is stored in a continuous piece
  of memory, and a selection, specified by start, extend, and
  stride. A Vector is a VectorView which also allocates its memory
  automatically. 

  VectorViews can not be generated directly, they only result from
  operations on Vectors, such as using the index operator with a Range
  object. However, you can store them, like:

  VectorView A = B[Range(0,3)]

  A VectorView acts like a reference to the selected region in the
  parent matrix. Functions that operate on an existing matrix (i.e.,
  they do not use resize) should take VectorView x as arguement, rather
  than Vector& x. That has the advantage that they can be called
  either with a VectorView or Vector. E.g., if you have:

  void fill_with_junk(VectorView x);
  Vector v;

  then you can call the function in these two ways:

  fill_with_junk(v);
  fill_with_junk(v[Range(1,3)]) 

  Assignment (=) copies the data from one Vector or VectorView to
  another one. Dimensions must agree. Only exceptions are the copy
  constructors which automatically set the dimensions to
  match.

  Things work in the same way for the type Matrix. 

  There exist operators *=, /=, +=, and -= to multiply  (divide,...)
  by a scalar. Plain operators *,... do not exist, because they would
  result in the creation of temporaries and therefor be inefficient.  

  However, you can use * to compute the scalar product. This is
  efficient, since the return value is just a scalar. 

  There is a constructor for vector filling it with a sequence of
  values. 

  Matrices:

  You can extract sub matrices (MatrixView) using Range objects. You
  can also extract rows and columns this way.

  transpose(A) Returns a special MatrixView that is the transpose of the
  original. The original is not changed by this!

  mult(A,B,C) computes A = B*C
  Note that the order is different from MTL (output first)!

  A VectorView or Vector can be taken in the place of a nx1
  matrix. That means, Vectors are interpreted as column
  vectors. Hence, you can compute:

  Vector a(10),b(20);
  Matrix M(10,20);

  mult(a,M,b);    // a = M*b

  but also, by using transpose:

  mult(transpose(b),transpose(a),M);    // b^t = a^t * M

  See the section about Matrices and Vectors in the ARTS user guide
  for more details.

  \author Stefan Buehler
  \date   2001-06-12
 */

#ifndef matpackI_h
#define matpackI_h

#include <Eigen/Dense>
#include <cassert>
#include "array.h"
#include "matpack.h"

// Declare existance of some classes
class bofstream;

// Declaration of Eigen types
typedef Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic> StrideType;
typedef Eigen::Matrix<Numeric, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
    MatrixType;
typedef Eigen::Map<MatrixType, 0, StrideType> MatrixViewMap;
typedef Eigen::Map<const MatrixType, 0, StrideType> ConstMatrixViewMap;
typedef Eigen::Matrix<Numeric, 4, 4, Eigen::RowMajor> Matrix4x4Type;
typedef Eigen::Map<Matrix4x4Type, 0, StrideType> Matrix4x4ViewMap;
typedef Eigen::Map<const Matrix4x4Type, 0, StrideType> ConstMatrix4x4ViewMap;

/** The Joker class.

    This class is used by Vector and Matrix in connection with Range
    to implement Matlab-like subranges of vectors and matrices.

    This class has no members. We just need a special type to indicate
    the joker. There is a global joker object defined somewhere:

    Joker joker;
*/
class Joker {
  // Nothing here.
};

// Declare existence of the global joker object:
extern const Joker joker;

// Declare the existence of class ConstMatrixView:
class ConstIterator1D;

// Declare the existence of class VectorView:
class VectorView;

// Declare the existence of class ConstVectorView:
class ConstVectorView;

// Declare the existence of class ConstMatrixView:
class ConstMatrixView;

/** The range class.

    This is used to specifiy a range of a vector. In general, a range is
    given by a start index, an extent, and a stride. The entire vector
    would be:
    start = 0, range = # elements, stride = 1

    Stride specifies the stepsize of the vector. A stride of 2 means
    only every second element. This is particularly important in
    connection with matrices.

    There are a number of special constructors for this class, of
    particular interest should be those using jokers, which provide a
    Matlab-like functionality.
*/
class Range {
 public:
  // Constructors:
  Range(Index start, Index extent, Index stride = 1);
  Range(Index start, Joker j, Index stride = 1);
  Range(Joker j, Index stride = 1);
  Range(Index max_size, const Range& r);
  Range(const Range& p, const Range& n);

  // Friends:
  friend class ConstVectorView;
  friend class VectorView;
  friend class Vector;
  friend class ConstMatrixView;
  friend class MatrixView;
  friend class Matrix;
  friend class Iterator2D;
  friend class Iterator3D;
  friend class Iterator4D;
  friend class Iterator5D;
  friend class Iterator6D;
  friend class Iterator7D;
  friend class ConstIterator2D;
  friend class ConstIterator3D;
  friend class ConstIterator4D;
  friend class ConstIterator5D;
  friend class ConstIterator6D;
  friend class ConstIterator7D;
  friend class ConstTensor3View;
  friend class Tensor3View;
  friend class Tensor3;
  friend class ConstTensor4View;
  friend class Tensor4View;
  friend class Tensor4;
  friend class ConstTensor5View;
  friend class Tensor5View;
  friend class Tensor5;
  friend class ConstTensor6View;
  friend class Tensor6View;
  friend class Tensor6;
  friend class ConstTensor7View;
  friend class Tensor7View;
  friend class Tensor7;
  friend class Sparse;
  friend class ConstComplexVectorView;
  friend class ComplexVectorView;
  friend class ComplexVector;
  friend class ConstComplexMatrixView;
  friend class ComplexMatrixView;
  friend class ComplexMatrix;
  friend class ComplexIterator2D;
  friend class ConstComplexIterator2D;

  friend void mult_general(VectorView,
                           const ConstMatrixView&,
                           const ConstVectorView&);

  /** Returns the start index of the range. */
  Index get_start() const { return mstart; }
  /** Returns the extent of the range. */
  Index get_extent() const { return mextent; }
  /** Returns the stride of the range. */
  Index get_stride() const { return mstride; }

  /** Range of range. */
  Range operator()(const Range r) const {
    return (r.mextent < 0) ? (mextent < 0) ? Range(mstart + r.mstart * mstride,
                                                   joker,
                                                   r.mstride * mstride)
                                           : Range(mstart + r.mstart * mstride,
                                                   mextent,
                                                   r.mstride * mstride)
                           : Range(mstart + r.mstart * mstride,
                                   r.mextent,
                                   r.mstride * mstride);
  }

  Index operator()(const Index i) const { return mstart + i * mstride; };

 private:
  /** The start index. */
  Index mstart;
  /** The number of elements. -1 means extent to the end of the
      vector. */
  Index mextent;
  /** The stride. Can be positive or negative. */
  Index mstride;
};

/** The iterator class for sub vectors. This takes into account the
    defined stride. */
class Iterator1D {
 public:
  using difference_type = Index;
  using value_type = Numeric;
  using pointer = Numeric*;
  using reference = Numeric&;
  using iterator_category = std::random_access_iterator_tag;

  /** Default constructor. */
  Iterator1D() = default;

  /** Explicit constructor. */
  Iterator1D(Numeric* x, Index stride)
      : mx(x), mstride(stride) { /* Nothing to do here. */
  }

  // Operators:

  /** Prefix increment operator. */
  Iterator1D& operator++() {
    mx += mstride;
    return *this;
  }

  /** Dereferencing. */
  Numeric& operator*() const { return *mx; }

  /** Not equal operator, needed for algorithms like copy. */
  bool operator!=(const Iterator1D& other) const {
    if (mx != other.mx)
      return true;
    else
      return false;
  }

#ifdef __GLIBCXX__
  bool operator==(const Iterator1D& other) const { return !operator!=(other); }

  Index operator-(const Iterator1D& other) const {
    return (Index)(mx - other.mx) / mstride;
  }
#endif

  friend void copy(ConstIterator1D origin,
                   const ConstIterator1D& end,
                   Iterator1D target);

 private:
  /** Current position. */
  Numeric* mx{nullptr};
  /** Stride. */
  Index mstride{0};
};

/** The constant iterator class for sub vectors. This takes into
    account the defined stride. */
class ConstIterator1D {
 public:
  using difference_type = Index;
  using value_type = const Numeric;
  using pointer = const Numeric*;
  using reference = const Numeric&;
  using iterator_category = std::random_access_iterator_tag;

  /** Default constructor. */
  ConstIterator1D() = default;

  /** Explicit constructor. */
  ConstIterator1D(const Numeric* x, Index stride)
      : mx(x), mstride(stride) { /* Nothing to do here. */
  }

  // Operators:
  /** Prefix increment operator. */
  ConstIterator1D& operator++() {
    mx += mstride;
    return *this;
  }

  /** Dereferencing. */
  const Numeric& operator*() const { return *mx; }

  /** Not equal operator, needed for algorithms like copy. */
  bool operator!=(const ConstIterator1D& other) const {
    if (mx != other.mx)
      return true;
    else
      return false;
  }

  friend void copy(ConstIterator1D origin,
                   const ConstIterator1D& end,
                   Iterator1D target);

 private:
  /** Current position. */
  const Numeric* mx{nullptr};
  /** Stride. */
  Index mstride{0};
};

// Declare the vector class:
class Vector;

// Declare the MatrixView class
class MatrixView;

/** A constant view of a Vector.

    Together with the derived class VectorView this contains the main
    implementation of a Vector. The class Vector is just a special
    case of a VectorView which also allocates storage. */
class ConstVectorView {
 public:
  constexpr ConstVectorView(const ConstVectorView&) = default;
  constexpr ConstVectorView(ConstVectorView&&) = default;
  ConstVectorView& operator=(const ConstVectorView&) = default;
  ConstVectorView& operator=(ConstVectorView&&) = default;

  // Typedef for compatibility with STL
  typedef ConstIterator1D const_iterator;

  // Member functions:
  bool empty() const;
  Index nelem() const;
  Numeric sum() const;

  // Const index operators:
  /** Plain const index operator. */
  Numeric operator[](Index n) const {  // Check if index is valid:
    assert(0 <= n);
    assert(n < mrange.mextent);
    return get(n);
  }

  /** Get element implementation without assertions. */
  Numeric get(Index n) const {
    return *(mdata + mrange.mstart + n * mrange.mstride);
  }

  ConstVectorView operator[](const Range& r) const;
  friend Numeric operator*(const ConstVectorView& a, const ConstVectorView& b);

  // Functions returning iterators:
  ConstIterator1D begin() const;
  ConstIterator1D end() const;

  // Conversion to 1 column matrix:
  operator ConstMatrixView() const;

  //! Destructor
  virtual ~ConstVectorView() = default;

  // Friends:
  friend class VectorView;
  friend class ConstIterator2D;
  friend class ConstMatrixView;
  friend class ConstTensor3View;
  friend class ConstTensor4View;
  friend class ConstTensor5View;
  friend class ConstTensor6View;
  friend class ConstTensor7View;
  friend int poly_root_solve(Matrix& roots, Vector& coeffs);
  friend void mult(VectorView, const ConstMatrixView&, const ConstVectorView&);
  friend void mult(VectorView, const Sparse&, ConstVectorView);
  friend void transpose_mult(VectorView, const Sparse&, ConstVectorView);
  friend void mult_general(VectorView,
                           const ConstMatrixView&,
                           const ConstVectorView&);
  friend void lubacksub(VectorView,
                        ConstMatrixView,
                        ConstVectorView,
                        const ArrayOfIndex&);
  friend void diagonalize(MatrixView, VectorView, VectorView, ConstMatrixView);

  friend ConstMatrixViewMap MapToEigen(const ConstVectorView&);
  friend ConstMatrixViewMap MapToEigenCol(const ConstVectorView&);
  friend MatrixViewMap MapToEigen(VectorView&);
  friend MatrixViewMap MapToEigenCol(VectorView&);

  // A special constructor, that allows to make a ConstVectorView of a scalar.
  ConstVectorView(const Numeric& a);

 protected:
  // Constructors:
  ConstVectorView() = default;
  ConstVectorView(Numeric* data, const Range& range);
  ConstVectorView(Numeric* data, const Range& p, const Range& n);

  // Data members:
  // -------------
  /** The range of mdata that is actually used. */
  Range mrange{0, 0};
  /** Pointer to the plain C array that holds the data */
  Numeric* mdata{nullptr};
};

/** The VectorView class.

    This contains the main implementation of a vector. The class
    Vector is just a special case of subvector which also allocates
    storage.

    Unfortunately, names of element functions of derived classes hide
    the names of the original class, even if the arguments are
    different. This means that we have to redefine those element
    functions that can have different arguments, for example the
    constant index operators and iterators. */
class VectorView : public ConstVectorView {
 public:
  // Make const methods visible from base class
  using ConstVectorView::begin;
  using ConstVectorView::end;
  using ConstVectorView::operator[];
  using ConstVectorView::get;

  constexpr VectorView(const VectorView&) = default;

  VectorView(const Vector&);
  VectorView(Vector& v);

  // Typedef for compatibility with STL
  typedef Iterator1D iterator;

  /** Plain Index operator. */
  Numeric& operator[](Index n) {  // Check if index is valid:
    assert(0 <= n);
    assert(n < mrange.mextent);
    return get(n);
  }

  /** Get element implementation without assertions. */
  Numeric& get(Index n) {
    return *(mdata + mrange.mstart + n * mrange.mstride);
  }

  VectorView operator[](const Range& r);

  // Iterators:
  Iterator1D begin();
  Iterator1D end();

  // Assignment operators:
  VectorView& operator=(const ConstVectorView& v);
  VectorView& operator=(const VectorView& v);
  VectorView& operator=(const Vector& v);
  VectorView& operator=(const Array<Numeric>& v);
  VectorView& operator=(Numeric x);

  // Other operators:
  VectorView operator*=(Numeric x);
  VectorView operator/=(Numeric x);
  VectorView operator+=(Numeric x);
  VectorView operator-=(Numeric x);

  VectorView operator*=(const ConstVectorView& x);
  VectorView operator/=(const ConstVectorView& x);
  VectorView operator+=(const ConstVectorView& x);
  VectorView operator-=(const ConstVectorView& x);

  // Conversion to 1 column matrix:
  operator MatrixView();
  // Conversion to a plain C-array
  const Numeric* get_c_array() const;
  Numeric* get_c_array();

  //! Destructor
  virtual ~VectorView() = default;

  // Friends:
  friend class ConstIterator2D;
  friend class Iterator2D;
  friend class MatrixView;
  friend class Tensor3View;
  friend class Tensor4View;
  friend class Tensor5View;
  friend class Tensor6View;
  friend class Tensor7View;

  // A special constructor, that allows to make a VectorView of a scalar.
  VectorView(Numeric& a);

 protected:
  // Constructors:
  VectorView() = default;
  VectorView(Numeric* data, const Range& range);
  VectorView(Numeric* data, const Range& p, const Range& n);
};

/** The row iterator class for sub matrices. This takes into account the
    defined row stride. The iterator points to a row of the matrix,
    which acts just like a VectorView. */
class Iterator2D {
 public:
  // Constructors:
  /** Default constructor. */
  Iterator2D() = default;

  /** Explicit constructor. */
  Iterator2D(const VectorView& x, Index stride)
      : msv(x), mstride(stride) { /* Nothing to do here. */
  }

  // Operators:
  /** Prefix increment operator. */
  Iterator2D& operator++() {
    msv.mdata += mstride;
    return *this;
  }

  /** Not equal operator, needed for algorithms like copy. */
  bool operator!=(const Iterator2D& other) const {
    if (msv.mdata + msv.mrange.mstart !=
        other.msv.mdata + other.msv.mrange.mstart)
      return true;
    else
      return false;
  }

  /** The -> operator is needed, so that we can write i->begin() to get
    the 1D iterators. */
  VectorView* operator->() { return &msv; }

  /** Dereferencing. */
  VectorView& operator*() { return msv; }

 private:
  /** Current position. */
  VectorView msv;
  /** Row stride. */
  Index mstride{0};
};

/** The const row iterator class for sub matrices. This takes into account the
    defined row stride. The iterator points to a row of the matrix,
    which acts just like a VectorView. */
class ConstIterator2D {
 public:
  // Constructors:
  /** Default constructor. */
  ConstIterator2D() = default;

  /** Explicit constructor. */
  ConstIterator2D(const ConstVectorView& x, Index stride)
      : msv(x), mstride(stride) { /* Nothing to do here. */
  }

  // Operators:
  /** Prefix increment operator. */
  ConstIterator2D& operator++() {
    msv.mdata += mstride;
    return *this;
  }

  /** Not equal operator, needed for algorithms like copy. */
  bool operator!=(const ConstIterator2D& other) const {
    if (msv.mdata + msv.mrange.mstart !=
        other.msv.mdata + other.msv.mrange.mstart)
      return true;
    else
      return false;
  }

  /** The -> operator is needed, so that we can write i->begin() to get
    the 1D iterators. */
  const ConstVectorView* operator->() const { return &msv; }

  /** Dereferencing. */
  const ConstVectorView& operator*() const { return msv; }

 private:
  /** Current position. */
  ConstVectorView msv;
  /** Row stride. */
  Index mstride{0};
};

/** The Vector class. This is a subvector that also allocates storage
    automatically, and deallocates it when it is destroyed. We take
    all the functionality from VectorView. Additionally defined in
    this class are:

    1. Constructors and destructors (allocating memory).
    2. Assignment operator
    3. Assignment operator from scalar.
    4. Resize function.
*/
class Vector : public VectorView {
 public:
  // Constructors:
  Vector() = default;
  Vector(std::initializer_list<Numeric> init);
  explicit Vector(Index n);
  Vector(Index n, Numeric fill);
  Vector(Numeric start, Index extent, Numeric stride);
  Vector(const ConstVectorView& v);
  Vector(const Vector& v);
  Vector(Vector&& v) noexcept : VectorView(std::forward<VectorView>(v)) {
    v.mdata = nullptr;
  }
  Vector(const std::vector<Numeric>&);

  // Assignment operators:
  Vector& operator=(const Vector& v);
  Vector& operator=(Vector&& v) noexcept;
  Vector& operator=(std::initializer_list<Numeric> v);
  Vector& operator=(const Array<Numeric>& v);
  Vector& operator=(Numeric x);

  // Resize function:
  void resize(Index n);

  // Swap function:
  friend void swap(Vector& v1, Vector& v2);

  // Destructor:
  virtual ~Vector();
};

// Declare class Matrix:
class Matrix;

/** A constant view of a Matrix.

    This, together with the derived class MatrixView, contains the
    main implementation of a Matrix. It defines the concepts of
    MatrixView. Plus additionally the recursive subrange operator,
    which makes it possible to create a MatrixView from a subrange of
    a MatrixView.

    The class Matrix is just a special case of a MatrixView
    which also allocates storage. */
class ConstMatrixView {
 public:
  constexpr ConstMatrixView(const ConstMatrixView&) = default;
  constexpr ConstMatrixView(ConstMatrixView&&) = default;
  ConstMatrixView& operator=(const ConstMatrixView&) = default;
  ConstMatrixView& operator=(ConstMatrixView&&) = default;

  // Typedef for compatibility with STL
  typedef ConstIterator2D const_iterator;

  // Member functions:
  bool empty() const;
  Index nrows() const;
  Index ncols() const;

  // Const index operators:
  /** Plain const index operator. */
  Numeric operator()(Index r, Index c) const {  // Check if indices are valid:
    assert(0 <= r);
    assert(0 <= c);
    assert(r < mrr.mextent);
    assert(c < mcr.mextent);

    return get(r, c);
  }

  /** Get element implementation without assertions. */
  Numeric get(Index r, Index c) const {
    return *(mdata + mrr.mstart + r * mrr.mstride + mcr.mstart +
             c * mcr.mstride);
  }

  ConstMatrixView operator()(const Range& r, const Range& c) const;
  ConstVectorView operator()(const Range& r, Index c) const;
  ConstVectorView operator()(Index r, const Range& c) const;

  // Functions returning iterators:
  ConstIterator2D begin() const;
  ConstIterator2D end() const;

  // View on diagonal vector
  ConstVectorView diagonal() const;

  //! Destructor
  virtual ~ConstMatrixView() = default;

  // Friends:
  friend class MatrixView;
  friend class ConstIterator3D;
  friend class ConstVectorView;
  friend class ConstTensor3View;
  friend class ConstTensor4View;
  friend class ConstTensor5View;
  friend class ConstTensor6View;
  friend class ConstTensor7View;
  friend ConstMatrixView transpose(ConstMatrixView m);
  friend int poly_root_solve(Matrix& roots, Vector& coeffs);
  friend void mult(VectorView, const ConstMatrixView&, const ConstVectorView&);
  friend void mult(MatrixView, const ConstMatrixView&, const ConstMatrixView&);
  friend void mult(MatrixView, const Sparse&, const ConstMatrixView&);
  friend void mult(MatrixView, const ConstMatrixView&, const Sparse&);
  friend void mult_general(VectorView,
                           const ConstMatrixView&,
                           const ConstVectorView&);
  friend void mult_general(MatrixView,
                           const ConstMatrixView&,
                           const ConstMatrixView&);
  friend void ludcmp(Matrix&, ArrayOfIndex&, ConstMatrixView);
  friend void lubacksub(VectorView,
                        ConstMatrixView,
                        ConstVectorView,
                        const ArrayOfIndex&);
  friend void inv(MatrixView, ConstMatrixView);
  friend void diagonalize(MatrixView, VectorView, VectorView, ConstMatrixView);

  friend ConstMatrixViewMap MapToEigen(const ConstMatrixView&);
  friend MatrixViewMap MapToEigen(MatrixView&);

  friend ConstMatrix4x4ViewMap MapToEigen4x4(const ConstMatrixView&);
  friend Matrix4x4ViewMap MapToEigen4x4(MatrixView&);

 protected:
  // Constructors:
  ConstMatrixView() = default;
  ConstMatrixView(Numeric* data, const Range& r, const Range& c);
  ConstMatrixView(Numeric* data,
                  const Range& pr,
                  const Range& pc,
                  const Range& nr,
                  const Range& nc);

  // Data members:
  // -------------
  /** The row range of mdata that is actually used. */
  Range mrr{0, 0, 1};
  /** The column range of mdata that is actually used. */
  Range mcr{0, 0, 1};
  /** Pointer to the plain C array that holds the data */
  Numeric* mdata{nullptr};
};

/** The MatrixView class

    This contains the main implementation of a Matrix. It defines
    the concepts of MatrixView. Plus additionally the recursive
    subrange operator, which makes it possible to create a MatrixView
    from a subrange of a MatrixView. 

    The class Matrix is just a special case of a MatrixView
    which also allocates storage. */
class MatrixView : public ConstMatrixView {
 public:
  // Make const methods visible from base class
  using ConstMatrixView::begin;
  using ConstMatrixView::end;
  using ConstMatrixView::operator();
  using ConstMatrixView::get;

  constexpr MatrixView(const MatrixView&) = default;

  // Typedef for compatibility with STL
  typedef Iterator2D iterator;

  // Index Operators:
  /** Plain index operator. */
  Numeric& operator()(Index r, Index c) {  // Check if indices are valid:
    assert(0 <= r);
    assert(0 <= c);
    assert(r < mrr.mextent);
    assert(c < mcr.mextent);

    return get(r, c);
  }

  /** Get element implementation without assertions. */
  Numeric& get(Index r, Index c) {
    return *(mdata + mrr.mstart + r * mrr.mstride + mcr.mstart +
             c * mcr.mstride);
  }

  MatrixView operator()(const Range& r, const Range& c);
  VectorView operator()(const Range& r, Index c);
  VectorView operator()(Index r, const Range& c);

  // Functions returning iterators:
  Iterator2D begin();
  Iterator2D end();

  // Assignment operators:
  MatrixView& operator=(const ConstMatrixView& v);
  MatrixView& operator=(const MatrixView& v);
  MatrixView& operator=(const Matrix& v);
  MatrixView& operator=(const ConstVectorView& v);
  MatrixView& operator=(Numeric x);

  // Other operators:
  MatrixView& operator*=(Numeric x);
  MatrixView& operator/=(Numeric x);
  MatrixView& operator+=(Numeric x);
  MatrixView& operator-=(Numeric x);

  MatrixView& operator*=(const ConstMatrixView& x);
  MatrixView& operator/=(const ConstMatrixView& x);
  MatrixView& operator+=(const ConstMatrixView& x);
  MatrixView& operator-=(const ConstMatrixView& x);

  MatrixView& operator*=(const ConstVectorView& x);
  MatrixView& operator/=(const ConstVectorView& x);
  MatrixView& operator+=(const ConstVectorView& x);
  MatrixView& operator-=(const ConstVectorView& x);

  // Conversion to a plain C-array
  const Numeric* get_c_array() const;
  Numeric* get_c_array();

  //! Destructor
  virtual ~MatrixView() = default;

  // Friends:
  friend class VectorView;
  friend class Iterator3D;
  friend class Tensor3View;
  friend class Tensor4View;
  friend class Tensor5View;
  friend class Tensor6View;
  friend class Tensor7View;
  friend ConstMatrixView transpose(ConstMatrixView m);
  friend MatrixView transpose(MatrixView m);
  friend void mult(MatrixView, const ConstMatrixView&, const ConstMatrixView&);

 protected:
  // Constructors:
  MatrixView() = default;
  MatrixView(Numeric* data, const Range& r, const Range& c);
  MatrixView(Numeric* data,
             const Range& pr,
             const Range& pc,
             const Range& nr,
             const Range& nc);
};

/** The Matrix class. This is a MatrixView that also allocates storage
    automatically, and deallocates it when it is destroyed. We take
    all the functionality from MatrixView. Additionally defined here
    are: 

    1. Constructors and destructor.
    2. Assignment operator from scalar.
    3. Resize function. */
class Matrix : public MatrixView {
 public:
  // Constructors:
  Matrix() = default;
  Matrix(Index r, Index c);
  Matrix(Index r, Index c, Numeric fill);
  Matrix(const ConstMatrixView& v);
  Matrix(const Matrix& v);
  Matrix(Matrix&& v) noexcept : MatrixView(std::forward<MatrixView>(v)) {
    v.mdata = nullptr;
  }

  // Assignment operators:
  Matrix& operator=(const Matrix& x);
  Matrix& operator=(Matrix&& x) noexcept;
  Matrix& operator=(Numeric x);
  Matrix& operator=(const ConstVectorView& v);

  // Resize function:
  void resize(Index r, Index c);

  // Swap function:
  friend void swap(Matrix& m1, Matrix& m2);

  // Destructor:
  virtual ~Matrix();

  Numeric* get_raw_data() { return mdata; }
};

// Function declarations:
// ----------------------

void copy(ConstIterator1D origin,
          const ConstIterator1D& end,
          Iterator1D target);

void copy(Numeric x, Iterator1D target, const Iterator1D& end);

void copy(ConstIterator2D origin,
          const ConstIterator2D& end,
          Iterator2D target);

void copy(Numeric x, Iterator2D target, const Iterator2D& end);

void mult(VectorView y, const ConstMatrixView& M, const ConstVectorView& x);

void mult_general(MatrixView A,
                  const ConstMatrixView& B,
                  const ConstMatrixView& C);

void mult(MatrixView A, const ConstMatrixView& B, const ConstMatrixView& C);

void mult_general(MatrixView A,
                  const ConstMatrixView& B,
                  const ConstMatrixView& C);

void cross3(VectorView c, const ConstVectorView& a, const ConstVectorView& b);

Numeric vector_angle(ConstVectorView a, ConstVectorView b);

void proj(Vector& c, ConstVectorView a, ConstVectorView b);

ConstMatrixView transpose(ConstMatrixView m);

MatrixView transpose(MatrixView m);

void transform(VectorView y, double (&my_func)(double), ConstVectorView x);

void transform(MatrixView y, double (&my_func)(double), ConstMatrixView x);

Numeric max(const ConstVectorView& x);

Numeric max(const ConstMatrixView& x);

Numeric min(const ConstVectorView& x);

Numeric min(const ConstMatrixView& x);

Numeric mean(const ConstVectorView& x);

Numeric mean(const ConstMatrixView& x);

Numeric operator*(const ConstVectorView& a, const ConstVectorView& b);

std::ostream& operator<<(std::ostream& os, const ConstVectorView& v);

std::ostream& operator<<(std::ostream& os, const ConstMatrixView& v);

std::ostream& operator<<(std::ostream& os, const Range& r);

// Converts constant matrix to constant eigen map
ConstMatrixViewMap MapToEigen(const ConstMatrixView& A);
// Converts constant vector to constant eigen row-view
ConstMatrix4x4ViewMap MapToEigen4x4(const ConstMatrixView& A);
// Converts constant vector to constant eigen row-view
ConstMatrixViewMap MapToEigen(const ConstVectorView& A);
// Converts constant vector to constant eigen row-view
ConstMatrixViewMap MapToEigenRow(const ConstVectorView& A);
// Converts constant vector to constant eigen column-view
ConstMatrixViewMap MapToEigenCol(const ConstVectorView& A);
// Converts matrix to eigen map
MatrixViewMap MapToEigen(MatrixView& A);
// Converts vector to eigen map row-view
Matrix4x4ViewMap MapToEigen4x4(MatrixView& A);
// Converts vector to eigen map row-view
MatrixViewMap MapToEigen(VectorView& A);
// Converts vector to eigen map row-view
MatrixViewMap MapToEigenRow(VectorView& A);
// Converts vector to eigen map column-view
MatrixViewMap MapToEigenCol(VectorView& A);

////////////////////////////////
// Helper function for debugging
#ifndef NDEBUG

Numeric debug_matrixview_get_elem(MatrixView& mv, Index r, Index c);

#endif
////////////////////////////////

#endif  // matpackI_h
