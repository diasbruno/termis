#pragma once

#include "reader.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace termis {

class SemanticError final : public std::runtime_error {
 public:
  explicit SemanticError(Diagnostic diagnostic);

  const Diagnostic& diagnostic() const;

 private:
  Diagnostic diagnostic_;
};

enum class SemanticKind {
  type_declaration,
  function_declaration,
  let_expression,
  do_expression,
  match_expression,
  const_declaration,
  application,
  list_expression,
  atom,
};

struct SemanticNode {
  SemanticKind kind;
  const Form* form;
};

using SemanticNodePtr = std::unique_ptr<SemanticNode>;

struct Program {
  std::vector<SemanticNodePtr> forms;
};

Program analyze_forms(const std::vector<FormPtr>& forms);

}  // namespace termis
