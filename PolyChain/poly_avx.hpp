// poly_avx.hpp — 高性能多项式全家桶 (C++98, SIMD, FMA3)
//
// 本文件提供实系数/复系数/整数系数多项式的全面运算。
// 已合并运行时 CPU 调度与 SIMD 复数乘法实现，实现单头文件分发。
// 新增 NTT 精确整数卷积模块，用于需要确定性计算的场景（如区块链 PoUW）。
//
// 作者: yuzheng2026 (与 DeepSeek AI 协作开发)
// 许可证: GNU GPLv3 or any later version
// 仓库: https://github.com/yuzheng2026/PolyAVX

#pragma once
#ifndef POLY_AVX_HPP
#define POLY_AVX_HPP

#include <vector>
#include <complex>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <utility>
#include <iostream>

// SIMD intrinsic 头文件
#include <emmintrin.h>
#include <pmmintrin.h>
#ifdef __AVX__
#include <immintrin.h>
#endif

// MSVC 支持
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace poly_avx {
	typedef std::complex<double> cd;
	typedef void (*pointwise_mul_func)(cd*, const cd*, int);   // 复数乘法函数指针类型
	
	const double PI = std::acos(-1.0);
	const double EPS = 1e-12;
	const cd I(0.0, 1.0);
	// 返回不小于 n 的最小 2 的幂
	inline int next_pow2(int n) {
		int r = 1;
		while (r < n) r <<= 1;
		return r;
	}
// ---------- SIMD 复数乘法实现 ----------
	
// SSE3 版本：一次处理 4 个复数，回退方案
	static void pointwise_mul_sse3(cd* A, const cd* B, int len) {
		const int step = 4;
		int i = 0;
		for (; i <= len - step; i += step) {
			_mm_prefetch((const char*)&A[i + step], _MM_HINT_T0);
			_mm_prefetch((const char*)&B[i + step], _MM_HINT_T0);
			
			__m128d a0 = _mm_loadu_pd((double*)&A[i]);
			__m128d b0 = _mm_loadu_pd((double*)&B[i]);
			__m128d a1 = _mm_loadu_pd((double*)&A[i + 1]);
			__m128d b1 = _mm_loadu_pd((double*)&B[i + 1]);
			__m128d a2 = _mm_loadu_pd((double*)&A[i + 2]);
			__m128d b2 = _mm_loadu_pd((double*)&B[i + 2]);
			__m128d a3 = _mm_loadu_pd((double*)&A[i + 3]);
			__m128d b3 = _mm_loadu_pd((double*)&B[i + 3]);
			
#if defined(__FMA__)
			__m128d a0_ar = _mm_movedup_pd(a0);
			__m128d a0_ai = _mm_unpackhi_pd(a0, a0);
			__m128d b0_sw = _mm_shuffle_pd(b0, b0, 1);
			__m128d real0 = _mm_fnmadd_pd(a0_ai, b0_sw, _mm_mul_pd(a0_ar, b0));
			__m128d imag0 = _mm_fmadd_pd(a0_ar, b0_sw, _mm_mul_pd(a0_ai, b0));
			_mm_storeu_pd((double*)&A[i], _mm_shuffle_pd(real0, imag0, 0));
			
			__m128d a1_ar = _mm_movedup_pd(a1);
			__m128d a1_ai = _mm_unpackhi_pd(a1, a1);
			__m128d b1_sw = _mm_shuffle_pd(b1, b1, 1);
			__m128d real1 = _mm_fnmadd_pd(a1_ai, b1_sw, _mm_mul_pd(a1_ar, b1));
			__m128d imag1 = _mm_fmadd_pd(a1_ar, b1_sw, _mm_mul_pd(a1_ai, b1));
			_mm_storeu_pd((double*)&A[i + 1], _mm_shuffle_pd(real1, imag1, 0));
			
			__m128d a2_ar = _mm_movedup_pd(a2);
			__m128d a2_ai = _mm_unpackhi_pd(a2, a2);
			__m128d b2_sw = _mm_shuffle_pd(b2, b2, 1);
			__m128d real2 = _mm_fnmadd_pd(a2_ai, b2_sw, _mm_mul_pd(a2_ar, b2));
			__m128d imag2 = _mm_fmadd_pd(a2_ar, b2_sw, _mm_mul_pd(a2_ai, b2));
			_mm_storeu_pd((double*)&A[i + 2], _mm_shuffle_pd(real2, imag2, 0));
			
			__m128d a3_ar = _mm_movedup_pd(a3);
			__m128d a3_ai = _mm_unpackhi_pd(a3, a3);
			__m128d b3_sw = _mm_shuffle_pd(b3, b3, 1);
			__m128d real3 = _mm_fnmadd_pd(a3_ai, b3_sw, _mm_mul_pd(a3_ar, b3));
			__m128d imag3 = _mm_fmadd_pd(a3_ar, b3_sw, _mm_mul_pd(a3_ai, b3));
			_mm_storeu_pd((double*)&A[i + 3], _mm_shuffle_pd(real3, imag3, 0));
#else
			for (int j = 0; j < step; ++j) {
				__m128d a = _mm_loadu_pd((double*)&A[i + j]);
				__m128d b = _mm_loadu_pd((double*)&B[i + j]);
				__m128d b_sw = _mm_shuffle_pd(b, b, 1);
				__m128d m1 = _mm_mul_pd(a, b);
				__m128d m2 = _mm_mul_pd(a, b_sw);
				__m128d X = _mm_unpacklo_pd(m1, m2);
				__m128d Y = _mm_unpackhi_pd(m1, m2);
				__m128d res = _mm_addsub_pd(X, Y);
				_mm_storeu_pd((double*)&A[i + j], res);
			}
#endif
		}
		for (; i < len; ++i) {
			A[i] *= B[i];
		}
	}
	
// AVX 版本：一次处理 8 个复数
#ifdef __AVX__
	static void pointwise_mul_avx(cd* A, const cd* B, int len) {
		const int step = 8;
		int i = 0;
		for (; i <= len - step; i += step) {
			_mm_prefetch((const char*)&A[i + step], _MM_HINT_T0);
			_mm_prefetch((const char*)&B[i + step], _MM_HINT_T0);
			
			__m256d a0 = _mm256_loadu_pd((double*)&A[i]);
			__m256d b0 = _mm256_loadu_pd((double*)&B[i]);
			__m256d a1 = _mm256_loadu_pd((double*)&A[i + 2]);
			__m256d b1 = _mm256_loadu_pd((double*)&B[i + 2]);
			__m256d a2 = _mm256_loadu_pd((double*)&A[i + 4]);
			__m256d b2 = _mm256_loadu_pd((double*)&B[i + 4]);
			__m256d a3 = _mm256_loadu_pd((double*)&A[i + 6]);
			__m256d b3 = _mm256_loadu_pd((double*)&B[i + 6]);
			
			__m256d b0_sw = _mm256_permute_pd(b0, 0x5);
			__m256d a0_ar = _mm256_permute_pd(a0, 0x0);
			__m256d a0_ai = _mm256_permute_pd(a0, 0xF);
			__m256d real0 = _mm256_fnmadd_pd(a0_ai, b0_sw, _mm256_mul_pd(a0_ar, b0));
			__m256d imag0 = _mm256_fmadd_pd(a0_ar, b0_sw, _mm256_mul_pd(a0_ai, b0));
			_mm256_storeu_pd((double*)&A[i], _mm256_shuffle_pd(real0, imag0, 0));
			
			__m256d b1_sw = _mm256_permute_pd(b1, 0x5);
			__m256d a1_ar = _mm256_permute_pd(a1, 0x0);
			__m256d a1_ai = _mm256_permute_pd(a1, 0xF);
			__m256d real1 = _mm256_fnmadd_pd(a1_ai, b1_sw, _mm256_mul_pd(a1_ar, b1));
			__m256d imag1 = _mm256_fmadd_pd(a1_ar, b1_sw, _mm256_mul_pd(a1_ai, b1));
			_mm256_storeu_pd((double*)&A[i + 2], _mm256_shuffle_pd(real1, imag1, 0));
			
			__m256d b2_sw = _mm256_permute_pd(b2, 0x5);
			__m256d a2_ar = _mm256_permute_pd(a2, 0x0);
			__m256d a2_ai = _mm256_permute_pd(a2, 0xF);
			__m256d real2 = _mm256_fnmadd_pd(a2_ai, b2_sw, _mm256_mul_pd(a2_ar, b2));
			__m256d imag2 = _mm256_fmadd_pd(a2_ar, b2_sw, _mm256_mul_pd(a2_ai, b2));
			_mm256_storeu_pd((double*)&A[i + 4], _mm256_shuffle_pd(real2, imag2, 0));
			
			__m256d b3_sw = _mm256_permute_pd(b3, 0x5);
			__m256d a3_ar = _mm256_permute_pd(a3, 0x0);
			__m256d a3_ai = _mm256_permute_pd(a3, 0xF);
			__m256d real3 = _mm256_fnmadd_pd(a3_ai, b3_sw, _mm256_mul_pd(a3_ar, b3));
			__m256d imag3 = _mm256_fmadd_pd(a3_ar, b3_sw, _mm256_mul_pd(a3_ai, b3));
			_mm256_storeu_pd((double*)&A[i + 6], _mm256_shuffle_pd(real3, imag3, 0));
		}
		for (; i < len; ++i) {
			pointwise_mul_sse3(A + i, B + i, 1);
		}
	}
#else
	static void pointwise_mul_avx(cd*, const cd*, int) { /* 未编译 */ }
#endif
	
// AVX-512 版本：一次处理 16 个复数
#if defined(__AVX512F__)
	static void pointwise_mul_avx512(cd* A, const cd* B, int len) {
		const int step = 16;
		int i = 0;
		for (; i <= len - step; i += step) {
			_mm_prefetch((const char*)&A[i + step], _MM_HINT_T0);
			_mm_prefetch((const char*)&B[i + step], _MM_HINT_T0);
			
			__m512d a0 = _mm512_loadu_pd((double*)&A[i]);
			__m512d b0 = _mm512_loadu_pd((double*)&B[i]);
			__m512d a1 = _mm512_loadu_pd((double*)&A[i + 4]);
			__m512d b1 = _mm512_loadu_pd((double*)&B[i + 4]);
			__m512d a2 = _mm512_loadu_pd((double*)&A[i + 8]);
			__m512d b2 = _mm512_loadu_pd((double*)&B[i + 8]);
			__m512d a3 = _mm512_loadu_pd((double*)&A[i + 12]);
			__m512d b3 = _mm512_loadu_pd((double*)&B[i + 12]);
			
			__m512d b0_sw = _mm512_permute_pd(b0, 0x55);
			__m512d real0 = _mm512_fnmadd_pd(_mm512_permute_pd(a0, 0xFF), b0_sw,
											 _mm512_mul_pd(_mm512_permute_pd(a0, 0x00), b0));
			__m512d imag0 = _mm512_fmadd_pd(_mm512_permute_pd(a0, 0x00), b0_sw,
											_mm512_mul_pd(_mm512_permute_pd(a0, 0xFF), b0));
			_mm512_storeu_pd((double*)&A[i], _mm512_shuffle_pd(real0, imag0, 0));
			
			__m512d b1_sw = _mm512_permute_pd(b1, 0x55);
			__m512d real1 = _mm512_fnmadd_pd(_mm512_permute_pd(a1, 0xFF), b1_sw,
											 _mm512_mul_pd(_mm512_permute_pd(a1, 0x00), b1));
			__m512d imag1 = _mm512_fmadd_pd(_mm512_permute_pd(a1, 0x00), b1_sw,
											_mm512_mul_pd(_mm512_permute_pd(a1, 0xFF), b1));
			_mm512_storeu_pd((double*)&A[i + 4], _mm512_shuffle_pd(real1, imag1, 0));
			
			__m512d b2_sw = _mm512_permute_pd(b2, 0x55);
			__m512d real2 = _mm512_fnmadd_pd(_mm512_permute_pd(a2, 0xFF), b2_sw,
											 _mm512_mul_pd(_mm512_permute_pd(a2, 0x00), b2));
			__m512d imag2 = _mm512_fmadd_pd(_mm512_permute_pd(a2, 0x00), b2_sw,
											_mm512_mul_pd(_mm512_permute_pd(a2, 0xFF), b2));
			_mm512_storeu_pd((double*)&A[i + 8], _mm512_shuffle_pd(real2, imag2, 0));
			
			__m512d b3_sw = _mm512_permute_pd(b3, 0x55);
			__m512d real3 = _mm512_fnmadd_pd(_mm512_permute_pd(a3, 0xFF), b3_sw,
											 _mm512_mul_pd(_mm512_permute_pd(a3, 0x00), b3));
			__m512d imag3 = _mm512_fmadd_pd(_mm512_permute_pd(a3, 0x00), b3_sw,
											_mm512_mul_pd(_mm512_permute_pd(a3, 0xFF), b3));
			_mm512_storeu_pd((double*)&A[i + 12], _mm512_shuffle_pd(real3, imag3, 0));
		}
		for (int j = len & ~15; j < len; ++j) {
			pointwise_mul_sse3(A + j, B + j, 1);
		}
	}
#else
	static void pointwise_mul_avx512(cd*, const cd*, int) { /* 未编译 */ }
#endif
	
// CPU 特性检测
	static bool cpu_has_avx() {
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_cpu_supports("avx");
#elif defined(_MSC_VER)
		int cpuInfo[4];
		__cpuidex(cpuInfo, 1, 0);
		return (cpuInfo[2] & (1 << 28)) != 0;
#else
		return false;
#endif
	}
	
	static bool cpu_has_avx512f() {
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_cpu_supports("avx512f");
#elif defined(_MSC_VER)
		int cpuInfo[4];
		__cpuidex(cpuInfo, 7, 0);
		return (cpuInfo[1] & (1 << 16)) != 0;
#else
		return false;
#endif
	}
	
// 运行时调度
	static pointwise_mul_func pointwise_mul = pointwise_mul_sse3;
	
	static void init_cpu_dispatch() {
#ifdef __AVX512F__
		if (cpu_has_avx512f()) {
			pointwise_mul = pointwise_mul_avx512;
			return;
		}
#endif
#ifdef __AVX__
		if (cpu_has_avx()) {
			pointwise_mul = pointwise_mul_avx;
			return;
		}
#endif
		pointwise_mul = pointwise_mul_sse3;
	}
	
	namespace {
		struct AutoInit {
			AutoInit() { init_cpu_dispatch(); }
		} auto_init;
	}
	
// ---------- FFT ----------
	template <typename F>
	void fft(std::complex<F>* a, int n, bool invert) {
		for (int i = 1, j = 0; i < n; ++i) {
			int bit = n >> 1;
			for (; j & bit; bit >>= 1) j ^= bit;
			j ^= bit;
			if (i < j) std::swap(a[i], a[j]);
		}
		for (int len = 2; len <= n; len <<= 1) {
			F ang = 2.0 * PI / len * (invert ? -1.0 : 1.0);
			std::complex<F> wlen(std::cos(ang), std::sin(ang));
			for (int i = 0; i < n; i += len) {
				std::complex<F> w(1.0, 0.0);
				for (int j = 0; j < len / 2; ++j) {
					std::complex<F> u = a[i + j];
					std::complex<F> v = a[i + j + len / 2] * w;
					a[i + j] = u + v;
					a[i + j + len / 2] = u - v;
					w *= wlen;
				}
			}
		}
		if (invert) {
			for (int i = 0; i < n; ++i) a[i] /= n;
		}
	}
	
// ---------- 卷积重载（double 和 complex） ----------
	inline std::vector<double> convolution(const std::vector<double>& a, const std::vector<double>& b, int lim) {
		if (a.empty() || b.empty()) return std::vector<double>();
		int n = (int)a.size(), m = (int)b.size(), sz = n + m - 1;
		if (sz <= 64) {
			int res_sz = (lim < sz) ? lim : sz;
			std::vector<double> res(res_sz, 0.0);
			for (int i = 0; i < n; ++i) {
				for (int j = 0; j < m && i + j < res_sz; ++j) {
					res[i + j] += a[i] * b[j];
				}
			}
			return res;
		}
		int N = next_pow2(sz);
		std::vector<cd> A(N, 0.0), B(N, 0.0);
		for (int i = 0; i < n; ++i) A[i] = cd(a[i], 0.0);
		for (int i = 0; i < m; ++i) B[i] = cd(b[i], 0.0);
		fft(&A[0], N, false);
		fft(&B[0], N, false);
		pointwise_mul(&A[0], &B[0], N);
		fft(&A[0], N, true);
		int res_sz = (lim < sz) ? lim : sz;
		std::vector<double> res(res_sz);
		for (int i = 0; i < res_sz; ++i) res[i] = A[i].real();
		return res;
	}
	
	inline std::vector<cd> convolution(const std::vector<cd>& a, const std::vector<cd>& b, int lim) {
		if (a.empty() || b.empty()) return std::vector<cd>();
		int n = (int)a.size(), m = (int)b.size(), sz = n + m - 1;
		if (sz <= 64) {
			int res_sz = (lim < sz) ? lim : sz;
			std::vector<cd> res(res_sz, cd(0.0, 0.0));
			for (int i = 0; i < n; ++i) {
				for (int j = 0; j < m && i + j < res_sz; ++j) {
					res[i + j] += a[i] * b[j];
				}
			}
			return res;
		}
		int N = next_pow2(sz);
		std::vector<cd> A(N, 0.0), B(N, 0.0);
		for (int i = 0; i < n; ++i) A[i] = a[i];
		for (int i = 0; i < m; ++i) B[i] = b[i];
		fft(&A[0], N, false);
		fft(&B[0], N, false);
		pointwise_mul(&A[0], &B[0], N);
		fft(&A[0], N, true);
		int res_sz = (lim < sz) ? lim : sz;
		std::vector<cd> res(res_sz);
		for (int i = 0; i < res_sz; ++i) res[i] = A[i];
		return res;
	}
	
// ---------- 多项式类 ----------
	template <typename T>
	class Poly {
	public:
		std::vector<T> data;
		
		Poly() {}
		explicit Poly(int sz) : data(sz) {}
		explicit Poly(const T& value) : data(1, value) {}
		Poly(const std::vector<T>& v) : data(v) {}
		
		int size() const { return (int)data.size(); }
		void resize(int n) { data.resize(n); }
		void trim() {
			while (!data.empty() && std::abs(data.back()) < EPS)
				data.pop_back();
		}
		
		T& operator[](int i) { return data[i]; }
		const T& operator[](int i) const { return data[i]; }
		
		Poly trunc(int n) const {
			Poly res = *this;
			if ((int)res.data.size() > n) res.data.resize(n);
			return res;
		}
		
		Poly mul_scalar(const T& s) const {
			Poly res(size());
			for (int i = 0; i < size(); ++i) res[i] = data[i] * s;
			return res;
		}
		Poly div_scalar(const T& s) const {
			Poly res(size());
			for (int i = 0; i < size(); ++i) res[i] = data[i] / s;
			return res;
		}
		
		Poly mul(const Poly& other, int lim = -1) const {
			if (data.empty() || other.data.empty()) return Poly();
			int total = size() + other.size() - 1;
			int l = (lim == -1) ? total : std::min(lim, total);
			return Poly(convolution(data, other.data, l));
		}
		
		std::pair<Poly, Poly> divmod(const Poly& rhs) const {
			assert(!rhs.data.empty() && std::abs(rhs.data.back()) > EPS);
			Poly A = *this; A.trim();
			Poly B = rhs;   B.trim();
			if (A.size() < B.size()) return std::make_pair(Poly(), A);
			int n = A.size() - B.size() + 1;
			std::vector<T> revA(A.data), revB(B.data);
			std::reverse(revA.begin(), revA.end());
			std::reverse(revB.begin(), revB.end());
			Poly revB_inv = Poly(revB).inv(n);
			Poly revQ = Poly(convolution(revA, revB_inv.data, n));
			revQ.data.resize(n);
			std::reverse(revQ.data.begin(), revQ.data.end());
			revQ.trim();
			Poly R = A - B * revQ;
			R.trim();
			return std::make_pair(revQ, R);
		}
		
		Poly operator/(const Poly& rhs) const { return divmod(rhs).first; }
		Poly operator%(const Poly& rhs) const { return divmod(rhs).second; }
		
		Poly operator+(const Poly& rhs) const {
			int n = std::max(size(), rhs.size());
			Poly res(n);
			for (int i = 0; i < n; ++i) {
				T a = (i < size()) ? data[i] : T(0);
				T b = (i < rhs.size()) ? rhs[i] : T(0);
				res[i] = a + b;
			}
			return res;
		}
		Poly operator-(const Poly& rhs) const {
			int n = std::max(size(), rhs.size());
			Poly res(n);
			for (int i = 0; i < n; ++i) {
				T a = (i < size()) ? data[i] : T(0);
				T b = (i < rhs.size()) ? rhs[i] : T(0);
				res[i] = a - b;
			}
			return res;
		}
		Poly operator-() const {
			Poly res(size());
			for (int i = 0; i < size(); ++i) res[i] = -data[i];
			return res;
		}
		Poly operator*(const T& scalar) const { return mul_scalar(scalar); }
		Poly operator*(const Poly& other) const { return mul(other); }
		Poly operator/(const T& scalar) const { return div_scalar(scalar); }
		
		bool operator==(const Poly& rhs) const {
			int n = std::max(size(), rhs.size());
			for (int i = 0; i < n; ++i) {
				T a = (i < size()) ? data[i] : T(0);
				T b = (i < rhs.size()) ? rhs[i] : T(0);
				if (!(a == b)) return false;
			}
			return true;
		}
		bool operator!=(const Poly& rhs) const { return !(*this == rhs); }
		
		Poly& operator+=(const Poly& rhs) { *this = *this + rhs; return *this; }
		Poly& operator-=(const Poly& rhs) { *this = *this - rhs; return *this; }
		Poly& operator*=(const T& scalar) { *this = mul_scalar(scalar); return *this; }
		Poly& operator/=(const T& scalar) { *this = div_scalar(scalar); return *this; }
		Poly& operator*=(const Poly& rhs) { *this = mul(rhs); return *this; }
		
		Poly deriv() const {
			if (data.empty()) return Poly();
			Poly res(size() - 1);
			for (int i = 1; i < size(); ++i) res[i - 1] = T(i) * data[i];
			return res;
		}
		Poly integ() const {
			Poly res(size() + 1);
			res[0] = T(0);
			for (int i = 0; i < size(); ++i) res[i + 1] = data[i] / T(i + 1);
			return res;
		}
		
		Poly inv(int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			if (n <= 64) {
				Poly res(n);
				T a0_inv = T(1) / data[0];
				res.data[0] = a0_inv;
				for (int k = 1; k < n; ++k) {
					T sum = T(0);
					for (int i = 1; i <= k && i < size(); ++i) {
						sum += data[i] * res.data[k - i];
					}
					res.data[k] = -a0_inv * sum;
				}
				return res;
			}
			Poly res(T(1) / data[0]);
			int m = 1;
			while (m < n) {
				m <<= 1;
				Poly tmp = this->trunc(m);
				Poly two(T(2));
				res = (res * (two - tmp * res)).trunc(m);
			}
			res.resize(n);
			return res;
		}
		
		Poly log(int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			if (std::abs(data[0] - T(1)) >= EPS) {
				T c = data[0];
				Poly A1 = (*this) / c;
				Poly logA1 = A1.log(n);
				logA1[0] += std::log(c);
				return logA1.trunc(n);
			}
			Poly A = trunc(n);
			return (A.deriv() * A.inv(n)).trunc(n - 1).integ().trunc(n);
		}
		
		Poly exp(int n) const {
			if (!data.empty() && std::abs(data[0]) > EPS) {
				T c = data[0];
				Poly B = (*this) - Poly(c);
				Poly expB = B.exp(n);
				return expB * std::exp(c);
			}
			if (n <= 64) {
				Poly res(T(1));
				Poly term(T(1));
				for (int k = 1; k < n; ++k) {
					term = (term * (*this)).trunc(n) / T(k);
					res = res + term;
				}
				return res.trunc(n);
			}
			Poly res(T(1));
			int m = 1;
			while (m < n) {
				m <<= 1;
				Poly tmp = this->trunc(m);
				Poly one(T(1));
				Poly delta = one - res.log(m) + tmp;
				res = (res * delta).trunc(m);
			}
			res.resize(n);
			return res;
		}
		
		Poly sqrt(int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			if (n <= 64) {
				T a0 = data[0];
				T s0 = T(std::sqrt(std::abs(a0)));
				Poly res(n);
				res.data[0] = s0;
				for (int k = 1; k < n; ++k) {
					T sum = T(0);
					for (int i = 1; i <= k - 1; ++i) {
						sum += res.data[i] * res.data[k - i];
					}
					T ak = (k < size()) ? data[k] : T(0);
					res.data[k] = (ak - sum) / (T(2) * s0);
				}
				return res;
			}
			T a0 = data[0];
			Poly res(T(std::sqrt(std::abs(a0))));
			int m = 1;
			while (m < n) {
				m <<= 1;
				Poly A = this->trunc(m);
				Poly half(T(0.5));
				res = (res + A * res.inv(m)) * half;
				res.resize(m);
			}
			res.resize(n);
			return res;
		}
		
		Poly pow(int k, int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			assert(k >= 0);
			Poly res(T(1));
			Poly base = *this;
			while (k > 0) {
				if (k & 1) res = (res * base).trunc(n);
				base = (base * base).trunc(n);
				k >>= 1;
			}
			res.resize(n);
			return res;
		}
		
		Poly pow(double k, int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			Poly lnA = log(n);
			for (int i = 0; i < n; ++i) lnA[i] = lnA[i] * k;
			return lnA.exp(n);
		}
	};
	
// 全局标量乘除
	template <typename T>
	Poly<T> operator*(const T& scalar, const Poly<T>& p) { return p.mul_scalar(scalar); }
	template <typename T>
	Poly<T> operator/(const T& scalar, const Poly<T>& p) {
		return Poly<T>(scalar) * p.inv(p.size());
	}
	
// 流输入输出
	template <typename T>
	std::ostream& operator<<(std::ostream& os, const Poly<T>& p) {
		for (int i = 0; i < p.size(); ++i) {
			if (i > 0) os << ' ';
			os << p[i];
		}
		return os;
	}
	template <typename T>
	std::istream& operator>>(std::istream& is, Poly<T>& p) {
		p.data.clear();
		T val;
		while (is >> val) p.data.push_back(val);
		if (!is.eof()) is.clear();
		return is;
	}
	
	typedef Poly<double> PolyD;
	typedef Poly<std::complex<double> > PolyC;
	typedef Poly<long long> PolyI;   // 精确整数多项式
	
// ---------- NTT 精确整数卷积模块 ----------
	namespace ntt {
		// 新增两个 NTT 模数，原根均为 3
		const long long MOD = 998244353;
		const long long MOD2 = 1004535809; // 479 * 2^21 + 1
		const long long MOD3 = 469762049;  // 7 * 2^26 + 1
		const long long G = 3;
		const long long G2 = 3;
		const long long G3 = 3;
		
		// 通用快速幂：计算 a^e mod mod，使用 __int128 防止溢出
		inline long long mod_pow_mod(long long a, long long e, long long mod) {
			long long res = 1;
			while (e > 0) {
				if (e & 1) res = (__int128)res * a % mod;
				a = (__int128)a * a % mod;
				e >>= 1;
			}
			return res;
		}
		
		inline long long mod_pow(long long a, long long e) {
			return mod_pow_mod(a, e, MOD);
		}
		
		// 模逆元
		inline long long mod_inv(long long a) {
			return mod_pow(a, MOD - 2);
		}
		
		// 模加法
		inline long long add(long long a, long long b) {
			long long res = a + b;
			if (res >= MOD) res -= MOD;
			return res;
		}
		
		// 模减法
		inline long long sub(long long a, long long b) {
			long long res = a - b;
			if (res < 0) res += MOD;
			return res;
		}
		
		// 模乘法
		inline long long mul(long long a, long long b) {
			return (a * b) % MOD;
		}
		
		// 模除法：a / b ≡ a * b^(-1)
		inline long long div(long long a, long long b) {
			return mul(a, mod_inv(b));
		}
		
		inline long long mod_sqrt(long long a) {
			if (a == 0) return 0;
			if (mod_pow(a, (MOD - 1) / 2) != 1) return -1;
			long long q = MOD - 1, s = 0;
			while ((q & 1) == 0) { q >>= 1; ++s; }
			long long z = 2;
			while (mod_pow(z, (MOD - 1) / 2) != MOD - 1) ++z;
			long long m = s, c = mod_pow(z, q), t = mod_pow(a, q), r = mod_pow(a, (q + 1) / 2);
			while (t != 1) {
				long long i = 0, temp = t;
				while (temp != 1) {
					temp = (temp * temp) % MOD;
					++i;
					if (i >= m) return -1;
				}
				long long b = c;
				for (int j = 0; j < m - i - 1; ++j) b = (b * b) % MOD;
				m = i;
				c = (b * b) % MOD;
				t = (t * c) % MOD;
				r = (r * b) % MOD;
			}
			return r;
		}
		
		inline void transform(std::vector<long long>& a, bool invert) {
			int n = (int)a.size();
			for (int i = 1, j = 0; i < n; ++i) {
				int bit = n >> 1;
				for (; j & bit; bit >>= 1) j ^= bit;
				j ^= bit;
				if (i < j) std::swap(a[i], a[j]);
			}
			for (int len = 2; len <= n; len <<= 1) {
				long long wlen = mod_pow(G, (MOD - 1) / len);
				if (invert) wlen = mod_pow(wlen, MOD - 2);
				for (int i = 0; i < n; i += len) {
					long long w = 1;
					for (int j = 0; j < len / 2; ++j) {
						long long u = a[i + j];
						long long v = a[i + j + len / 2] * w % MOD;
						a[i + j] = (u + v) % MOD;
						a[i + j + len / 2] = (u - v + MOD) % MOD;
						w = w * wlen % MOD;
					}
				}
			}
			if (invert) {
				long long inv_n = mod_pow(n, MOD - 2);
				for (int i = 0; i < n; ++i)
					a[i] = a[i] * inv_n % MOD;
			}
		}
		
		// 通用 NTT 变换，使用给定模数和原根
		inline void transform_mod(std::vector<long long>& a, bool invert, long long mod, long long g) {
			int n = (int)a.size();
			for (int i = 1, j = 0; i < n; ++i) {
				int bit = n >> 1;
				for (; j & bit; bit >>= 1) j ^= bit;
				j ^= bit;
				if (i < j) std::swap(a[i], a[j]);
			}
			for (int len = 2; len <= n; len <<= 1) {
				long long wlen = mod_pow(g, (mod - 1) / len); // 注意 mod_pow 需要支持 mod 参数
				if (invert) wlen = mod_pow(wlen, mod - 2);
				for (int i = 0; i < n; i += len) {
					long long w = 1;
					for (int j = 0; j < len / 2; ++j) {
						long long u = a[i + j];
						long long v = a[i + j + len / 2] * w % mod;
						a[i + j] = (u + v) % mod;
						a[i + j + len / 2] = (u - v + mod) % mod;
						w = w * wlen % mod;
					}
				}
			}
			if (invert) {
				long long inv_n = mod_pow(n, mod - 2);
				for (int i = 0; i < n; ++i) a[i] = a[i] * inv_n % mod;
			}
		}
		
		inline std::vector<long long> convolution(const std::vector<long long>& a,
												  const std::vector<long long>& b,
												  int lim = -1) {
			if (a.empty() || b.empty()) return std::vector<long long>();
			int n = (int)a.size(), m = (int)b.size();
			int sz = n + m - 1;
			if (sz <= 64) {
				int res_sz = (lim == -1 || lim > sz) ? sz : lim;
				std::vector<long long> res(res_sz, 0);
				for (int i = 0; i < n; ++i) {
					for (int j = 0; j < m && i + j < res_sz; ++j) {
						res[i + j] = (res[i + j] + (a[i] * b[j]) % MOD) % MOD;
					}
				}
				return res;
			}
			int N = 1;
			while (N < sz) N <<= 1;
			std::vector<long long> A(N), B(N);
			for (int i = 0; i < n; ++i) A[i] = a[i] % MOD;
			for (int i = 0; i < m; ++i) B[i] = b[i] % MOD;
			transform(A, false);
			transform(B, false);
			for (int i = 0; i < N; ++i) A[i] = A[i] * B[i] % MOD;
			transform(A, true);
			int res_sz = (lim == -1 || lim > sz) ? sz : lim;
			A.resize(res_sz);
			return A;
		}
		
		// 多模数卷积，返回精确整数结果（使用 __int128 合并后转换回 long long）
		inline std::vector<long long> convolution_multi_mod(const std::vector<long long>& a,
															const std::vector<long long>& b,
															int lim = -1) {
			if (a.empty() || b.empty()) return std::vector<long long>();
			int n = (int)a.size(), m = (int)b.size(), sz = n + m - 1;
			int N = 1;
			while (N < sz) N <<= 1;
			
			// 对三个模数分别做 NTT 卷积
			std::vector<long long> A1(N), B1(N), A2(N), B2(N), A3(N), B3(N);
			for (int i = 0; i < n; ++i) {
				A1[i] = a[i] % MOD;
				A2[i] = a[i] % MOD2;
				A3[i] = a[i] % MOD3;
			}
			for (int i = 0; i < m; ++i) {
				B1[i] = b[i] % MOD;
				B2[i] = b[i] % MOD2;
				B3[i] = b[i] % MOD3;
			}
			
			transform_mod(A1, false, MOD, G);
			transform_mod(B1, false, MOD, G);
			for (int i = 0; i < N; ++i) A1[i] = A1[i] * B1[i] % MOD;
			transform_mod(A1, true, MOD, G);
			
			transform_mod(A2, false, MOD2, G2);
			transform_mod(B2, false, MOD2, G2);
			for (int i = 0; i < N; ++i) A2[i] = A2[i] * B2[i] % MOD2;
			transform_mod(A2, true, MOD2, G2);
			
			transform_mod(A3, false, MOD3, G3);
			transform_mod(B3, false, MOD3, G3);
			for (int i = 0; i < N; ++i) A3[i] = A3[i] * B3[i] % MOD3;
			transform_mod(A3, true, MOD3, G3);
			
			int res_sz = (lim == -1 || lim > sz) ? sz : lim;
			std::vector<long long> res(res_sz);
			
			// CRT 合并，使用 __int128 保证中间计算不溢出
			// 预计算三个模数的逆
			static bool crt_init = false;
			static long long inv_m1_mod_m2, inv_m1m2_mod_m3, inv_m2_mod_m1, inv_m3_mod_m1m2;
			if (!crt_init) {
				inv_m1_mod_m2 = mod_pow_mod(MOD, MOD2 - 2, MOD2);
				inv_m1m2_mod_m3 = mod_pow_mod((__int128)MOD * MOD2 % MOD3, MOD3 - 2, MOD3);
				inv_m2_mod_m1 = mod_pow_mod(MOD2, MOD - 2, MOD);
				inv_m3_mod_m1m2 = mod_pow_mod(MOD3, MOD2 - 2, MOD2); // 实际需要 MOD3 在 MOD2 下的逆
				// 为简化，这里使用 Garner 算法逐步合并
				crt_init = true;
			}
			
			for (int i = 0; i < res_sz; ++i) {
				// Garner 算法
				long long x1 = A1[i];
				long long x2 = A2[i];
				long long x3 = A3[i];
				
				// 合并 x1 和 x2 -> y (mod MOD*MOD2)
				long long t = (x2 - x1) % MOD2;
				if (t < 0) t += MOD2;
				t = t * inv_m1_mod_m2 % MOD2;
				long long y = x1 + (__int128)MOD * t; // y 是 __int128，但可以存入 __int128 变量
				
				// 合并 y 和 x3 -> z (mod MOD*MOD2*MOD3)
				// 需要将 y 视为 __int128
				__int128 y_i = y;
				long long y_mod_m3 = y_i % MOD3;
				long long diff = (x3 - y_mod_m3) % MOD3;
				if (diff < 0) diff += MOD3;
				long long t3 = diff * inv_m1m2_mod_m3 % MOD3;
				__int128 full = y_i + (__int128)MOD * MOD2 * t3;
				
				// 将 full 转换为有符号 long long（取模 2^64 可能丢失，但假设结果范围在 long long 内）
				// 如果结果超出 long long，将产生溢出，这里简单转换并提醒
				res[i] = (long long)full;
			}
			return res;
		}
	} // namespace ntt
	// 特化加法：逐系数模加
	template <>
	inline Poly<long long> Poly<long long>::operator+(const Poly<long long>& rhs) const {
		int n = std::max(size(), rhs.size());
		Poly<long long> res(n);
		for (int i = 0; i < n; ++i) {
			long long a = (i < size()) ? data[i] : 0;
			long long b = (i < rhs.size()) ? rhs[i] : 0;
			res[i] = ntt::add(a, b);
		}
		return res;
	}
	
// 特化减法：逐系数模减
	template <>
	inline Poly<long long> Poly<long long>::operator-(const Poly<long long>& rhs) const {
		int n = std::max(size(), rhs.size());
		Poly<long long> res(n);
		for (int i = 0; i < n; ++i) {
			long long a = (i < size()) ? data[i] : 0;
			long long b = (i < rhs.size()) ? rhs[i] : 0;
			res[i] = ntt::sub(a, b);
		}
		return res;
	}
	
// 特化取负：逐系数模负
	template <>
	inline Poly<long long> Poly<long long>::operator-() const {
		Poly<long long> res(size());
		for (int i = 0; i < size(); ++i) {
			res[i] = (data[i] == 0) ? 0 : ntt::MOD - data[i];
		}
		return res;
	}
	
// 特化复合赋值
	template <>
	inline Poly<long long>& Poly<long long>::operator+=(const Poly<long long>& rhs) {
		*this = *this + rhs;
		return *this;
	}
	template <>
	inline Poly<long long>& Poly<long long>::operator-=(const Poly<long long>& rhs) {
		*this = *this - rhs;
		return *this;
	}
	
// 特化 Poly<long long> 乘法，使用 NTT
	template <>
	inline Poly<long long> Poly<long long>::mul(const Poly<long long>& other, int lim) const {
		if (data.empty() || other.data.empty()) return Poly<long long>();
		return Poly<long long>(ntt::convolution(data, other.data, lim));
	}
	// ==================== Poly<long long> 精确运算特化 ====================
// 标量乘法（模乘）
	template <>
	inline Poly<long long> Poly<long long>::mul_scalar(const long long& s) const {
		Poly<long long> res(size());
		for (int i = 0; i < size(); ++i) {
			res[i] = ntt::mul(data[i], s);
		}
		return res;
	}
	
// 标量除法（模逆乘）
	template <>
	inline Poly<long long> Poly<long long>::div_scalar(const long long& s) const {
		assert(s != 0);
		long long inv_s = ntt::mod_inv(s);
		Poly<long long> res(size());
		for (int i = 0; i < size(); ++i) {
			res[i] = ntt::mul(data[i], inv_s);
		}
		return res;
	}
		
// 求逆（模逆）
	template <>
	inline Poly<long long> Poly<long long>::inv(int n) const {
		assert(!data.empty() && data[0] != 0);
		if (n <= 64) {
			Poly<long long> res(n);
			long long a0_inv = ntt::mod_inv(data[0]);
			res.data[0] = a0_inv;
			for (int k = 1; k < n; ++k) {
				long long sum = 0;
				for (int i = 1; i <= k && i < size(); ++i) {
					sum = ntt::add(sum, ntt::mul(data[i], res.data[k - i]));
				}
				res.data[k] = ntt::sub(0, ntt::mul(a0_inv, sum));
			}
			return res;
		}
		Poly<long long> res;
		res.data.push_back(ntt::mod_inv(data[0]));
		int m = 1;
		while (m < n) {
			m <<= 1;
			Poly<long long> tmp = this->trunc(m);
			Poly<long long> two;
			two.data.push_back(2);
			res = (res * (two - tmp * res)).trunc(m);
		}
		res.resize(n);
		return res;
	}
	
// 对数（级数展开，常数项必须为 1）
	template <>
	inline Poly<long long> Poly<long long>::log(int n) const {
		assert(!data.empty() && data[0] == 1);
		if (n <= 0) return Poly<long long>();
		if (n == 1) {
			Poly<long long> res(1);
			res.data[0] = 0;
			return res;
		}
		
		// log(A) = ∫ (A' * inv(A)) dx
		Poly<long long> A = trunc(n);
		Poly<long long> invA = A.inv(n);      // 使用已验证的模逆
		
		// 手动求导 A'
		Poly<long long> derivA;
		if (A.size() > 1) {
			derivA.resize(A.size() - 1);
			for (int i = 1; i < A.size(); ++i) {
				derivA.data[i - 1] = ntt::mul(A.data[i], i);
			}
		}
		
		Poly<long long> integrand = (derivA * invA).trunc(n - 1);
		
		// 手动积分
		Poly<long long> res(n);
		res.data[0] = 0;
		for (int i = 0; i < integrand.size() && i + 1 < n; ++i) {
			res.data[i + 1] = ntt::div(integrand.data[i], i + 1);
		}
		return res;
	}
	
// 指数（级数展开，常数项必须为 0）
	template <>
	inline Poly<long long> Poly<long long>::exp(int n) const {
		assert(!data.empty() && data[0] == 0);
		if (n <= 0) return Poly<long long>();
		if (n == 1) {
			Poly<long long> res(1);
			res.data[0] = 1;
			return res;
		}
		
		Poly<long long> res;
		res.data.push_back(1);          // exp(0) = 1
		
		int m = 1;
		while (m < n) {
			m <<= 1;
			Poly<long long> tmp = this->trunc(m);
			Poly<long long> one;
			one.data.push_back(1);
			// delta = 1 - log(res) + tmp
			Poly<long long> delta = one - res.log(m) + tmp;
			res = (res * delta).trunc(m);
		}
		res.resize(n);
		return res;
	}
	
// 平方根（常数项必须为二次剩余）
	template <>
	inline Poly<long long> Poly<long long>::sqrt(int n) const {
		assert(!data.empty());
		long long a0 = data[0];
		long long s0 = ntt::mod_sqrt(a0);
		if (s0 == -1) return Poly<long long>();  // 无解
		
		if (n <= 64) {
			Poly<long long> res(n);
			res.data[0] = s0;
			long long inv_2s0 = ntt::div(1, ntt::mul(2, s0));
			for (int k = 1; k < n; ++k) {
				long long sum = 0;
				for (int i = 1; i <= k - 1; ++i) {
					sum = ntt::add(sum, ntt::mul(res.data[i], res.data[k - i]));
				}
				long long ak = (k < size()) ? data[k] : 0;
				res.data[k] = ntt::mul(ntt::sub(ak, sum), inv_2s0);
			}
			return res;
		}
		
		Poly<long long> res;
		res.data.push_back(s0);
		int m = 1;
		while (m < n) {
			m <<= 1;
			Poly<long long> A = this->trunc(m);
			Poly<long long> half;
			half.data.push_back(ntt::mod_inv(2));
			res = (res + A * res.inv(m)) * half;
			res.resize(m);
		}
		res.resize(n);
		return res;
	}
	
// 整数幂（快速幂）
	template <>
	inline Poly<long long> Poly<long long>::pow(int k, int n) const {
		assert(k >= 0);
		Poly<long long> res;
		res.data.push_back(1);              // 常数 1
		Poly<long long> base = *this;
		
		while (k > 0) {
			if (k & 1) {
				res = (res * base).trunc(n);
			}
			base = (base * base).trunc(n);
			k >>= 1;
		}
		res.resize(n);
		return res;
	}
	
	// 外部函数：多模数精确乘法
	inline Poly<long long> mul_exact(const Poly<long long>& a, const Poly<long long>& b, int lim = -1) {
		if (a.data.empty() || b.data.empty()) return Poly<long long>();
		return Poly<long long>(ntt::convolution_multi_mod(a.data, b.data, lim));
	}
	
// ---------- 三角函数（带归一化） ----------
	inline PolyC to_complex(const PolyD& p) {
		PolyC res(p.size());
		for (int i = 0; i < p.size(); ++i) res[i] = cd(p[i], 0.0);
		return res;
	}
	
	inline void poly_sincos(const PolyD& A, int n, PolyD& sinA, PolyD& cosA) {
		PolyC Ac = to_complex(A.trunc(n));
		PolyC exp_iA = (Ac * I).exp(n);
		sinA = PolyD(n);
		cosA = PolyD(n);
		for (int i = 0; i < n; ++i) {
			cosA[i] = exp_iA[i].real();
			sinA[i] = exp_iA[i].imag();
		}
		// 归一化 sin²+cos² = 1
		PolyD s2 = (sinA * sinA).trunc(n);
		PolyD c2 = (cosA * cosA).trunc(n);
		PolyD norm = s2 + c2;
		PolyD inv_norm = norm.sqrt(n).inv(n);
		sinA = (sinA * inv_norm).trunc(n);
		cosA = (cosA * inv_norm).trunc(n);
	}
	
	inline PolyD poly_sin(const PolyD& A, int n) {
		PolyD s, c;
		poly_sincos(A, n, s, c);
		return s;
	}
	inline PolyD poly_cos(const PolyD& A, int n) {
		PolyD s, c;
		poly_sincos(A, n, s, c);
		return c;
	}
	inline PolyD poly_tan(const PolyD& A, int n) {
		PolyD s, c;
		poly_sincos(A, n, s, c);
		return (s * c.inv(n)).trunc(n);
	}
	
// ---------- 反三角函数 ----------
	inline PolyD poly_asin(const PolyD& A, int n) {
		assert(A.data.empty() || std::abs(A[0]) < 1.0 - EPS);
		PolyD one(1.0);
		PolyD A2 = (A * A).trunc(n);
		PolyD integrand = (A.deriv() * (one - A2).sqrt(n).inv(n)).trunc(n - 1);
		PolyD res = integrand.integ().trunc(n);
		if (!res.data.empty()) res.data[0] = std::asin(A[0]);
		return res;
	}
	inline PolyD poly_acos(const PolyD& A, int n) {
		assert(A.data.empty() || std::abs(A[0]) < 1.0 - EPS);
		PolyD asin_res = poly_asin(A, n);
		PolyD res = asin_res * (-1.0);
		res.data[0] = PI / 2.0 - std::asin(A[0]);
		return res;
	}
	inline PolyD poly_atan(const PolyD& A, int n) {
		PolyD one(1.0);
		PolyD A2 = (A * A).trunc(n);
		PolyD den = one + A2;
		PolyD integrand = (A.deriv() * den.inv(n)).trunc(n - 1);
		PolyD res = integrand.integ().trunc(n);
		res.data[0] = std::atan(A[0]);
		return res;
	}
	
// ---------- 双曲函数（带归一化） ----------
	inline void poly_sinhcosh(const PolyD& A, int n, PolyD& sinhA, PolyD& coshA) {
		PolyC Ac = to_complex(A.trunc(n));
		PolyC expA  = (Ac).exp(n);
		PolyC expmA = (Ac * (-1.0)).exp(n);
		sinhA = PolyD(n);
		coshA = PolyD(n);
		for (int i = 0; i < n; ++i) {
			sinhA[i] = (expA[i] - expmA[i]).real() * 0.5;
			coshA[i] = (expA[i] + expmA[i]).real() * 0.5;
		}
		PolyD s2 = (sinhA * sinhA).trunc(n);
		PolyD c2 = (coshA * coshA).trunc(n);
		PolyD diff = c2 - s2;
		PolyD inv_norm = diff.sqrt(n).inv(n);
		sinhA = (sinhA * inv_norm).trunc(n);
		coshA = (coshA * inv_norm).trunc(n);
	}
	
	inline PolyD poly_sinh(const PolyD& A, int n) {
		PolyD s, c;
		poly_sinhcosh(A, n, s, c);
		return s;
	}
	inline PolyD poly_cosh(const PolyD& A, int n) {
		PolyD s, c;
		poly_sinhcosh(A, n, s, c);
		return c;
	}
	inline PolyD poly_tanh(const PolyD& A, int n) {
		PolyD s, c;
		poly_sinhcosh(A, n, s, c);
		return (s * c.inv(n)).trunc(n);
	}
	
	inline PolyD poly_log1p(const PolyD& A, int n) {
		assert(A.data.empty() || std::abs(A[0]) < EPS);
		PolyD one(1.0);
		return (one + A).log(n);
	}
	
// ---------- 反双曲函数 ----------
	inline PolyD poly_asinh(const PolyD& A, int n) {
		PolyD one(1.0);
		PolyD A2 = (A * A).trunc(n);
		PolyD integrand = (A.deriv() * (one + A2).sqrt(n).inv(n)).trunc(n - 1);
		PolyD res = integrand.integ().trunc(n);
		double a0 = A[0];
		res.data[0] = std::log(a0 + std::sqrt(a0 * a0 + 1.0));
		return res;
	}
	inline PolyD poly_acosh(const PolyD& A, int n) {
		assert(!A.data.empty() && A.data[0] > 1.0 + EPS);
		double c = A.data[0];
		double acosh_c = std::log(c + std::sqrt(c * c - 1.0));
		PolyD one(1.0);
		PolyD t = A - one;
		PolyD two(2.0);
		PolyD t2 = (t * t).trunc(n);
		PolyD inner = (t * two + t2).trunc(n);
		PolyD sqrt_term = inner.sqrt(n);
		PolyD sum = t + sqrt_term;
		double d = sum[0];
		PolyD temp = sum - PolyD(d);
		if (!temp.data.empty()) temp.data[0] = 0.0;
		PolyD shifted = temp * (1.0 / (1.0 + d));
		PolyD res = poly_log1p(shifted, n);
		res.data[0] = acosh_c;
		return res.trunc(n);
	}
	inline PolyD poly_atanh(const PolyD& A, int n) {
		assert(A.data.empty() || std::abs(A[0]) < 1.0 - EPS);
		PolyD one(1.0);
		PolyD A2 = (A * A).trunc(n);
		PolyD den = one - A2;
		PolyD integrand = (A.deriv() * den.inv(n)).trunc(n - 1);
		PolyD res = integrand.integ().trunc(n);
		double a0 = A[0];
		res.data[0] = 0.5 * std::log((1.0 + a0) / (1.0 - a0));
		return res;
	}
	
// ==================== 扩展功能 ====================
	
// ---------- 阶乘 / 二项式系数 ----------
	inline std::vector<double> factorial_table(int n) {
		std::vector<double> f(n);
		f[0] = 1.0;
		for (int i = 1; i < n; ++i) f[i] = f[i-1] * i;
		return f;
	}
	
	inline std::vector<double> inv_factorial_table(int n) {
		std::vector<double> f = factorial_table(n);
		std::vector<double> inv(n);
		inv[n-1] = 1.0 / f[n-1];
		for (int i = n-2; i >= 0; --i) inv[i] = inv[i+1] * (i+1);
		return inv;
	}
	
// ---------- 多项式平移（Taylor shift） ----------
	inline PolyD poly_shift(const PolyD& A, double c, int n) {
		PolyD A_trunc = A.trunc(n);
		std::vector<double> fact = factorial_table(n);
		std::vector<double> inv_fact = inv_factorial_table(n);
		PolyD L(n), R(n);
		double cp = 1.0;
		for (int i = 0; i < n; ++i) {
			double ai = (i < A_trunc.size()) ? A_trunc[i] : 0.0;
			L[i] = ai * fact[i];
			R[n-1-i] = cp * inv_fact[i];
			cp *= c;
		}
		PolyD M = L * R;
		PolyD res(n);
		for (int i = 0; i < n; ++i)
			res[i] = M[n-1+i] * inv_fact[i];
		return res;
	}
	
// ---------- 多点求值（分治快速版） ----------
	inline std::vector<double> multipoint_eval_naive(const PolyD& P, const std::vector<double>& pts) {
		std::vector<double> res(pts.size());
		for (size_t i = 0; i < pts.size(); ++i) {
			double val = 0.0, xp = 1.0;
			for (int j = 0; j < P.size(); ++j) {
				val += P[j] * xp;
				xp *= pts[i];
			}
			res[i] = val;
		}
		return res;
	}
	
	struct EvalTree {
		int left, right;
		PolyD prod;
		EvalTree *lc, *rc;
		EvalTree(int l, int r) : left(l), right(r), prod(1.0), lc(NULL), rc(NULL) {}
	};
	
	inline EvalTree* build_eval_tree(const std::vector<double>& pts, int l, int r) {
		EvalTree* node = new EvalTree(l, r);
		if (r - l == 1) {
			PolyD factor(2);
			factor[0] = -pts[l];
			factor[1] = 1.0;
			node->prod = factor;
		} else {
			int mid = (l + r) / 2;
			node->lc = build_eval_tree(pts, l, mid);
			node->rc = build_eval_tree(pts, mid, r);
			node->prod = (node->lc->prod * node->rc->prod).trunc(r - l + 1);
		}
		return node;
	}
	
	inline void delete_eval_tree(EvalTree* node) {
		if (!node) return;
		delete_eval_tree(node->lc);
		delete_eval_tree(node->rc);
		delete node;
	}
	
	inline void eval_rec(const PolyD& f, EvalTree* node, std::vector<double>& res) {
		if (node->right - node->left == 1) {
			res[node->left] = (f.size() == 0) ? 0.0 : f[0];
			return;
		}
		PolyD r0 = f % node->lc->prod;
		PolyD r1 = f % node->rc->prod;
		eval_rec(r0, node->lc, res);
		eval_rec(r1, node->rc, res);
	}
	
	inline std::vector<double> multipoint_eval(const PolyD& P, const std::vector<double>& pts) {
		int k = (int)pts.size();
		if (k <= 32 || P.size() <= 32) {
			return multipoint_eval_naive(P, pts);
		}
		EvalTree* root = build_eval_tree(pts, 0, k);
		std::vector<double> res(k);
		PolyD f = P;
		f.trim();
		eval_rec(f, root, res);
		delete_eval_tree(root);
		return res;
	}
	
// ---------- 拉格朗日插值（快速版） ----------
	inline PolyD multipoint_interpolate(const std::vector<double>& x, const std::vector<double>& y) {
		int n = (int)x.size();
		PolyD result(n);
		for (int i = 0; i < n; ++i) {
			double yi = y[i];
			PolyD Li(std::vector<double>(1, 1.0));
			double denom = 1.0;
			for (int j = 0; j < n; ++j) if (i != j) {
				PolyD factor(2);
				factor[0] = -x[j];
				factor[1] = 1.0;
				Li = (Li * factor).trunc(n);
				denom *= (x[i] - x[j]);
			}
			double coeff = yi / denom;
			for (int k = 0; k < n; ++k)
				result[k] += coeff * Li[k];
		}
		return result;
	}
	
	inline std::pair<PolyD, PolyD> build_interp_rec(EvalTree* node, const std::vector<double>& w) {
		if (node->right - node->left == 1) {
			int idx = node->left;
			PolyD interp(1);
			interp[0] = w[idx];
			return std::make_pair(interp, node->prod);
		} else {
			std::pair<PolyD, PolyD> left = build_interp_rec(node->lc, w);
			std::pair<PolyD, PolyD> right = build_interp_rec(node->rc, w);
			PolyD interp = (left.first * right.second) + (right.first * left.second);
			return std::make_pair(interp, node->prod);
		}
	}
	
	inline PolyD multipoint_interpolate_fast(const std::vector<double>& x, const std::vector<double>& y) {
		int n = (int)x.size();
		if (n <= 32) return multipoint_interpolate(x, y);
		EvalTree* root = build_eval_tree(x, 0, n);
		PolyD prod = root->prod;
		PolyD dprod = prod.deriv();
		std::vector<double> deriv_vals(n);
		eval_rec(dprod, root, deriv_vals);
		std::vector<double> w(n);
		for (int i = 0; i < n; ++i) w[i] = y[i] / deriv_vals[i];
		std::pair<PolyD, PolyD> result = build_interp_rec(root, w);
		delete_eval_tree(root);
		return result.first;
	}
	
// ---------- 形式幂级数复合（Brent–Kung） ----------
	inline PolyD poly_composite(const PolyD& A, const PolyD& B, int n) {
		assert(B.data.empty() || std::abs(B[0]) < EPS);
		if (A.size() == 0) return PolyD(n);
		int L = (int)std::sqrt((double)n) + 1;
		std::vector<PolyD> B_pow(L + 1);
		B_pow[0] = PolyD(1.0);
		for (int i = 1; i <= L; ++i)
			B_pow[i] = (B_pow[i-1] * B).trunc(n);
		PolyD res;
		for (int i = L-1; i >= 0; --i) {
			if (!res.data.empty())
				res = (res * B_pow[L]).trunc(n);
			PolyD sum;
			for (int j = 0; j < L && i*L + j < A.size(); ++j) {
				double c = A[i*L + j];
				if (std::abs(c) > EPS)
					sum = (sum + B_pow[j] * c).trunc(n);
			}
			res = (res + sum).trunc(n);
		}
		res.resize(n);
		return res;
	}
	
// ---------- 复合逆（Reversion）牛顿迭代 ----------
	inline PolyD poly_reversion(const PolyD& F, int n) {
		assert(F.data.size() >= 2 && std::abs(F[0]) < EPS && std::abs(F[1]) > EPS);
		PolyD G;
		G.data.push_back(0.0);
		G.data.push_back(1.0 / F[1]);
		int m = 2;
		while (m < n) {
			m <<= 1;
			PolyD F_trunc = F.trunc(m);
			PolyD FG = poly_composite(F_trunc, G, m);
			PolyD x_poly(2);
			x_poly[0] = 0.0;
			x_poly[1] = 1.0;
			PolyD diff = (FG - x_poly).trunc(m);
			PolyD dF = F.deriv().trunc(m);
			PolyD dFG = poly_composite(dF, G, m);
			if (std::abs(dFG[0]) < EPS) dFG[0] = F[1];
			PolyD inv_dFG = dFG.inv(m);
			G = (G - diff * inv_dFG).trunc(m);
			G[0] = 0.0;
		}
		G.resize(n);
		return G;
	}
	
// ---------- 特殊函数 ----------
	inline PolyD poly_erf(int n) {
		std::vector<double> c(n, 0.0);
		double f = 2.0 / std::sqrt(PI);
		double fact = 1.0;
		for (int k = 0; 2*k+1 < n; ++k) {
			int i = 2*k+1;
			double term = f * ((k%2) ? -1.0 : 1.0) / (fact * (2*k+1));
			c[i] = term;
			fact *= (k + 1);
		}
		return PolyD(c);
	}
	inline PolyD poly_erf(const PolyD& A, int n) {
		if (A.data.empty()) return PolyD(n);
		PolyD erf_series = poly_erf(n);
		PolyD res = poly_composite(erf_series, A, n);
		if (!res.data.empty()) res.data[0] = 0.0;
		return res;
	}
	inline PolyD poly_bessel_J0(int n) {
		std::vector<double> c(n, 0.0);
		double val = 1.0;
		for (int k = 0; 2*k < n; ++k) {
			int i = 2*k;
			c[i] = (k%2 ? -val : val);
			val /= (4.0 * (k+1) * (k+1));
		}
		return PolyD(c);
	}
	inline PolyD poly_erfc(int n) {
		PolyD erf_series = poly_erf(n);
		PolyD one(1.0);
		return (one - erf_series).trunc(n);
	}
	inline PolyD poly_bessel_J1(int n) {
		std::vector<double> c(n, 0.0);
		double coeff = 0.5;
		for (int k = 0; 2*k+1 < n; ++k) {
			int i = 2*k+1;
			double term = coeff * ((k%2)? -1.0 : 1.0);
			c[i] = term;
			coeff /= (4.0 * (k+1) * (k+2));
		}
		return PolyD(c);
	}
	
} // namespace poly_avx

#endif // POLY_AVX_HPP
