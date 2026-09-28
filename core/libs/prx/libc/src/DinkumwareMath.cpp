#include "prx/libc/include/General.hpp"
#include <cmath>
#include <limits>

// Dinkumware math helpers imported by guest code: classification codes and scaled hyperbolics.
namespace {
constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;
}

extern "C" {

short APS5_VABI _Dtest_nid_postfix(const double* value) {
    if (value == nullptr) throw std::invalid_argument("_Dtest: null");
    switch (std::fpclassify(*value)) {
        case FP_NAN: return NotANumber;
        case FP_INFINITE: return Infinite;
        case FP_ZERO: return Zero;
        case FP_SUBNORMAL: return Denormal;
        default: return Finite;
    }
}

double APS5_VABI _Cosh_nid_postfix(double x, double y) { return y * std::cosh(x); }
double APS5_VABI _Sinh_nid_postfix(double x, double y) { return y * std::sinh(x); }

// _Inf is a data object (union _Dconst) holding +infinity.
extern const double _Inf_nid_postfix = std::numeric_limits<double>::infinity();

double APS5_VABI modf_nid_postfix(double x, double* integral) { return std::modf(x, integral); }
float APS5_VABI modff_nid_postfix(float x, float* integral) { return std::modf(x, integral); }
double APS5_VABI tanh_nid_postfix(double x) { return std::tanh(x); }
double APS5_VABI logb_nid_postfix(double x) { return std::logb(x); }
int APS5_VABI __isinf_nid_postfix(double x) { return std::isinf(x) ? 1 : 0; }
int APS5_VABI __isnanf_nid_postfix(float x) { return std::isnan(x) ? 1 : 0; }

}
