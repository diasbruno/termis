#pragma once

#include "reader.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace termis {

class TypeError final : public std::runtime_error {
 public:
  explicit TypeError(Diagnostic diagnostic);

  const Diagnostic& diagnostic() const;

 private:
  Diagnostic diagnostic_;
};

enum class PrimitiveType {
  i8,
  i16,
  i32,
  i64,
  u8,
  u16,
  u32,
  u64,
  f32,
  f64,
  bool_,
  unit,
  void_,
};

enum class TypeKind {
  primitive,
  name,
  pointer,
  array,
  slice,
  function,
  product,
  sum,
  union_,
  application,
};

struct Type;
using TypePtr = std::unique_ptr<Type>;

struct Field {
  std::string name;
  TypePtr type;
};

struct SumAlternative {
  std::string name;
  std::vector<TypePtr> payload;
};

struct Type {
  TypeKind kind = TypeKind::name;
  SourceLocation location;

  PrimitiveType primitive = PrimitiveType::unit;
  std::string name;

  TypePtr element;
  TypePtr result;
  std::vector<TypePtr> arguments;
  std::vector<Field> fields;
  std::vector<SumAlternative> alternatives;

  const Form* array_size = nullptr;
};

TypePtr parse_type(const Form& form);

}  // namespace termis
