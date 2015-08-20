/*
 * Copyright 2011-2013 Blender Foundation
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

CCL_NAMESPACE_BEGIN

#define Buf_F(x, y, o) (buffers[((y) * w + (x)) * kernel_data.film.pass_stride + (o)])
#define Buf_F3(x, y, o) *((float3*) (buffers + ((y) * w + (x)) * kernel_data.film.pass_stride + (o)))
#define Buf_F4(x, y, o) *((float4*) (buffers + ((y) * w + (x)) * kernel_data.film.pass_stride + (o)))

ccl_device_inline void filter_get_color_passes(KernelGlobals *kg, int mode, int &m_C, int &v_C)
{
	switch(mode) {
		case FILTER_DIFFUSE_DIRECT:
			m_C = kernel_data.film.pass_diffuse_direct;
			v_C = kernel_data.film.lwr_diffuse_direct;
			break;
		case FILTER_DIFFUSE_INDIRECT:
			m_C = kernel_data.film.pass_diffuse_indirect;
			v_C = kernel_data.film.lwr_diffuse_indirect;
			break;
		case FILTER_GLOSSY_DIRECT:
			m_C = kernel_data.film.pass_glossy_direct;
			v_C = kernel_data.film.lwr_glossy_direct;
			break;
		case FILTER_GLOSSY_INDIRECT:
			m_C = kernel_data.film.pass_glossy_indirect;
			v_C = kernel_data.film.lwr_glossy_indirect;
			break;
		case FILTER_TRANSMISSION_DIRECT:
			m_C = kernel_data.film.pass_transmission_direct;
			v_C = kernel_data.film.lwr_transmission_direct;
			break;
		case FILTER_TRANSMISSION_INDIRECT:
			m_C = kernel_data.film.pass_transmission_indirect;
			v_C = kernel_data.film.lwr_transmission_indirect;
			break;
		case FILTER_COMBINED:
		default:
			m_C = kernel_data.film.pass_lwr + 14;
			v_C = kernel_data.film.pass_lwr + 17;
			break;
	}
}

ccl_device void kernel_filter1_pixel(KernelGlobals *kg, float *buffers, int x, int y, int w, int h, int samples, int mode, int halfWindow, float bandwidthFactor, float* storage)
{
	float invS = 1.0f / samples;
	float invSv = 1.0f / (samples - 1);

	int2 lo = make_int2(max(x - halfWindow, 0), max(y - halfWindow, 0));
	int2 hi = make_int2(min(x + halfWindow, w-1), min(y + halfWindow, h-1));
	int num = (hi.x - lo.x + 1) * (hi.y - lo.y + 1);

	int m_D = kernel_data.film.pass_lwr     , v_D = kernel_data.film.pass_lwr + 1 ,
	    m_N = kernel_data.film.pass_lwr + 2 , v_N = kernel_data.film.pass_lwr + 5 ,
	    m_T = kernel_data.film.pass_lwr + 8 , v_T = kernel_data.film.pass_lwr + 11,
	    m_C, v_C;
	filter_get_color_passes(kg, mode, m_C, v_C);

	float3 meanT = make_float3(0.0f, 0.0f, 0.0f);
	float3 meanN = make_float3(0.0f, 0.0f, 0.0f);
	float meanD = 0.0f;

	for(int py = lo.y; py <= hi.y; py++) {
		for(int px = lo.x; px <= hi.x; px++) {
			meanT += Buf_F3(px, py, m_T) * invS;
			meanN += Buf_F3(px, py, m_N) * invS;
			meanD += Buf_F (px, py, m_D) * invS;
		}
	}
	meanT /= num;
	meanN /= num;
	meanD /= num;

	float delta[9], transform[81], norm;
	int rank;
	/* Generate transform */
	{
		float nD = 0.0f, nT = 0.0f, nN = 0.0f;
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				nD = max(fabsf(Buf_F(px, py, m_D) * invS - meanD), nD);
				nN = max(len_squared(Buf_F3(px, py, m_N) * invS - meanN), nN);
				nT = max(len_squared(Buf_F3(px, py, m_T) * invS - meanT), nT);
			}
		}

		nD = 1.0f / max(nD, 0.01f);
		nN = 1.0f / max(sqrtf(nN), 0.01f);
		nT = 1.0f / max(sqrtf(nT), 0.01f);

		norm = 0.0f;
		for(int i = 0; i < 81; i++)
			transform[i] = 0.0f;
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				delta[0] = ((float) px - x) / halfWindow;
				delta[1] = ((float) py - y) / halfWindow;
				delta[2] = (Buf_F(px, py, m_D) * invS - meanD) * nD;
				float3 dN = (Buf_F3(px, py, m_N) * invS - meanN) * nN;
				delta[3] = dN.x;
				delta[4] = dN.y;
				delta[5] = dN.z;
				float3 dT = (Buf_F3(px, py, m_T) * invS - meanT) * nT;
				delta[6] = dT.x;
				delta[7] = dT.y;
				delta[8] = dT.z;

				for(int r = 0; r < 9; r++)
					for(int c = r; c < 9; c++)
						transform[9*r+c] += delta[r]*delta[c];

				norm += nD * nD * saturate(Buf_F(px, py, v_D) * invSv) * invS;
				norm += nN * nN * 3.0f * average(saturate(Buf_F3(px, py, v_N) * invSv)) * invS;
				norm += nT * nT * 3.0f * average(saturate(Buf_F3(px, py, v_T) * invSv)) * invS;
			}
		}

		/* Here, transform is self-adjoint (TODO term symmetric?) by construction, so one half can be copied from the other one */
		for(int r = 1; r < 9; r++)
			for(int c = 0; c < r; c++)
				transform[9*r+c] = transform[9*c+r];

		float V[81], S[9];
		rank = svd(transform, V, S, 9);

		for(int i = 0; i < 9; i++)
			S[i] = sqrtf(fabsf(S[i]));

		float threshold = 0.01f + 2.0f * (sqrtf(norm) / (sqrtf(rank) * 0.5f));
		rank = 0;

		/* Truncate matrix to reduce the rank */
		for(int c = 0; c < 9; c++) {
			float singular = sqrtf(S[c]); //TODO 2x sqrtf?
			if((singular > threshold) || (c < 2)) { /* Image position is always used */
				transform[     c] = V[     c] / halfWindow;
				transform[ 9 + c] = V[ 9 + c] / halfWindow;
				transform[18 + c] = V[18 + c] * nD;
				transform[27 + c] = V[27 + c] * nN;
				transform[36 + c] = V[36 + c] * nN;
				transform[45 + c] = V[45 + c] * nN;
				transform[54 + c] = V[54 + c] * nT;
				transform[63 + c] = V[63 + c] * nT;
				transform[72 + c] = V[72 + c] * nT;
				rank++;
			}
		}
	}

	float bi[9];
	/* Approximate bandwidths */
	{
		const int size = 2*rank+1;
		float z[18]; /* Only 0 to rank-1 gets used (and rank to 2*rank-1 for the squared values) */
		float A[19*19];
		float3 XtB[19];

		meanD = Buf_F (x, y, m_D) * invS;
		meanN = Buf_F3(x, y, m_N) * invS;
		meanT = Buf_F3(x, y, m_T) * invS;

		for(int i = 0; i < size*size; i++)
			A[i] = 0.0f;
		for(int i = 0; i < size; i++)
			XtB[i] = make_float3(0.0f, 0.0f, 0.0f);
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					if(fabsf(z[col]) < 1.0f)
						weight *= 0.75f * (1.0f - z[col]*z[col]);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);
					for(int i = 0; i < rank; i++)
						z[rank+i] = z[i]*z[i];
					A[0] += weight;
					XtB[0] += Buf_F3(px, py, m_C) * invS * weight;
					for(int c = 1; c < size; c++) {
						float lweight = weight * z[c-1];
						A[c] += lweight;
						XtB[c] += Buf_F3(px, py, m_C) * invS * lweight;
					}
					for(int r = 1; r < size; r++)
						for(int c = r; c < size; c++)
							A[r*size + c] += weight * z[r-1] * z[c-1];
				}
			}
		}

		for(int i = 0; i < size; i++)
			A[i*size+i] += 0.001f;

		cholesky(A, size, A);

		XtB[0] /= A[0];
		for(int i = 1; i < size; i++) {
			float3 s = make_float3(0.0f, 0.0f, 0.0f);
			for(int j = 0; j < i; j++)
				s += A[i*size + j] * XtB[j];
			XtB[i] = (XtB[i] - s) / A[i*size + i];
		}
		XtB[size-1] /= A[(size-1)*size + (size-1)];
		for(int i = size-2; i >= 0; i--) {
			float3 s = make_float3(0.0f, 0.0f, 0.0f);
			for(int j = size-1; j > i; j--)
				s += A[j*size + i] * XtB[j];
			XtB[i] = (XtB[i] - s) / A[i*size + i];
		}

		for(int i = 0; i < 9; i++) //TODO < rank enough? Why +0.16?
		{
			bi[i] = bandwidthFactor / sqrtf(fabsf(2.0f * average(fabs(XtB[i + rank + 1]))) + 0.16f);
		}

	}

	double bias_XtX[4], bias_XtB[2];
	double  var_XtX[4],  var_XtB[2];
	bias_XtX[0] = bias_XtX[1] = bias_XtX[2] = bias_XtX[3] = bias_XtB[0] = bias_XtB[1] = 0.0;
	 var_XtX[0] =  var_XtX[1] =  var_XtX[2] =  var_XtX[3] =  var_XtB[0] =  var_XtB[1] = 0.0;
	for(int g = 0; g < 5; g++) {
		float A[100], z[9], invL[100], invA[10];
		for(int i = 0; i < 100; i++)
			A[i] = 0.0f;

		float g_w = (g + 1.0f) / 5.0f;
		float bandwidth[9];
		for(int i = 0; i < rank; i++)
			bandwidth[i] = g_w * bi[i];

		float cCm = average(Buf_F3(x, y, m_C))*invS;
		float cCs = sqrtf(average(Buf_F3(x, y, v_C))*invS*invSv);

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, m_C))*invS - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);
					A[0] += weight;
					for(int c = 1; c < rank+1; c++)
						A[c] += weight*z[c-1];
					for(int r = 1; r < rank+1; r++)
						for(int c = r; c < rank+1; c++)
							A[r*(rank+1)+c] += weight*z[c-1]*z[r-1];
				}
			}
		}
		for(int i = 0; i < rank+1; i++)
			A[i*(rank+1)+i] += 0.0001f;

		cholesky(A, rank+1, A);

		for(int i = 0; i < 100; i++)
			invL[i] = 0.0f;

		for(int j = rank; j >= 0; j--) {
			invL[j*(rank+1)+j] = 1.0f / A[j*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				for(int i = j+1; i < rank+1; i++)
					invL[k*(rank+1)+j] += invL[k*(rank+1)+i] * A[i*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				invL[k*(rank+1)+j] *= -invL[j*(rank+1)+j];
		}

		for(int i = 0; i < rank+1; i++) {
			invA[i] = 0.0f;
			for(int k = i; k < rank+1; k++)
				invA[i] += invL[k*(rank+1)] * invL[k*(rank+1)+i];
		}

		float3 beta, err_beta;
		beta = err_beta = make_float3(0.0f, 0.0f, 0.0f);
		float err_sum_l, var, err_var;
		err_sum_l = var = err_var = 0.0f;

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, m_C))*invS - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);

					float l = invA[0];
					for(int f = 0; f < rank; f++)
						l += z[f]*invA[f+1];
					l *= weight;

					beta += l * Buf_F3(px, py, m_C)*invS;
					var += l*l * max(average(Buf_F3(px, py, v_C))*invSv*invS, 0.0f);

					if(l > 0.0f) {
						err_beta += l * Buf_F3(px, py, m_C)*invS;
						err_var += l*l * max(average(Buf_F3(px, py, v_C))*invSv*invS, 0.0f);
						err_sum_l += l;
					}
				}
			}
		}

		if(beta.x < 0.0f || beta.y < 0.0f || beta.z < 0.0f) {
			err_sum_l = max(err_sum_l, 0.001f);
			beta = err_beta / err_sum_l;
			var = err_var / (err_sum_l*err_sum_l);
		}

		double h2 = g_w*g_w;
		double bias = average(beta - (Buf_F3(x, y, m_C)*invS));
		double i_h_r = pow(g_w, -rank);
		double svar = max(samples*var, 0.0f);

		bias_XtX[0] += 1.0;
		bias_XtX[1] += h2;
		bias_XtX[2] += h2;
		bias_XtX[3] += h2*h2;
		bias_XtB[0] += bias;
		bias_XtB[1] += h2*bias;

		var_XtX[0] += 1.0;
		var_XtX[1] += i_h_r;
		var_XtX[2] += i_h_r;
		var_XtX[3] += i_h_r*i_h_r;
		var_XtB[0] += svar;
		var_XtB[1] += svar*i_h_r;
	}

	double coef_bias[2], coef_var[2];

	double i_det = 1.0 / (bias_XtX[0]*bias_XtX[3] - bias_XtX[1]*bias_XtX[2] + 0.0001);
	coef_bias[0] = i_det*bias_XtX[3]*bias_XtB[0] - i_det*bias_XtX[1]*bias_XtB[1];
	coef_bias[1] = i_det*bias_XtX[0]*bias_XtB[1] - i_det*bias_XtX[2]*bias_XtB[0];

	i_det = 1.0 / (var_XtX[0]*var_XtX[3] - var_XtX[1]*var_XtX[2] + 0.0001);
	coef_var[0] = i_det*var_XtX[3]*var_XtB[0] - i_det*var_XtX[1]*var_XtB[1];
	coef_var[1] = i_det*var_XtX[0]*var_XtB[1] - i_det*var_XtX[2]*var_XtB[0];
	if(coef_var[1] < 0.0) {
		coef_var[0] = 0.0f;
		coef_var[1] = fabsf(coef_var[1]);
	}

	double h_opt = pow((rank * coef_var[1]) / (4.0 * coef_bias[1] * coef_bias[1] * samples), 1.0 / (4 + rank));
	h_opt = clamp(h_opt, 0.2, 1.0);

	for(int i = 0; i < 9; i++)
		storage[i] = (i < rank)? bi[i]: 0.0f;
	for(int i = 0; i < 81; i++)
		storage[i+9] = transform[i];
	storage[90] = __int_as_float(rank);
	storage[91] = meanD;
	storage[92] = meanN.x;
	storage[93] = meanN.y;
	storage[94] = meanN.z;
	storage[95] = meanT.x;
	storage[96] = meanT.y;
	storage[97] = meanT.z;
	storage[98] = h_opt;
}

