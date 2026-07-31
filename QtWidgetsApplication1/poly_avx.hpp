// poly_avx.hpp — 高性能多项式全家桶 (C++98, SIMD, FMA3)
//
// 本文件提供实系数/复系数多项式的全面运算。
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

// SIMD intrinsic 头文件（cpu_dispatch.cpp 中会使用，这里保留以便兼容性检查）
#include <emmintrin.h>
#include <pmmintrin.h>
#ifdef __AVX__
#include <immintrin.h>
#endif

// 前向声明，实际定义在 cpu_dispatch.h / cpu_dispatch.cpp 中

namespace poly_avx {
	typedef std::complex<double> cd;
	typedef void (*pointwise_mul_func)(cd*, const cd*, int);   // 定义函数指针类型
	extern pointwise_mul_func pointwise_mul;                    // 全局函数指针（将在 cpu_dispatch.cpp 中定义）
	void init_cpu_dispatch();                                   // 初始化调度（可选，用 AutoInit 自动调用）

	const double PI = std::acos(-1.0);
	const double EPS = 1e-12;
	const cd I(0.0, 1.0);

	inline int next_pow2(int n) {
		int r = 1;
		while (r < n) r <<= 1;
		return r;
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

	// ---------- 卷积重载 ----------
	inline std::vector<double> convolution(const std::vector<double>& a, const std::vector<double>& b, int lim) {
		if (a.empty() || b.empty()) return std::vector<double>();
		int n = (int)a.size(), m = (int)b.size(), sz = n + m - 1;
		int N = next_pow2(sz);
		std::vector<cd> A(N, 0.0), B(N, 0.0);
		for (int i = 0; i < n; ++i) A[i] = cd(a[i], 0.0);
		for (int i = 0; i < m; ++i) B[i] = cd(b[i], 0.0);
		fft(&A[0], N, false);
		fft(&B[0], N, false);
		pointwise_mul(&A[0], &B[0], N);      // 使用函数指针
		fft(&A[0], N, true);
		int res_sz = (lim < sz) ? lim : sz;
		std::vector<double> res(res_sz);
		for (int i = 0; i < res_sz; ++i) res[i] = A[i].real();
		return res;
	}

	inline std::vector<cd> convolution(const std::vector<cd>& a, const std::vector<cd>& b, int lim) {
		if (a.empty() || b.empty()) return std::vector<cd>();
		int n = (int)a.size(), m = (int)b.size(), sz = n + m - 1;
		int N = next_pow2(sz);
		std::vector<cd> A(N, 0.0), B(N, 0.0);
		for (int i = 0; i < n; ++i) A[i] = a[i];
		for (int i = 0; i < m; ++i) B[i] = b[i];
		fft(&A[0], N, false);
		fft(&B[0], N, false);
		pointwise_mul(&A[0], &B[0], N);      // 使用函数指针
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

		// 对数 (自动归一化常数项)
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

		// 指数
		Poly exp(int n) const {
			if (!data.empty() && std::abs(data[0]) > EPS) {
				T c = data[0];
				Poly B = (*this) - Poly(c);
				Poly expB = B.exp(n);
				return expB * std::exp(c);
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

	// ---------- 三角函数（带归一化） ----------
	inline PolyC to_complex(const PolyD& p) {
		PolyC res(p.size());
		for (int i = 0; i < p.size(); ++i) res[i] = cd(p[i], 0.0);
		return res;
	}

	inline void poly_sincos(const PolyD& A, int n, PolyD& sinA, PolyD& cosA) {
		PolyC Ac = to_complex(A.trunc(n));
		PolyC exp_iA = (Ac * I).exp(n);
		PolyC exp_miA = (Ac * (-I)).exp(n);
		sinA = PolyD(n);
		cosA = PolyD(n);
		for (int i = 0; i < n; ++i) {
			sinA[i] = (exp_iA[i] - exp_miA[i]).imag() * 0.5;
			cosA[i] = (exp_iA[i] + exp_miA[i]).real() * 0.5;
		}
		// 归一化 sin²+cos² = 1
		PolyD s2 = (sinA * sinA).trunc(n);
		PolyD c2 = (cosA * cosA).trunc(n);
		PolyD norm = s2 + c2;
		PolyD inv_norm = (PolyD(1.0) + norm).trunc(n) * 0.5;
		inv_norm = inv_norm.sqrt(n).inv(n);
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
		PolyD S = poly_sin(A, n);
		PolyD C = poly_cos(A, n);
		return (S * C.inv(n)).trunc(n);
	}

	// ---------- 反三角函数 ----------
	inline PolyD poly_asin(const PolyD& A, int n) {
		PolyD Acopy = A;
		if (!Acopy.data.empty() && std::abs(Acopy.data[0]) < 1e-8) {
			Acopy.data[0] = 0.0;
		}
		assert(Acopy.data.empty() || std::abs(Acopy.data[0]) < EPS);
		PolyD DA = Acopy.deriv();
		PolyD one(1.0);
		PolyD sqrt_term = (one - (Acopy * Acopy).trunc(n)).sqrt(n);
		return (DA * sqrt_term.inv(n)).trunc(n - 1).integ().trunc(n);
	}

	inline PolyD poly_acos(const PolyD& A, int n) {
		assert(A.data.empty() || std::abs(A[0]) < EPS);
		PolyD asinA = poly_asin(A, n);
		asinA.data[0] = PI / 2.0 - asinA.data[0];
		return asinA;
	}

	inline PolyD poly_atan(const PolyD& A, int n) {
		PolyD Acopy = A;
		if (!Acopy.data.empty() && std::abs(Acopy.data[0]) < 1e-8) {
			Acopy.data[0] = 0.0;
		}
		assert(Acopy.data.empty() || std::abs(Acopy.data[0]) < EPS);
		PolyD DA = Acopy.deriv();
		PolyD one(1.0);
		PolyD den = one + (Acopy * Acopy).trunc(n);
		return (DA * den.inv(n)).trunc(n - 1).integ().trunc(n);
	}

	// ---------- 双曲函数（带归一化） ----------

	// ---------- 双曲函数（带归一化） ----------
	inline void poly_sinhcosh(const PolyD& A, int n, PolyD& sinhA, PolyD& coshA) {
		PolyC Ac = to_complex(A.trunc(n));
		PolyC expA = (Ac).exp(n);
		PolyC expmA = (Ac * (-1.0)).exp(n);
		sinhA = PolyD(n);
		coshA = PolyD(n);
		for (int i = 0; i < n; ++i) {
			sinhA[i] = (expA[i] - expmA[i]).real() * 0.5;
			coshA[i] = (expA[i] + expmA[i]).real() * 0.5;
		}
		// 归一化 cosh² - sinh² = 1
		PolyD s2 = (sinhA * sinhA).trunc(n);
		PolyD c2 = (coshA * coshA).trunc(n);
		PolyD diff = c2 - s2;
		PolyD inv_norm = (PolyD(1.0) + diff).trunc(n) * 0.5;
		inv_norm = inv_norm.sqrt(n).inv(n);
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
		PolyD S = poly_sinh(A, n);
		PolyD C = poly_cosh(A, n);
		return (S * C.inv(n)).trunc(n);
	}
	// log1p(A) = log(1 + A), 要求常数项为 0
	inline PolyD poly_log1p(const PolyD& A, int n) {
		// 如果常数项非零但非常小（浮点噪声），自动清零
		PolyD Acopy = A;
		if (!Acopy.data.empty() && std::abs(Acopy.data[0]) < 1e-8) {
			Acopy.data[0] = 0.0;
		}
		assert(Acopy.data.empty() || std::abs(Acopy.data[0]) < 1e-8);
		PolyD one(1.0);
		return (one + Acopy).log(n);
	}
	// asinh(A)，常数项 0。使用 log1p 稳定公式避免 x≈0 时的精度损失。
	inline PolyD poly_asinh(const PolyD& A, int n) {
		assert(A.data.empty() || std::abs(A[0]) < EPS);
		if (A.data.empty()) return PolyD(n);            // A = 0 → asinh(0) = 0
		bool negative = (A.data[0] < 0);
		if (A.data.size() > 1 && A.data[0] == 0.0)
			negative = (A.data[1] < 0);
		PolyD absA = negative ? (A * (-1.0)) : A;       // |A|
		PolyD one(1.0);
		PolyD A2 = (absA * absA).trunc(n);
		PolyD sqrt_term = (one + A2).sqrt(n);           // sqrt(1 + A²)
		PolyD denom = one + sqrt_term;                  // 1 + sqrt(1 + A²)
		// 核心公式：log1p(|A| + A² / (1 + sqrt(1 + A²)))
		PolyD frac = (A2 * denom.inv(n)).trunc(n);      // A² / (1 + sqrt(1 + A²))
		PolyD inner = absA + frac;                      // |A| + A² / (1 + sqrt(1 + A²))
		if (!inner.data.empty()) inner.data[0] = 0.0;   // 强制清除常数项浮点噪声
		PolyD res = poly_log1p(inner, n);
		if (negative) res = res * (-1.0);               // 恢复符号
		return res.trunc(n);
	}

	// acosh(A)，常数项 > 1。使用 log1p 变形避免 x≈1 时的灾难性抵消。
	inline PolyD poly_acosh(const PolyD& A, int n) {
		assert(!A.data.empty() && A.data[0] > 1.0 + EPS);
		double c = A.data[0];
		// 常数项使用标量 acosh 保证精度
		double acosh_c = std::log(c + std::sqrt(c * c - 1.0));
		PolyD one(1.0);
		PolyD t = A - one;                               // t = A - 1
		PolyD two(2.0);
		PolyD t2 = (t * t).trunc(n);
		PolyD inner = (t * two + t2).trunc(n);           // 2t + t²
		PolyD sqrt_term = inner.sqrt(n);                 // sqrt(2t + t²)
		PolyD sum = t + sqrt_term;                       // t + sqrt(2t + t²)
		// 注意：这里不能用 poly_log1p，因为 sum 的常数项可以远大于 0。
		// 直接计算 log(1 + sum)，然后强制常数项为精确值。
		PolyD res = (one + sum).log(n);                  // log(1 + sum)
		res.data[0] = acosh_c;                           // 强制常数项为精确值
		return res.trunc(n);
	}
	inline PolyD poly_atanh(const PolyD& A, int n) {
		PolyD Acopy = A;
		if (!Acopy.data.empty() && std::abs(Acopy.data[0]) < 1e-8) {
			Acopy.data[0] = 0.0;
		}
		assert(Acopy.data.empty() || std::abs(Acopy.data[0]) < EPS);
		PolyD one(1.0);
		PolyD log1pA = (one + Acopy).log(n);
		PolyD log1mA = (one - Acopy).log(n);
		return ((log1pA - log1mA) * 0.5).trunc(n);
	}
	// ==================== 扩展功能 ====================

	// ---------- 阶乘 / 二项式系数 ----------
	inline std::vector<double> factorial_table(int n) {
		std::vector<double> f(n);
		f[0] = 1.0;
		for (int i = 1; i < n; ++i) f[i] = f[i - 1] * i;
		return f;
	}

	inline std::vector<double> inv_factorial_table(int n) {
		std::vector<double> f = factorial_table(n);
		std::vector<double> inv(n);
		inv[n - 1] = 1.0 / f[n - 1];
		for (int i = n - 2; i >= 0; --i) inv[i] = inv[i + 1] * (i + 1);
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
			L[i] = A_trunc[i] * fact[i];
			R[n - 1 - i] = cp * inv_fact[i];
			cp *= c;
		}
		PolyD M = L * R;
		PolyD res(n);
		for (int i = 0; i < n; ++i)
			res[i] = M[n - 1 + i] * inv_fact[i];
		return res;
	}

	// ---------- 多点求值（朴素 O(n^2)）----------
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

	// ---------- 拉格朗日插值（O(n^2)）----------
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

	// ---------- 形式幂级数复合（Brent–Kung） ----------
	inline PolyD poly_composite(const PolyD& A, const PolyD& B, int n) {
		assert(B.data.empty() || std::abs(B[0]) < EPS);
		if (A.size() == 0) return PolyD(n);
		int L = (int)std::sqrt((double)n) + 1;
		std::vector<PolyD> B_pow(L + 1);
		B_pow[0] = PolyD(1.0);
		for (int i = 1; i <= L; ++i)
			B_pow[i] = (B_pow[i - 1] * B).trunc(n);
		PolyD res;
		for (int i = L - 1; i >= 0; --i) {
			if (!res.data.empty())
				res = (res * B_pow[L]).trunc(n);
			PolyD sum;
			for (int j = 0; j < L && i * L + j < A.size(); ++j) {
				double c = A[i * L + j];
				if (std::abs(c) > EPS)
					sum = (sum + B_pow[j] * c).trunc(n);
			}
			res = (res + sum).trunc(n);
		}
		res.resize(n);
		return res;
	}

	// ---------- 复合逆（Reversion）牛顿迭代 (C++98 兼容) ----------
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
			G[0] = 0.0;   // 强制清除常数项浮点误差
		}
		G.resize(n);
		return G;
	}

	// ---------- 辅助：计算 gamma(n + 0.5) 的值 (n 为整数, C++98 兼容) ----------
	inline double gamma_half_int(int n) {
		double res = std::sqrt(PI);
		for (int i = 1; i <= n; ++i)
			res *= (i - 0.5);
		return res;
	}

	// 生成 erf 泰勒级数（前 n 项）
	inline PolyD poly_erf(int n) {
		std::vector<double> c(n, 0.0);
		double f = 2.0 / std::sqrt(PI);
		for (int k = 0; 2 * k + 1 < n; ++k) {
			int i = 2 * k + 1;
			double term = f * ((k % 2) ? -1.0 : 1.0) / (gamma_half_int(k + 1) * (2 * k + 1));
			c[i] = term;
		}
		return PolyD(c);
	}

	// erf(A(x))，要求 A(0)=0。若常数项有微小浮点噪声则直接清零后重新计算。
	inline PolyD poly_erf(const PolyD& A, int n) {
		PolyD Acopy = A;
		if (!Acopy.data.empty() && std::abs(Acopy.data[0]) < 1e-8) {
			Acopy.data[0] = 0.0;   // 直接清零，不递归
		}
		assert(Acopy.data.empty() || std::abs(Acopy.data[0]) < EPS);
		PolyD erf_series = poly_erf(n);
		return poly_composite(erf_series, Acopy, n);
	}

	inline PolyD poly_bessel_J0(int n) {
		std::vector<double> c(n, 0.0);
		double val = 1.0;
		for (int k = 0; 2 * k < n; ++k) {
			int i = 2 * k;
			c[i] = (k % 2 ? -val : val);
			val /= (4.0 * (k + 1) * (k + 1));
		}
		return PolyD(c);
	}

} // namespace poly_avx

#endif // POLY_AVX_HPP
