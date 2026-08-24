#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace termis {

struct SourceLocation {
  std::size_t offset = 0;
  std::size_t line = 1;
  std::size_t column = 1;
};

struct Diagnostic {
  SourceLocation location;
  std::string message;
};

class ReadError final : public std::runtime_error {
 public:
  explicit ReadError(Diagnostic diagnostic);

  const Diagnostic& diagnostic() const;

 private:
  Diagnostic diagnostic_;
};

struct IntegerLiteral {
  std::int64_t value = 0;
};

struct FloatLiteral {
  double value = 0.0;
};

struct StringLiteral {
  std::string value;
};

struct Symbol {
  std::string name;
};

struct UnitLiteral {};

struct Form;

using FormPtr = std::unique_ptr<Form>;

struct List {
  std::vector<FormPtr> elements;
};

using FormKind =
    std::variant<List, Symbol, IntegerLiteral, FloatLiteral, StringLiteral, UnitLiteral>;

struct Form {
  SourceLocation location;
  FormKind kind;
};

std::vector<FormPtr> read_forms(std::string_view source);

}  // namespace termis
