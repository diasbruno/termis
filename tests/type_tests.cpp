#include "reader.hpp"
#include "type.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "type test failed: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

termis::TypePtr parse_one_type(std::string_view source) {
  auto forms = termis::read_forms(source);
  require(forms.size() == 1, "expected one form");
  return termis::parse_type(*forms[0]);
}

void parses_primitive_types() {
  struct Case {
    std::string_view source;
    termis::PrimitiveType primitive;
  };

  static constexpr std::array<Case, 13> cases = {{
      {"i8", termis::PrimitiveType::i8},
      {"i16", termis::PrimitiveType::i16},
      {"i32", termis::PrimitiveType::i32},
      {"i64", termis::PrimitiveType::i64},
      {"u8", termis::PrimitiveType::u8},
      {"u16", termis::PrimitiveType::u16},
      {"u32", termis::PrimitiveType::u32},
      {"u64", termis::PrimitiveType::u64},
      {"f32", termis::PrimitiveType::f32},
      {"f64", termis::PrimitiveType::f64},
      {"bool", termis::PrimitiveType::bool_},
      {"unit", termis::PrimitiveType::unit},
      {"void", termis::PrimitiveType::void_},
  }};

  for (const auto& test_case : cases) {
    const auto type = parse_one_type(test_case.source);

    require(type->kind == termis::TypeKind::primitive, "expected primitive type");
    require(type->primitive == test_case.primitive, "expected matching primitive");
  }
}

void parses_named_and_applied_types() {
  const auto named = parse_one_type("Point");
  require(named->kind == termis::TypeKind::name, "expected named type");
  require(named->name == "Point", "expected Point name");

  const auto applied = parse_one_type("(Pair i32 f64)");
  require(applied->kind == termis::TypeKind::application, "expected type application");
  require(applied->name == "Pair", "expected Pair application");
  require(applied->arguments.size() == 2, "expected two type arguments");
  require(applied->arguments[0]->primitive == termis::PrimitiveType::i32, "expected first argument");
  require(applied->arguments[1]->primitive == termis::PrimitiveType::f64, "expected second argument");
}

void parses_core_type_constructors() {
  const auto pointer = parse_one_type("(& Point)");
  require(pointer->kind == termis::TypeKind::pointer, "expected pointer type");
  require(pointer->element->kind == termis::TypeKind::name, "expected pointed type");

  const auto array = parse_one_type("(array u8 256)");
  require(array->kind == termis::TypeKind::array, "expected array type");
  require(array->element->primitive == termis::PrimitiveType::u8, "expected array element");
  require(array->array_size == 256, "expected array size");

  const auto slice = parse_one_type("(slice u8)");
  require(slice->kind == termis::TypeKind::slice, "expected slice type");
  require(slice->element->primitive == termis::PrimitiveType::u8, "expected slice element");

  const auto function = parse_one_type("(fn (i32 i32) i32)");
  require(function->kind == termis::TypeKind::function, "expected function type");
  require(function->arguments.size() == 2, "expected two function arguments");
  require(function->result->primitive == termis::PrimitiveType::i32, "expected function result");
}

void parses_aggregate_types() {
  const auto product = parse_one_type("(product (x f32) (y f32))");
  require(product->kind == termis::TypeKind::product, "expected product type");
  require(product->fields.size() == 2, "expected product fields");
  require(product->fields[0].name == "x", "expected product field name");

  const auto sum = parse_one_type("(sum None (Some i32))");
  require(sum->kind == termis::TypeKind::sum, "expected sum type");
  require(sum->alternatives.size() == 2, "expected sum alternatives");
  require(sum->alternatives[0].name == "None", "expected empty alternative");
  require(sum->alternatives[1].payload.size() == 1, "expected payload alternative");

  const auto union_type = parse_one_type("(union (integer i64) (floating f64))");
  require(union_type->kind == termis::TypeKind::union_, "expected union type");
  require(union_type->fields.size() == 2, "expected union fields");
}

void rejects_bad_type_forms() {
  try {
    (void)parse_one_type("(array u8)");
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "array type requires an element type and compile-time size",
            "expected array diagnostic");
    return;
  }

  require(false, "expected bad array failure");
}

