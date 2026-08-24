#include "type.hpp"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace termis {
namespace {

const Symbol* as_symbol(const Form& form) {
  return std::get_if<Symbol>(&form.kind);
}

const List* as_list(const Form& form) {
  return std::get_if<List>(&form.kind);
}

const Form& element(const List& list, std::size_t index) {
  return *list.elements[index];
}

[[noreturn]] void fail(SourceLocation location, std::string message) {
  throw TypeError(Diagnostic{location, std::move(message)});
}

PrimitiveType primitive_from_name(std::string_view name, bool& found) {
  struct Entry {
    std::string_view name;
    PrimitiveType type;
  };

  static constexpr std::array<Entry, 13> entries = {{
      {"i8", PrimitiveType::i8},
      {"i16", PrimitiveType::i16},
      {"i32", PrimitiveType::i32},
      {"i64", PrimitiveType::i64},
      {"u8", PrimitiveType::u8},
      {"u16", PrimitiveType::u16},
      {"u32", PrimitiveType::u32},
      {"u64", PrimitiveType::u64},
      {"f32", PrimitiveType::f32},
      {"f64", PrimitiveType::f64},
      {"bool", PrimitiveType::bool_},
      {"unit", PrimitiveType::unit},
      {"void", PrimitiveType::void_},
  }};

  for (const auto& entry : entries) {
    if (entry.name == name) {
      found = true;
      return entry.type;
    }
  }

  found = false;
  return PrimitiveType::unit;
}

TypePtr make_type(TypeKind kind, SourceLocation location) {
  auto type = std::make_unique<Type>();
  type->kind = kind;
  type->location = location;
  return type;
}

TypePtr substitute_type_impl(const Type& type, const std::unordered_map<std::string, const Type*>& substitutions);

std::vector<TypePtr> clone_type_vector(const std::vector<TypePtr>& types) {
  std::vector<TypePtr> cloned;
  cloned.reserve(types.size());
  for (const auto& type : types) {
    cloned.push_back(clone_type(*type));
  }
  return cloned;
}

std::vector<TypePtr> substitute_type_vector(const std::vector<TypePtr>& types,
                                            const std::unordered_map<std::string, const Type*>& substitutions) {
  std::vector<TypePtr> substituted;
  substituted.reserve(types.size());
  for (const auto& type : types) {
    substituted.push_back(substitute_type_impl(*type, substitutions));
  }
  return substituted;
}

std::string require_symbol_name(const Form& form, std::string message) {
  const auto* symbol = as_symbol(form);
  if (symbol == nullptr) {
    fail(form.location, std::move(message));
  }
  return symbol->name;
}

Field parse_field(const Form& form, std::string_view aggregate_name) {
  const auto* list = as_list(form);
  if (list == nullptr || list->elements.size() != 2) {
    fail(form.location, std::string(aggregate_name) + " field requires a name and type");
  }

  return Field{
      require_symbol_name(element(*list, 0), std::string(aggregate_name) + " field name must be a symbol"),
      parse_type(element(*list, 1)),
  };
}

SumAlternative parse_sum_alternative(const Form& form) {
  if (const auto* symbol = as_symbol(form)) {
    return SumAlternative{symbol->name, {}};
  }

  const auto* list = as_list(form);
  if (list == nullptr || list->elements.empty()) {
    fail(form.location, "sum alternative requires a constructor name");
  }

  SumAlternative alternative{
      require_symbol_name(element(*list, 0), "sum alternative name must be a symbol"),
      {},
  };
  for (std::size_t index = 1; index < list->elements.size(); ++index) {
    alternative.payload.push_back(parse_type(element(*list, index)));
  }
  return alternative;
}

TypePtr parse_type_list(const Form& form, const List& list) {
  if (list.elements.empty()) {
    fail(form.location, "type expression list cannot be empty");
  }

  const auto* head = as_symbol(element(list, 0));
  if (head == nullptr) {
    fail(element(list, 0).location, "type constructor must be a symbol");
  }

  if (head->name == "&") {
    if (list.elements.size() != 2) {
      fail(form.location, "pointer type requires exactly one element type");
    }
    auto type = make_type(TypeKind::pointer, form.location);
    type->element = parse_type(element(list, 1));
    return type;
  }

  if (head->name == "array") {
    if (list.elements.size() != 3) {
      fail(form.location, "array type requires an element type and compile-time size");
    }
    const auto* size = std::get_if<IntegerLiteral>(&element(list, 2).kind);
    if (size == nullptr) {
      fail(element(list, 2).location, "array size must be an integer literal");
    }
    auto type = make_type(TypeKind::array, form.location);
    type->element = parse_type(element(list, 1));
    type->array_size = size->value;
    return type;
  }

  if (head->name == "slice") {
    if (list.elements.size() != 2) {
      fail(form.location, "slice type requires exactly one element type");
    }
    auto type = make_type(TypeKind::slice, form.location);
    type->element = parse_type(element(list, 1));
    return type;
  }

  if (head->name == "fn") {
    if (list.elements.size() != 3) {
      fail(form.location, "function type requires arguments and return type");
    }
    const auto* arguments = as_list(element(list, 1));
    if (arguments == nullptr) {
      fail(element(list, 1).location, "function type arguments must be a list");
    }
    auto type = make_type(TypeKind::function, form.location);
    for (const auto& argument : arguments->elements) {
      type->arguments.push_back(parse_type(*argument));
    }
    type->result = parse_type(element(list, 2));
    return type;
  }

  if (head->name == "product") {
    auto type = make_type(TypeKind::product, form.location);
    for (std::size_t index = 1; index < list.elements.size(); ++index) {
      type->fields.push_back(parse_field(element(list, index), "product"));
    }
    return type;
  }

  if (head->name == "sum") {
    auto type = make_type(TypeKind::sum, form.location);
    for (std::size_t index = 1; index < list.elements.size(); ++index) {
      type->alternatives.push_back(parse_sum_alternative(element(list, index)));
    }
    return type;
  }

  if (head->name == "union") {
    auto type = make_type(TypeKind::union_, form.location);
    for (std::size_t index = 1; index < list.elements.size(); ++index) {
      type->fields.push_back(parse_field(element(list, index), "union"));
    }
    return type;
  }

  auto type = make_type(TypeKind::application, form.location);
  type->name = head->name;
  for (std::size_t index = 1; index < list.elements.size(); ++index) {
    type->arguments.push_back(parse_type(element(list, index)));
  }
  return type;
}

}  // namespace

