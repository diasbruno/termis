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
    (extern fn c-abs ((value i64)) i64 "llabs")
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

  require(program.forms.size() == 13, "expected thirteen semantic forms");
  require(program.forms[0]->kind == termis::SemanticKind::type_declaration,
          "expected type declaration");
  require(program.forms[1]->kind == termis::SemanticKind::function_declaration,
          "expected function declaration");
  require(program.forms[2]->kind == termis::SemanticKind::extern_function_declaration,
          "expected extern function declaration");
  require(program.forms[3]->kind == termis::SemanticKind::let_expression,
          "expected let expression");
  require(program.forms[4]->kind == termis::SemanticKind::do_expression,
          "expected do expression");
  require(program.forms[5]->kind == termis::SemanticKind::match_expression,
          "expected match expression");
  require(program.forms[6]->kind == termis::SemanticKind::const_declaration,
          "expected const declaration");
  require(program.forms[7]->kind == termis::SemanticKind::application,
          "expected application");
  require(program.forms[8]->kind == termis::SemanticKind::application,
          "expected call to be an application");
  require(program.forms[9]->kind == termis::SemanticKind::list_expression,
          "expected empty list expression");
  require(program.forms[10]->kind == termis::SemanticKind::list_expression,
          "expected list-headed list expression");
  require(program.forms[11]->kind == termis::SemanticKind::list_expression,
          "expected non-symbol-headed list expression");
  require(program.forms[12]->kind == termis::SemanticKind::atom,
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

void rejects_unknown_type_references() {
  try {
    (void)analyze("(type MissingBox Missing)");
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "unknown type name",
            "expected unknown type diagnostic");
    return;
  }

  require(false, "expected unknown type failure");
}

void rejects_missing_generic_arguments() {
  try {
    (void)analyze(R"(
      (type Box (T) (product (value T)))
      (type Bad Box)
    )");
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "generic type requires type arguments",
            "expected missing generic arguments diagnostic");
    return;
  }

  require(false, "expected missing generic arguments failure");
}

void rejects_bad_generic_argument_count() {
  try {
    (void)analyze(R"(
      (type Box (T) (product (value T)))
      (type Bad (Box i32 bool))
    )");
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "generic type argument count mismatch",
            "expected generic arity diagnostic");
    return;
  }

  require(false, "expected bad generic arity failure");
}

void rejects_applying_concrete_types() {
  try {
    (void)analyze(R"(
      (type UserId u64)
      (type Bad (UserId i32))
    )");
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "type does not accept type arguments",
            "expected concrete type application diagnostic");
    return;
  }

  require(false, "expected concrete type application failure");
}

}  // namespace

int main() {
  recognizes_core_forms();
  rejects_bad_type_declaration();
  rejects_bad_type_body();
  accepts_empty_lists();
  collects_type_declarations();
  rejects_unknown_type_references();
  rejects_missing_generic_arguments();
  rejects_bad_generic_argument_count();
  rejects_applying_concrete_types();
}
