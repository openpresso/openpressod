#ifndef LAMBDA_OVERLOAD_HPP
#define LAMBDA_OVERLOAD_HPP

// NOLINTBEGIN(misc-multiple-inheritance)

template <typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};

// NOLINTEND(misc-multiple-inheritance)

#endif // LAMBDA_OVERLOAD_HPP