TypeError::TypeError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.message), diagnostic_(std::move(diagnostic)) {}

const Diagnostic& TypeError::diagnostic() const {
  return diagnostic_;
}

TypePtr parse_type(const Form& form) {
  if (const auto* symbol = as_symbol(form)) {
    bool found = false;
    const auto primitive = primitive_from_name(symbol->name, found);
    if (found) {
      auto type = make_type(TypeKind::primitive, form.location);
      type->primitive = primitive;
      return type;
    }

    auto type = make_type(TypeKind::name, form.location);
    type->name = symbol->name;
    return type;
  }

  if (const auto* list = as_list(form)) {
    return parse_type_list(form, *list);
  }

  fail(form.location, "expected type expression");
}

TypeDeclaration parse_type_declaration(const Form& form) {
  const auto* list = as_list(form);
  if (list == nullptr || list->elements.empty()) {
    fail(form.location, "type declaration must be a list");
  }

  const auto head = require_symbol_name(element(*list, 0), "type declaration head must be a symbol");
  if (head != "type") {
    fail(element(*list, 0).location, "expected type declaration");
  }

  if (list->elements.size() != 3 && list->elements.size() != 4) {
    fail(form.location, "type declaration expects either 3 or 4 forms");
  }

  TypeDeclaration declaration{
      require_symbol_name(element(*list, 1), "type declaration name must be a symbol"),
      element(*list, 1).location,
      {},
      nullptr,
  };

  std::size_t body_index = 2;
  if (list->elements.size() == 4) {
    const auto* parameters = as_list(element(*list, 2));
    if (parameters == nullptr) {
      fail(element(*list, 2).location, "type parameters must be a list");
    }

    std::unordered_set<std::string> parameter_names;
    for (const auto& parameter : parameters->elements) {
      auto name = require_symbol_name(*parameter, "type parameter must be a symbol");
      if (!parameter_names.insert(name).second) {
        fail(parameter->location, "type parameter redefines existing parameter");
      }
      declaration.parameters.push_back(TypeParameter{std::move(name), parameter->location});
    }
    body_index = 3;
  }

  declaration.body = parse_type(element(*list, body_index));
  return declaration;
}

