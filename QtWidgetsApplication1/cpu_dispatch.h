// cpu_dispatch.h – CPU 特性检测与动态调度 (C++98 兼容)
#ifndef CPU_DISPATCH_H
#define CPU_DISPATCH_H

#include "poly_avx.hpp"

namespace poly_avx {

// 函数指针类型：复数逐点乘法
typedef void (*pointwise_mul_func)(cd*, const cd*, int);

// 各个版本的前置声明
void pointwise_mul_sse3(cd* A, const cd* B, int len);
void pointwise_mul_avx(cd* A, const cd* B, int len);
void pointwise_mul_avx512(cd* A, const cd* B, int len);

// 全局函数指针，初始化为检测后的最优版本
extern pointwise_mul_func g_pointwise_mul;

// 初始化函数：在 main 之前调用一次
void init_cpu_dispatch();

} // namespace poly_avx

#endif // CPU_DISPATCH_H