void parses_type_declarations() {
  auto forms = termis::read_forms("(type Pair (A B) (product (first A) (second B)))");
  const auto declaration = termis::parse_type_declaration(*forms[0]);

  require(declaration.name == "Pair", "expected declaration name");
  require(declaration.parameters.size() == 2, "expected type parameters");
  require(declaration.parameters[0].name == "A", "expected first type parameter");
  require(declaration.parameters[1].name == "B", "expected second type parameter");
  require(declaration.body->kind == termis::TypeKind::product, "expected declaration body");
}

void rejects_bad_type_parameters() {
  try {
    auto forms = termis::read_forms("(type Id (42) u64)");
    (void)termis::parse_type_declaration(*forms[0]);
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "type parameter must be a symbol",
            "expected type parameter diagnostic");
    return;
  }

  require(false, "expected bad type parameter failure");
}

void rejects_duplicate_type_parameters() {
  try {
    auto forms = termis::read_forms("(type Bad (T T) T)");
    (void)termis::parse_type_declaration(*forms[0]);
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "type parameter redefines existing parameter",
            "expected duplicate type parameter diagnostic");
    return;
  }

  require(false, "expected duplicate type parameter failure");
}

void rejects_duplicate_type_declarations() {
  try {
    termis::TypeEnvironment environment;
    auto first = termis::read_forms("(type UserId u64)");
    auto second = termis::read_forms("(type UserId i64)");
    environment.declare(termis::parse_type_declaration(*first[0]));
    environment.declare(termis::parse_type_declaration(*second[0]));
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "type declaration redefines existing type",
            "expected duplicate declaration diagnostic");
    return;
  }

  require(false, "expected duplicate declaration failure");
}

void instantiates_generic_type_declarations() {
  termis::TypeEnvironment environment;
  auto forms = termis::read_forms("(type Pair (A B) (product (first A) (second B)))");
  environment.declare(termis::parse_type_declaration(*forms[0]));

  const auto application = parse_one_type("(Pair i32 bool)");
  const auto instantiated = termis::instantiate_type_application(*application, environment);

  require(instantiated->kind == termis::TypeKind::product, "expected instantiated product");
  require(instantiated->fields.size() == 2, "expected instantiated fields");
  require(instantiated->fields[0].type->primitive == termis::PrimitiveType::i32,
          "expected substituted first field");
  require(instantiated->fields[1].type->primitive == termis::PrimitiveType::bool_,
          "expected substituted second field");
}

void instantiates_nested_generic_type_declarations() {
  termis::TypeEnvironment environment;
  auto box_forms = termis::read_forms("(type Box (T) (product (value T)))");
  auto pair_forms = termis::read_forms("(type Pair (A B) (product (first A) (second B)))");
  environment.declare(termis::parse_type_declaration(*box_forms[0]));
  environment.declare(termis::parse_type_declaration(*pair_forms[0]));

  const auto application = parse_one_type("(Box (Pair i32 bool))");
  const auto instantiated = termis::instantiate_type_application(*application, environment);

  require(instantiated->kind == termis::TypeKind::product, "expected outer instantiated product");
  require(instantiated->fields.size() == 1, "expected outer instantiated field");
  require(instantiated->fields[0].type->kind == termis::TypeKind::application,
          "expected nested generic application to be preserved");
  require(instantiated->fields[0].type->name == "Pair", "expected nested Pair application");
}

void rejects_bad_generic_arity() {
  try {
    termis::TypeEnvironment environment;
    auto forms = termis::read_forms("(type Box (T) (product (value T)))");
    environment.declare(termis::parse_type_declaration(*forms[0]));
    const auto application = parse_one_type("(Box i32 bool)");
    (void)termis::instantiate_type_application(*application, environment);
  } catch (const termis::TypeError& error) {
    require(error.diagnostic().message == "generic type argument count mismatch",
            "expected generic arity diagnostic");
    return;
  }

  require(false, "expected bad generic arity failure");
}

}  // namespace

int main() {
  parses_primitive_types();
  parses_named_and_applied_types();
  parses_core_type_constructors();
  parses_aggregate_types();
  rejects_bad_type_forms();
  parses_type_declarations();
  rejects_bad_type_parameters();
  rejects_duplicate_type_parameters();
  rejects_duplicate_type_declarations();
  instantiates_generic_type_declarations();
  instantiates_nested_generic_type_declarations();
  rejects_bad_generic_arity();
}