void TypeEnvironment::declare(TypeDeclaration declaration) {
  if (declaration_indexes_.contains(declaration.name)) {
    fail(declaration.location, "type declaration redefines existing type");
  }

  const auto index = declarations_.size();
  declaration_indexes_.emplace(declaration.name, index);
  declarations_.push_back(std::move(declaration));
}

const TypeDeclaration* TypeEnvironment::find(std::string_view name) const {
  const auto found = declaration_indexes_.find(std::string(name));
  if (found == declaration_indexes_.end()) {
    return nullptr;
  }
  return &declarations_[found->second];
}

const std::vector<TypeDeclaration>& TypeEnvironment::declarations() const {
  return declarations_;
}

std::size_t TypeEnvironment::size() const {
  return declarations_.size();
}

TypePtr clone_type(const Type& type) {
  auto cloned = make_type(type.kind, type.location);
  cloned->primitive = type.primitive;
  cloned->name = type.name;
  cloned->array_size = type.array_size;

  if (type.element) {
    cloned->element = clone_type(*type.element);
  }
  if (type.result) {
    cloned->result = clone_type(*type.result);
  }
  cloned->arguments = clone_type_vector(type.arguments);

  cloned->fields.reserve(type.fields.size());
  for (const auto& field : type.fields) {
    cloned->fields.push_back(Field{field.name, clone_type(*field.type)});
  }

  cloned->alternatives.reserve(type.alternatives.size());
  for (const auto& alternative : type.alternatives) {
    cloned->alternatives.push_back(SumAlternative{
        alternative.name,
        clone_type_vector(alternative.payload),
    });
  }

  return cloned;
}

namespace {

TypePtr substitute_type_impl(const Type& type, const std::unordered_map<std::string, const Type*>& substitutions) {
  if (type.kind == TypeKind::name) {
    const auto found = substitutions.find(type.name);
    if (found != substitutions.end()) {
      return clone_type(*found->second);
    }
  }

  auto substituted = make_type(type.kind, type.location);
  substituted->primitive = type.primitive;
  substituted->name = type.name;
  substituted->array_size = type.array_size;

  if (type.element) {
    substituted->element = substitute_type_impl(*type.element, substitutions);
  }
  if (type.result) {
    substituted->result = substitute_type_impl(*type.result, substitutions);
  }
  substituted->arguments = substitute_type_vector(type.arguments, substitutions);

  substituted->fields.reserve(type.fields.size());
  for (const auto& field : type.fields) {
    substituted->fields.push_back(Field{field.name, substitute_type_impl(*field.type, substitutions)});
  }

  substituted->alternatives.reserve(type.alternatives.size());
  for (const auto& alternative : type.alternatives) {
    substituted->alternatives.push_back(SumAlternative{
        alternative.name,
        substitute_type_vector(alternative.payload, substitutions),
    });
  }

  return substituted;
}

}  // namespace

TypePtr instantiate_type_application(const Type& application, const TypeEnvironment& environment) {
  if (application.kind != TypeKind::application) {
    fail(application.location, "expected type application");
  }

  const auto* declaration = environment.find(application.name);
  if (declaration == nullptr) {
    fail(application.location, "unknown generic type name");
  }
  if (declaration->parameters.empty()) {
    fail(application.location, "type does not accept type arguments");
  }
  if (declaration->parameters.size() != application.arguments.size()) {
    fail(application.location, "generic type argument count mismatch");
  }

  std::unordered_map<std::string, const Type*> substitutions;
  for (std::size_t index = 0; index < declaration->parameters.size(); ++index) {
    substitutions.emplace(declaration->parameters[index].name, application.arguments[index].get());
  }

  return substitute_type_impl(*declaration->body, substitutions);
}

}  // namespace termis
