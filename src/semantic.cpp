#include "semantic.hpp"
#include <utility>

namespace termis {
namespace {

const Symbol* as_symbol(const Form& form) {
  return std::get_if<Symbol>(&form.kind);
}

const List* as_list(const Form& form) {
  return std::get_if<List>(&form.kind);
}

std::size_t element_count(const Form& form) {
  if (const auto* list = as_list(form)) {
    return list->elements.size();
  }
  return 0;
}

const Form& list_element(const Form& form, std::size_t index) {
  return *as_list(form)->elements[index];
}

void require_count_at_least(const Form& form, std::size_t minimum, std::string message) {
  if (element_count(form) < minimum) {
    throw SemanticError(Diagnostic{form.location, std::move(message)});
  }
}

void require_exact_count(const Form& form, std::size_t expected, std::string message) {
  if (element_count(form) != expected) {
    throw SemanticError(Diagnostic{form.location, std::move(message)});
  }
}

void require_symbol(const Form& form, std::string message) {
  if (as_symbol(form) == nullptr) {
    throw SemanticError(Diagnostic{form.location, std::move(message)});
  }
}

void require_list(const Form& form, std::string message) {
  if (as_list(form) == nullptr) {
    throw SemanticError(Diagnostic{form.location, std::move(message)});
  }
}

void require_optional_string(const Form& form, std::string message) {
  if (!std::holds_alternative<StringLiteral>(form.kind)) {
    throw SemanticError(Diagnostic{form.location, std::move(message)});
  }
}

SemanticKind classify(const Form& form) {
  const auto* list = as_list(form);
  if (list == nullptr) {
    return SemanticKind::atom;
  }

  if (list->elements.empty()) {
    return SemanticKind::list_expression;
  }

  const auto* head = as_symbol(*list->elements.front());
  if (head == nullptr) {
    return SemanticKind::list_expression;
  }

  if (head->name == "list") {
    return SemanticKind::list_expression;
  }

  if (head->name == "type") {
    require_count_at_least(form, 3, "type declaration requires a name and body");
    require_symbol(list_element(form, 1), "type declaration name must be a symbol");
    if (element_count(form) == 4) {
      require_list(list_element(form, 2), "type parameters must be a list");
      (void)parse_type(list_element(form, 3));
    } else {
      require_exact_count(form, 3, "type declaration expects either 3 or 4 forms");
      (void)parse_type(list_element(form, 2));
    }
    return SemanticKind::type_declaration;
  }

  if (head->name == "fn") {
    require_count_at_least(form, 5, "function declaration requires name, parameters, return type, and body");
    require_symbol(list_element(form, 1), "function name must be a symbol");
    require_list(list_element(form, 2), "function parameters must be a list");
    return SemanticKind::function_declaration;
  }

  if (head->name == "extern") {
    require_count_at_least(form, 5, "extern function declaration requires fn, name, parameters, and return type");
    if (element_count(form) != 5 && element_count(form) != 6) {
      throw SemanticError(Diagnostic{form.location, "extern function declaration expects 5 or 6 forms"});
    }
    const auto* extern_kind = as_symbol(list_element(form, 1));
    if (extern_kind == nullptr || extern_kind->name != "fn") {
      throw SemanticError(Diagnostic{list_element(form, 1).location,
                                     "extern declaration currently supports only fn"});
    }
    require_symbol(list_element(form, 2), "extern function name must be a symbol");
    require_list(list_element(form, 3), "extern function parameters must be a list");
    if (element_count(form) == 6) {
      require_optional_string(list_element(form, 5), "extern function link name must be a string");
    }
    return SemanticKind::extern_function_declaration;
  }

  if (head->name == "let") {
    require_count_at_least(form, 3, "let expression requires bindings and a body");
    require_list(list_element(form, 1), "let bindings must be a list");
    return SemanticKind::let_expression;
  }

  if (head->name == "do") {
    require_count_at_least(form, 2, "do expression requires at least one body form");
    return SemanticKind::do_expression;
  }

  if (head->name == "match") {
    require_count_at_least(form, 3, "match expression requires a value and at least one arm");
    return SemanticKind::match_expression;
  }

  if (head->name == "const") {
    require_exact_count(form, 3, "const declaration requires a name and value");
    require_symbol(list_element(form, 1), "const name must be a symbol");
    return SemanticKind::const_declaration;
  }

  return SemanticKind::application;
}

SemanticNodePtr analyze_form(const Form& form) {
  return std::make_unique<SemanticNode>(SemanticNode{classify(form), &form});
}

}  // namespace

SemanticError::SemanticError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.message), diagnostic_(std::move(diagnostic)) {}

const Diagnostic& SemanticError::diagnostic() const {
  return diagnostic_;
}

Program analyze_forms(const std::vector<FormPtr>& forms) {
  Program program;
  program.forms.reserve(forms.size());
  for (const auto& form : forms) {
    auto node = analyze_form(*form);
    if (node->kind == SemanticKind::type_declaration) {
      program.types.declare(parse_type_declaration(*form));
    }
    program.forms.push_back(std::move(node));
  }
  validate_type_references(program.types);
  return program;
}

}  // namespace termis
