# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Copyright(C) 2020 Max-Planck-Society


import ducc0.wgridder as ng
import numpy as np
import pytest
from numpy.testing import assert_allclose

pmp = pytest.mark.parametrize


def _l2error(a, b):
    return np.sqrt(np.sum(np.abs(a-b)**2)/np.maximum(np.sum(np.abs(a)**2),
                                                     np.sum(np.abs(b)**2)))


def explicit_gridder(uvw, freq, ms, wgt, nxdirty, nydirty, xpixsize, ypixsize,
                     apply_w):
    speedoflight = 299792458.
    x, y = np.meshgrid(*[-ss/2 + np.arange(ss) for ss in [nxdirty, nydirty]],
                       indexing='ij')
    x *= xpixsize
    y *= ypixsize
    res = np.zeros((nxdirty, nydirty))
    eps = x**2+y**2
    if apply_w:
        nm1 = -eps/(np.sqrt(1.-eps)+1.)
        n = nm1+1
    else:
        nm1 = 0.
        n = 1.
    for row in range(ms.shape[0]):
        for chan in range(ms.shape[1]):
            phase = (freq[chan]/speedoflight *
                     (x*uvw[row, 0] + y*uvw[row, 1] - uvw[row, 2]*nm1))
            if wgt is None:
                res += (ms[row, chan]*np.exp(2j*np.pi*phase)).real
            else:
                res += (ms[row, chan]*wgt[row, chan]
                        * np.exp(2j*np.pi*phase)).real
    return res/n


@pmp('nxdirty', [16, 64])
@pmp('nydirty', [64])
@pmp("nrow", (1, 100))
@pmp("nchan", (1, 7))
@pmp("epsilon", list(10.**np.linspace(-2., -12., 100)))
@pmp("singleprec", (False,))
@pmp("wstacking", (True,))
@pmp("use_wgt", (True,))
@pmp("nthreads", (1, 10))
@pmp("fov", (10.,))
def test_ms2dirty_against_wdft2(nxdirty, nydirty, nrow, nchan, epsilon,
                                singleprec, wstacking, use_wgt, fov, nthreads):
    if singleprec and epsilon < 5e-5:
        return
    rng = np.random.default_rng()
    pixsizex = fov*np.pi/180/nxdirty
    pixsizey = fov*np.pi/180/nydirty*1.1
    speedoflight, f0 = 299792458., 1e9
    freq = f0 + np.arange(nchan)*(f0/nchan)
    uvw = (rng.random((nrow, 3))-0.5)/(pixsizex*f0/speedoflight)
    ms = rng.random((nrow, nchan))-0.5 + 1j*(rng.random((nrow, nchan))-0.5)
    wgt = rng.random((nrow, 1)) if use_wgt else None
    wgt = np.broadcast_to(wgt, (nrow, nchan)) if use_wgt else None
    if singleprec:
        ms = ms.astype("c8")
        if wgt is not None:
            wgt = wgt.astype("f4")
    try:
        dirty = ng.ms2dirty(
            uvw, freq, ms, wgt, nxdirty, nydirty, pixsizex,
            pixsizey, 0, 0, epsilon, wstacking, nthreads, 0).astype("f8")
    except:
        # no matching kernel was found
        pytest.skip()
    ref = explicit_gridder(uvw, freq, ms, wgt, nxdirty, nydirty, pixsizex,
                           pixsizey, wstacking)
    assert_allclose(_l2error(dirty, ref), 0, atol=epsilon)
