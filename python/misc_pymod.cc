/*
 *  This file is part of DUCC.
 *
 *  DUCC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  DUCC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with DUCC; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  DUCC is being developed at the Max-Planck-Institut fuer Astrophysik
 */

/*
 *  Copyright (C) 2020-2021 Max-Planck-Society
 *  Author: Martin Reinecke
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <vector>

#include "ducc0/infra/mav.h"
#include "ducc0/infra/transpose.h"
#include "ducc0/math/fft.h"
#include "ducc0/math/constants.h"
#include "ducc0/bindings/pybind_utils.h"

namespace ducc0 {

namespace detail_pymodule_misc {

using namespace std;
namespace py = pybind11;

void upsample_to_cc(const mav<double,2> &in, bool has_np, bool has_sp,
  mav<double,2> &out)
  {
  size_t ntheta_in = in.shape(0),
         ntheta_out = out.shape(0),
         nphi = in.shape(1);
  MR_assert(out.shape(1)==nphi, "phi dimensions must be equal");
  MR_assert((nphi&1)==0, "nphi must be even");
  size_t nrings_in = 2*ntheta_in-has_np-has_sp;
  size_t nrings_out = 2*ntheta_out-2;
  MR_assert(nrings_out>=nrings_in, "number of rings must increase");
  constexpr size_t delta=128;
  for (size_t js=0; js<nphi; js+=delta)
    {
    size_t je = min(js+delta, nphi);
    mav<double,2> tmp({nrings_out,je-js});
    fmav<double> ftmp(tmp);
    mav<double,2> tmp2(tmp.vdata(),{nrings_in, je-js}, true);
    fmav<double> ftmp2(tmp2);
    // enhance to "double sphere"
    if (has_np)
      for (size_t j=js; j<je; ++j)
        tmp2.v(0,j-js) = in(0,j);
    if (has_sp)
      for (size_t j=js; j<je; ++j)
        tmp2.v(ntheta_in-1,j-js) = in(ntheta_in-1,j);
    for (size_t i=has_np, i2=nrings_in-1; i+has_sp<ntheta_in; ++i,--i2)
      for (size_t j=js,j2=js+nphi/2; j<je; ++j,++j2)
        {
        if (j2>=nphi) j2-=nphi;
        tmp2.v(i,j-js) = in(i,j);
        tmp2.v(i2,j-js) = in(i,j2);
        }
    // FFT in theta direction
    r2r_fftpack(ftmp2,ftmp2,{0},true,true,1./nrings_in,0);
    if (!has_np)  // shift
      {
      double ang = -pi/nrings_in;
      for (size_t i=1; i<ntheta_in; ++i)
        {
        complex<double> rot(cos(i*ang),sin(i*ang));
        for (size_t j=js; j<je; ++j)
          {
          complex<double> ctmp(tmp2(2*i-1,j-js),tmp2(2*i,j-js));
          ctmp *= rot;
          tmp2.v(2*i-1,j-js) = ctmp.real();
          tmp2.v(2*i  ,j-js) = ctmp.imag();
          }
        }
      }
    // zero-padding
    for (size_t i=nrings_in; i<nrings_out; ++i)
      for (size_t j=js; j<je; ++j)
        tmp.v(i,j-js) = 0;
    // FFT back
    r2r_fftpack(ftmp,ftmp,{0},false,false,1.,0);
    // copy to output map
    for (size_t i=0; i<ntheta_out; ++i)
      for (size_t j=js; j<je; ++j)
        out.v(i,j) = tmp(i,j-js);
    }
  }

py::array Py_upsample_to_cc(const py::array &in, size_t nrings_out, bool has_np,
  bool has_sp, py::object &out_)
  {
  auto in2 = to_mav<double,2>(in);
  auto out = get_optional_Pyarr<double>(out_, {nrings_out,size_t(in.shape(1))});
  auto out2 = to_mav<double,2>(out,true);
    MR_assert(out2.writable(),"x1");
  upsample_to_cc(in2, has_np, has_sp, out2);
  return move(out);
  }

template<typename T> py::array Py2_transpose(const py::array &in, py::array &out)
  {
  auto in2 = to_fmav<T>(in, false);
  auto out2 = to_fmav<T>(out, true);
  transpose(in2, out2, [](const T &in, T &out){out=in;});
  return out;
  }

py::array Py_transpose(const py::array &in, py::array &out)
  {
  if (isPyarr<float>(in))
    return Py2_transpose<float>(in, out);
  if (isPyarr<double>(in))
    return Py2_transpose<double>(in, out);
  if (isPyarr<complex<float>>(in))
    return Py2_transpose<complex<float>>(in, out);
  if (isPyarr<complex<double>>(in))
    return Py2_transpose<complex<double>>(in, out);
  if (isPyarr<int>(in))
    return Py2_transpose<int>(in, out);
  if (isPyarr<long>(in))
    return Py2_transpose<long>(in, out);
  MR_fail("unsupported datatype");
  }


const char *misc_DS = R"""(
Various unsorted utilities

Notes
-----

The functionality in this module is not considered to have a stable interface
and also may be moved to other modules in the future. If you use it, be prepared
to adjust your code at some point ion the future!
)""";

void add_misc(py::module_ &msup)
  {
  using namespace pybind11::literals;
  auto m = msup.def_submodule("misc");
  m.doc() = misc_DS;

  m.def("upsample_to_cc",&Py_upsample_to_cc, "in"_a, "nrings_out"_a,
    "has_np"_a, "has_sp"_a, "out"_a=py::none());

  m.def("transpose",&Py_transpose, "in"_a, "out"_a);
  }

}

using detail_pymodule_misc::add_misc;

}

