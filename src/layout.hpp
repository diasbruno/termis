#pragma once

#include "type.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace termis {

class LayoutError final : public std::runtime_error {
 public:
  explicit LayoutError(Diagnostic diagnostic);

  const Diagnostic& diagnostic() const;

 private:
  Diagnostic diagnostic_;
};

struct FieldLayout {
  std::string name;
  std::uint64_t offset = 0;
  std::uint64_t size = 0;
  std::uint64_t alignment = 1;
};

struct Layout {
  std::uint64_t size = 0;
  std::uint64_t alignment = 1;
  std::vector<FieldLayout> fields;
};

class LayoutEngine {
 public:
  explicit LayoutEngine(const TypeEnvironment& types);

  Layout compute(const Type& type) const;

 private:
  const TypeEnvironment& types_;
};

}  // namespace termis
