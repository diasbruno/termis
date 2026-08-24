#include "reader.hpp"
#include "semantic.hpp"
#include "type.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "semantic test failed: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

termis::Program analyze(std::string_view source) {
  auto forms = termis::read_forms(source);
  return termis::analyze_forms(forms);
}

void recognizes_core_forms() {
  auto program = analyze(R"(
    (type UserId u64)
    (fn noop () unit .)
    (let ((x 1)) x)
    (do .)
    (match value (_ .))
    (const size 42)
    (+ 1 2)
    (call ok 1 0)
    ()
    (list 1 2)
    ((f) 1)
    42
  )");

  require(program.forms.size() == 12, "expected twelve semantic forms");
  require(program.forms[0]->kind == termis::SemanticKind::type_declaration,
          "expected type declaration");
  require(program.forms[1]->kind == termis::SemanticKind::function_declaration,
          "expected function declaration");
  require(program.forms[2]->kind == termis::SemanticKind::let_expression,
          "expected let expression");
  require(program.forms[3]->kind == termis::SemanticKind::do_expression,
          "expected do expression");
  require(program.forms[4]->kind == termis::SemanticKind::match_expression,
          "expected match expression");
  require(program.forms[5]->kind == termis::SemanticKind::const_declaration,
          "expected const declaration");
  require(program.forms[6]->kind == termis::SemanticKind::application,
          "expected application");
  require(program.forms[7]->kind == termis::SemanticKind::application,
          "expected call to be an application");
  require(program.forms[8]->kind == termis::SemanticKind::list_expression,
          "expected empty list expression");
  require(program.forms[9]->kind == termis::SemanticKind::list_expression,
          "expected list-headed list expression");
  require(program.forms[10]->kind == termis::SemanticKind::list_expression,
          "expected non-symbol-headed list expression");
  require(program.forms[11]->kind == termis::SemanticKind::atom,
          "expected atom");
}

void rejects_bad_type_declaration() {
  try {
    (void)analyze("(type 42 u64)");
  } catch (const termis::SemanticError& error) {
    require(error.diagnostic().message == "type declaration name must be a symbol",
            "expected type name diagnostic");
    return;
  }

  require(false, "expected bad type declaration failure");
}

void rejects_bad_type_body() {
  try {
    (void)analyze("(type Buffer (array u8))");
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "array type requires an element type and compile-time size",
            "expected type body diagnostic");
    return;
  }

  require(false, "expected bad type body failure");
}

void accepts_empty_lists() {
  auto program = analyze("()");

  require(program.forms.size() == 1, "expected one semantic form");
  require(program.forms[0]->kind == termis::SemanticKind::list_expression,
          "expected empty list to remain a list expression");
}

void collects_type_declarations() {
  auto program = analyze(R"(
    (type UserId u64)
    (type Pair (A B) (product (first A) (second B)))
  )");

  require(program.types.size() == 2, "expected two type declarations");

  const auto* user_id = program.types.find("UserId");
  require(user_id != nullptr, "expected UserId declaration");
  require(user_id->body->primitive == termis::PrimitiveType::u64, "expected UserId body");

  const auto* pair = program.types.find("Pair");
  require(pair != nullptr, "expected Pair declaration");
  require(pair->parameters.size() == 2, "expected Pair parameters");
}

}  // namespace

int main() {
  recognizes_core_forms();
  rejects_bad_type_declaration();
  rejects_bad_type_body();
  accepts_empty_lists();
  collects_type_declarations();
}
