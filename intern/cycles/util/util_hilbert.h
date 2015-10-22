/*
 * Copyright 2011-2015 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_HILBERT_H__
#define __UTIL_HILBERT_H__

CCL_NAMESPACE_BEGIN

inline int hilbert_roundpow2(int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

//Source: http://en.wikipedia.org/wiki/Hilbert_curve
inline void hilbert_rot(int n, int *x, int *y, int rx, int ry) {
	if (ry == 0) {
		if (rx == 1) {
			*x = n-1 - *x;
			*y = n-1 - *y;
		}

		//Swap x and y
		int t  = *x;
		*x = *y;
		*y = t;
	}
}

inline int hilbert_xy2d (int n, int x, int y) {
	int rx, ry, s, d=0;
	for (s=n/2; s>0; s/=2) {
		rx = (x & s) > 0;
		ry = (y & s) > 0;
		d += s * s * ((3 * rx) ^ ry);
		hilbert_rot(s, &x, &y, rx, ry);
	}
	return d;
}

inline void hilbert_d2xy(int n, int d, int *x, int *y) {
	int rx, ry, s, t=d;
	*x = *y = 0;
	for (s=1; s<n; s*=2) {
		rx = 1 & (t/2);
		ry = 1 & (t ^ rx);
		hilbert_rot(s, x, y, rx, ry);
		*x += s * rx;
		*y += s * ry;
		t /= 4;
	}
}

CCL_NAMESPACE_END

#endif // __UTIL_HILBERT_H__

