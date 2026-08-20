// poly_avx.hpp — 高性能多项式全家桶 (C++98, SIMD, FMA3)
//
// 本文件提供实系数/复系数多项式的全面运算。
// 所有核心运算均基于 FFT 的卷积算法，并针对双精度浮点数进行优化。
// 小规模多项式（n ≤ 64）使用朴素递推和直接卷积，避免 FFT 启动开销。
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

namespace poly_avx {
	typedef std::complex<double> cd;                     // 复数类型别名
	typedef void (*pointwise_mul_func)(cd*, const cd*, int);   // 函数指针类型，用于运行时调度复数乘法
	extern pointwise_mul_func pointwise_mul;             // 全局函数指针，指向当前 CPU 最优实现
	void init_cpu_dispatch();                            // 初始化 CPU 调度，自动选择最优 SIMD 路径

	const double PI = std::acos(-1.0);
	const double EPS = 1e-12;                            // 用于 trim 和数值判断的阈值
	const cd I(0.0, 1.0);                                // 虚数单位

	// 返回不小于 n 的最小 2 的幂
	inline int next_pow2(int n) {
		int r = 1;
		while (r < n) r <<= 1;
		return r;
	}

// ---------- FFT ----------
	// 模板化 FFT，支持 double 和 long double（若需要）
	template <typename F>
	void fft(std::complex<F>* a, int n, bool invert) {
		// 位逆序置换
		for (int i = 1, j = 0; i < n; ++i) {
			int bit = n >> 1;
			for (; j & bit; bit >>= 1) j ^= bit;
			j ^= bit;
			if (i < j) std::swap(a[i], a[j]);
		}
		// 蝴蝶操作
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
		// 逆变换时缩放
		if (invert) {
			for (int i = 0; i < n; ++i) a[i] /= n;
		}
	}

// ---------- 卷积重载 ----------
	// 实数系数卷积，小规模走朴素算法
	inline std::vector<double> convolution(const std::vector<double>& a, const std::vector<double>& b, int lim) {
		if (a.empty() || b.empty()) return std::vector<double>();
		int n = (int)a.size(), m = (int)b.size(), sz = n + m - 1;
		if (sz <= 64) {
			// 小规模：直接 O(n*m) 卷积，避免 FFT 固定开销
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
		pointwise_mul(&A[0], &B[0], N);      // 使用运行时调度的复数乘法
		fft(&A[0], N, true);
		int res_sz = (lim < sz) ? lim : sz;
		std::vector<double> res(res_sz);
		for (int i = 0; i < res_sz; ++i) res[i] = A[i].real();
		return res;
	}

	// 复数系数卷积，同样包含小规模优化
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
		std::vector<T> data;               // 系数按升幂排列

		Poly() {}
		explicit Poly(int sz) : data(sz) {}
		explicit Poly(const T& value) : data(1, value) {}
		Poly(const std::vector<T>& v) : data(v) {}

		int size() const { return (int)data.size(); }
		void resize(int n) { data.resize(n); }
		void trim() {                       // 移除最高次的接近零的项（绝对值 < EPS）
			while (!data.empty() && std::abs(data.back()) < EPS)
				data.pop_back();
		}

		T& operator[](int i) { return data[i]; }
		const T& operator[](int i) const { return data[i]; }

		// 截断到前 n 项
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

		// 多项式乘法（截断可选）
		Poly mul(const Poly& other, int lim = -1) const {
			if (data.empty() || other.data.empty()) return Poly();
			int total = size() + other.size() - 1;
			int l = (lim == -1) ? total : std::min(lim, total);
			return Poly(convolution(data, other.data, l));
		}

		// 带余除法，返回 (商, 余数)
		std::pair<Poly, Poly> divmod(const Poly& rhs) const {
			assert(!rhs.data.empty() && std::abs(rhs.data.back()) > EPS);
			Poly A = *this; A.trim();
			Poly B = rhs;   B.trim();
			if (A.size() < B.size()) return std::make_pair(Poly(), A);
			int n = A.size() - B.size() + 1;
			// 通过反转系数利用 inv 计算商
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

		// 形式导数
		Poly deriv() const {
			if (data.empty()) return Poly();
			Poly res(size() - 1);
			for (int i = 1; i < size(); ++i) res[i - 1] = T(i) * data[i];
			return res;
		}
		// 形式积分（常数项为 0）
		Poly integ() const {
			Poly res(size() + 1);
			res[0] = T(0);
			for (int i = 0; i < size(); ++i) res[i + 1] = data[i] / T(i + 1);
			return res;
		}

		// 乘法逆元（截断至 n 项）
		Poly inv(int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			if (n <= 64) {              // 小规模递推公式
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
			// 牛顿迭代
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

		// 对数（自动归一化常数项）
		Poly log(int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			if (std::abs(data[0] - T(1)) >= EPS) {
				T c = data[0];
				Poly A1 = (*this) / c;            // 常数项归一化为 1
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
				// 提取常数项因子
				T c = data[0];
				Poly B = (*this) - Poly(c);
				Poly expB = B.exp(n);
				return expB * std::exp(c);
			}
			if (n <= 64) {
				// 直接使用级数展开 sum A^k / k!
				Poly res(T(1));
				Poly term(T(1));
				for (int k = 1; k < n; ++k) {
					term = (term * (*this)).trunc(n) / T(k);
					res = res + term;
				}
				return res.trunc(n);
			}
			// 牛顿迭代
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

		// 平方根
		Poly sqrt(int n) const {
			assert(!data.empty() && std::abs(data[0]) > EPS);
			if (n <= 64) {
				// 递推公式
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
			// 牛顿迭代
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

		// 整数幂（快速幂）
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

		// 实数幂（通过 exp(log(A) * k)）
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

	// 同时计算 sin(A) 和 cos(A)，只需一次复数指数 exp(iA)
	inline void poly_sincos(const PolyD& A, int n, PolyD& sinA, PolyD& cosA) {
		PolyC Ac = to_complex(A.trunc(n));
		PolyC exp_iA = (Ac * I).exp(n);
		sinA = PolyD(n);
		cosA = PolyD(n);
		for (int i = 0; i < n; ++i) {
			cosA[i] = exp_iA[i].real();
			sinA[i] = exp_iA[i].imag();
		}
		// 归一化 sin² + cos² = 1，消除数值误差
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
	inline PolyD poly_composite(const PolyD& A, const PolyD& B, int n);

	// asin 采用积分定义（无牛顿校正，精度已足够）
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

	// atan 积分定义
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
		// 归一化 cosh² - sinh² = 1
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

	// log1p(A) = log(1 + A)，要求常数项为 0
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
		res.data[0] = std::log(a0 + std::sqrt(a0 * a0 + 1.0)); // asinh(a0)
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
		res.data[0] = 0.5 * std::log((1.0 + a0) / (1.0 - a0)); // atanh(a0)
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
	// 朴素版本，保留用于小规模
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

	// 分治树节点
	struct EvalTree {
		int left, right;
		PolyD prod;          // 区间内 (x - pts[i]) 的乘积
		EvalTree *lc, *rc;
		EvalTree(int l, int r) : left(l), right(r), prod(1.0), lc(NULL), rc(NULL) {}
	};

	// 构建分治树
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

	// 递归释放树
	inline void delete_eval_tree(EvalTree* node) {
		if (!node) return;
		delete_eval_tree(node->lc);
		delete_eval_tree(node->rc);
		delete node;
	}

	// 递归求值
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

	// 快速多点求值（小规模自动回退朴素）
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
	// 朴素版本
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

	// 递归合并构建插值多项式
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

	// 快速插值
	inline PolyD multipoint_interpolate_fast(const std::vector<double>& x, const std::vector<double>& y) {
		int n = (int)x.size();
		if (n <= 32) {
			return multipoint_interpolate(x, y);
		}
		EvalTree* root = build_eval_tree(x, 0, n);
		PolyD prod = root->prod;
		PolyD dprod = prod.deriv();
		std::vector<double> deriv_vals(n);
		eval_rec(dprod, root, deriv_vals);
		std::vector<double> w(n);
		for (int i = 0; i < n; ++i) {
			w[i] = y[i] / deriv_vals[i];
		}
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

// ---------- 辅助：计算 gamma(n + 0.5) ----------
	inline double gamma_half_int(int n) {
		double res = std::sqrt(PI);
		for (int i = 1; i <= n; ++i)
			res *= (i - 0.5);
		return res;
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
		if (!res.data.empty()) res.data[0] = 0.0;  // 消除 erf(0) 噪声
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