ccl_device void kernel_filter2_pixel(KernelGlobals *kg, float *buffers, int x, int y, int w, int h, int samples, int mode, int halfWindow, float bandwidthFactor, float *storage, int4 tile)
{
	float invS = 1.0f / samples;
	float invSv = 1.0f / (samples - 1);

	int2 lo = make_int2(max(x - halfWindow, 0), max(y - halfWindow, 0));
	int2 hi = make_int2(min(x + halfWindow, w-1), min(y + halfWindow, h-1));

	int m_D = kernel_data.film.pass_lwr, m_N = kernel_data.film.pass_lwr + 2, m_T = kernel_data.film.pass_lwr + 8,
	    m_C, v_C;
	filter_get_color_passes(kg, mode, m_C, v_C);

	//Load storage data
	float *bi = storage;
	float *transform = storage + 9;
	int rank = __float_as_int(storage[90]);
	float meanD = storage[91];
	float3 meanN = make_float3(storage[92], storage[93], storage[94]);
	float3 meanT = make_float3(storage[95], storage[96], storage[97]);

	float h_opt = 0.0f, sum_w = 0.0f;
	for(int dy = -3; dy < 4; dy++) {
		if(dy+y < tile.y || dy+y >= tile.y+tile.w) continue;
		for(int dx = -3; dx < 4; dx++) {
			if(dx+x < tile.x || dx+x >= tile.x+tile.z) continue;
			float we = expf(-0.5f*(dx*dx+dy*dy));
			h_opt += we*storage[98 + 99*(dx + tile.z*dy)];
			sum_w += we;
		}
	}
	h_opt /= sum_w;
	Buf_F(x, y, 4) = h_opt*samples;

	{
		float A[100], z[9], invL[100], invA[10];
		for(int i = 0; i < 100; i++)
			A[i] = 0.0f;

		float bandwidth[9];
		for(int i = 0; i < rank; i++)
			bandwidth[i] = (float) h_opt * bi[i];

		float cCm = average(Buf_F3(x, y, m_C))*invS;
		float cCs = sqrtf(average(Buf_F3(x, y, v_C))*invS*invSv);

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(cCm - average(Buf_F3(px, py, m_C))*invS) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);
					A[0] += weight;
					for(int c = 1; c < rank+1; c++)
						A[c] += weight*z[c-1];
					for(int r = 1; r < rank+1; r++)
						for(int c = r; c < rank+1; c++)
							A[r*(rank+1)+c] += weight*z[c-1]*z[r-1];
				}
			}
		}
		for(int i = 0; i < rank+1; i++)
			A[i*(rank+1)+i] += 0.0001f;

		cholesky(A, rank+1, A);

		for(int i = 0; i < 100; i++)
			invL[i] = 0.0f;

		for(int j = rank; j >= 0; j--) {
			invL[j*(rank+1)+j] = 1.0f / A[j*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				for(int i = j+1; i < rank+1; i++)
					invL[k*(rank+1)+j] += invL[k*(rank+1)+i] * A[i*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				invL[k*(rank+1)+j] *= -invL[j*(rank+1)+j];
		}

		for(int i = 0; i < rank+1; i++) {
			invA[i] = 0.0f;
			for(int k = i; k < rank+1; k++)
				invA[i] += invL[k*(rank+1)] * invL[k*(rank+1)+i];
		}

		float3 out = make_float3(0.0f, 0.0f, 0.0f);
		float3 outP = make_float3(0.0f, 0.0f, 0.0f);
		float Psum = 0.0f;

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, m_C))*invS - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);

					float l = invA[0];
					for(int f = 0; f < rank; f++)
						l += z[f]*invA[f+1];
					l *= weight;

					out += l * Buf_F3(px, py, m_C)*invS;
					if(l > 0.0f) {
						outP += l * Buf_F3(px, py, m_C)*invS;
						Psum += l;
					}
				}
			}
		}

		if(out.x < 0.0f || out.y < 0.0f || out.z < 0.0f)
			out = outP / max(Psum, 0.001f);

		out *= samples;
		if(mode == FILTER_COMBINED) {
			float o_alpha = Buf_F(x, y, 3);
			Buf_F4(x, y, 0) = make_float4(out.x, out.y, out.z, o_alpha);
		}
		else {
			Buf_F4(x, y, 0) += samples*make_float4(out.x, out.y, out.z, 0.0f);
		}
	}
}

CCL_NAMESPACE_END
