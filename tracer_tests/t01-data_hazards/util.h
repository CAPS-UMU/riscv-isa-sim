#ifndef _util_h_
#define _util_h_

#include <vector>
#include <string>
#include <numeric>
#include <utility>
#include <tgmath.h>
#include <iostream>
#include <cstring>
#define TRACER_VERBOSE 0
#include "tracer-interface.h"

bool parse_size_arg(const char* arg, const char* name, size_t& var) {
  auto len = strlen(name);
  if (arg[0] == '-' && arg[1] == '-' && !strncmp(&arg[2], name, len) && arg[len + 2] == '=') {
    errno = 0;
    char *p;
    var = strtol(&arg[3 + len], &p, 10);
    if (errno != 0 || p == &arg[3 + len] || *p != '\0') {
      std::cerr << "El valor de --" << name << " debe ser entero: " << &arg[3+len] << std::endl;
      return false;
    } else {
      return true;
    }
  } else {
    return false;
  }
}

bool parse_bool_arg(const char* arg, const char* name, bool& var) {
  auto len = strlen(name);
  if (arg[0] == '-' && arg[1] == '-' && !strncmp(&arg[2], name, len) && (arg[len + 2] == '=' || arg[len + 2] == '\0')) {
    if (arg[len + 2] == '\0' || !strcmp(&arg[2 + len], "=true")) {
      var = true;
      return true;
    } else if (!strcmp(&arg[2 + len], "=false")) {
      var = false;
      return true;
    } else {
      std::cerr << "El valor de --" << name << " debe ser un true o false: " << &arg[3+len] << std::endl;
      return false;
    }
  } else {
    return false;
  }
}

template<typename F, typename ...Args>
uint64_t measure_cycles(F func, Args&&... args) {
  auto start = tracer_rdcycle();
  func(std::forward<Args>(args)...);
  return tracer_rdcycle() - start;
}

template<typename T>
T vector_average(const std::vector<T>& v) {
  return reduce(v.begin(), v.end(), 0.0) / v.size();
}

template<typename T>
T vector_stddev(const std::vector<T>& v) {
  T avg = vector_average(v);
  return (sqrt)(accumulate(v.begin(), v.end(), 0.0, // The parenthesis around sqrt are a workaround for an ICC bug
                           [=](T acc, T t){
                             T dt = avg - t;
                             return acc + dt * dt; })
                / v.size());
}

template<typename T>
T vector_average_harmonic(const std::vector<T>& v) {
  return v.size() / accumulate(v.begin(), v.end(), 0.0,
                               [=](T acc, T t){ return acc + T(1) / t; });
}

template<typename T>
T vector_stddev_harmonic(const std::vector<T>& v) {
  // F.C. Lam, C.T. Hung, D.G. Perrier, Estimation of Variance for Harmonic Mean Half-Lives, Journal of Pharmaceutical Sciences, Volume 74, Issue 2, 1985, Pages 229-231, ISSN 0022-3549, https://doi.org/10.1002/jps.2600740229.
  // Pharmaceutics 2017, 9, 14; doi:10.3390/pharmaceutics9020014
  T avg = vector_average_harmonic(v);
  T iavg = T(1) / avg;
  return (sqrt)(accumulate(v.begin(), v.end(), 0.0,  // The parenthesis around sqrt are a workaround for an ICC bug
                           [=](T acc, T t){
                             T dt = iavg - T(1) / t;
                             return acc + dt * dt; })
                / v.size()) * avg * avg;
}

// To be able to use pragmas in macros
#define MACRO_PRAGMA(x) _Pragma(#x)

// Portable #pragma unroll
#ifdef __clang__
   // equivalent to #pragma unroll (x)
#  define PRAGMA_UNROLL(x) MACRO_PRAGMA(unroll (x)) // supported by clang and icc
#else
   // equivalent to #pragma GCC unroll (x)
#  define PRAGMA_UNROLL(x) MACRO_PRAGMA(GCC unroll (x)) // supported by clang and icc
#endif

#endif
