#include "codegen.hpp"

#include <sstream>
#include <unordered_map>
#include <utility>

namespace termis {
namespace {

struct Value {
  std::string llvm_type;
  std::string ref;
};

struct FunctionSignature {
  std::string name;
  std::vector<std::pair<std::string, std::string>> parameters;
  std::string return_type;
};

const Symbol* as_symbol(const Form& form) {
  return std::get_if<Symbol>(&form.kind);
}

const List* as_list(const Form& form) {
  return std::get_if<List>(&form.kind);
}

const IntegerLiteral* as_integer(const Form& form) {
  return std::get_if<IntegerLiteral>(&form.kind);
}

bool is_unit(const Form& form) {
  return std::holds_alternative<UnitLiteral>(form.kind);
}

const Form& element(const List& list, std::size_t index) {
  return *list.elements[index];
}

[[noreturn]] void fail(SourceLocation location, std::string message) {
  throw CodegenError(Diagnostic{location, std::move(message)});
}

std::string symbol_name(const Form& form, std::string message) {
  const auto* symbol = as_symbol(form);
  if (symbol == nullptr) {
    fail(form.location, std::move(message));
  }
  return symbol->name;
}

std::string primitive_llvm_type(PrimitiveType primitive, SourceLocation location) {
  switch (primitive) {
    case PrimitiveType::bool_:
      return "i1";
    case PrimitiveType::i8:
    case PrimitiveType::u8:
      return "i8";
    case PrimitiveType::i16:
    case PrimitiveType::u16:
      return "i16";
    case PrimitiveType::i32:
    case PrimitiveType::u32:
      return "i32";
    case PrimitiveType::i64:
    case PrimitiveType::u64:
      return "i64";
    case PrimitiveType::unit:
    case PrimitiveType::void_:
      return "void";
    case PrimitiveType::f32:
    case PrimitiveType::f64:
      fail(location, "LLVM emission does not support floating-point values yet");
  }
}

std::string llvm_type_for(const Form& form, const TypeEnvironment& types) {
  const auto type = parse_type(form);
  if (type->kind == TypeKind::primitive) {
    return primitive_llvm_type(type->primitive, form.location);
  }
  if (type->kind == TypeKind::name) {
    const auto* declaration = types.find(type->name);
    if (declaration == nullptr) {
      fail(form.location, "unknown type name");
    }
    if (!declaration->parameters.empty()) {
      fail(form.location, "generic type requires type arguments");
    }
    if (declaration->body->kind != TypeKind::primitive) {
      fail(form.location, "LLVM emission currently supports only primitive type aliases in function signatures");
    }
    return primitive_llvm_type(declaration->body->primitive, form.location);
  }
  fail(form.location, "LLVM emission currently supports only primitive types in function signatures");
}

FunctionSignature parse_signature(const Form& form, const TypeEnvironment& types) {
  const auto* list = as_list(form);
  if (list == nullptr || list->elements.size() < 5) {
    fail(form.location, "function declaration requires name, parameters, return type, and body");
  }
  if (symbol_name(element(*list, 0), "function head must be a symbol") != "fn") {
    fail(element(*list, 0).location, "expected function declaration");
  }

  FunctionSignature signature;
  signature.name = symbol_name(element(*list, 1), "function name must be a symbol");
  signature.return_type = llvm_type_for(element(*list, 3), types);

  const auto* parameters = as_list(element(*list, 2));
  if (parameters == nullptr) {
    fail(element(*list, 2).location, "function parameters must be a list");
  }
  for (const auto& parameter_form : parameters->elements) {
    const auto* parameter = as_list(*parameter_form);
    if (parameter == nullptr || parameter->elements.size() != 2) {
      fail(parameter_form->location, "function parameter requires name and type");
    }
    signature.parameters.push_back({
        symbol_name(element(*parameter, 0), "function parameter name must be a symbol"),
        llvm_type_for(element(*parameter, 1), types),
    });
  }

  return signature;
}

class FunctionEmitter {
 public:
  FunctionEmitter(const std::unordered_map<std::string, FunctionSignature>& functions,
                  FunctionSignature signature)
      : functions_(functions), signature_(std::move(signature)) {}

  std::string emit(const Form& declaration) {
    const auto* list = as_list(declaration);
    out_ << "define " << signature_.return_type << " @" << signature_.name << '(';
    for (std::size_t index = 0; index < signature_.parameters.size(); ++index) {
      if (index != 0) {
        out_ << ", ";
      }
      out_ << signature_.parameters[index].second << " %" << signature_.parameters[index].first;
      variables_.emplace(signature_.parameters[index].first,
                         Value{signature_.parameters[index].second, "%" + signature_.parameters[index].first});
    }
    out_ << ") {\nentry:\n";

    Value result{"void", ""};
    for (std::size_t index = 4; index < list->elements.size(); ++index) {
      result = emit_expression(element(*list, index));
    }

    if (signature_.return_type == "void") {
      out_ << "  ret void\n";
    } else {
      require_type(result, signature_.return_type, declaration.location);
      out_ << "  ret " << signature_.return_type << ' ' << result.ref << "\n";
    }
    out_ << "}\n";
    return out_.str();
  }

 private:
  std::string next_temp() {
    return "%t" + std::to_string(next_temp_++);
  }

  Value emit_expression(const Form& form) {
    if (const auto* integer = as_integer(form)) {
      return Value{"i64", std::to_string(integer->value)};
    }
    if (is_unit(form)) {
      return Value{"void", ""};
    }
    if (const auto* symbol = as_symbol(form)) {
      if (symbol->name == "true") {
        return Value{"i1", "1"};
      }
      if (symbol->name == "false") {
        return Value{"i1", "0"};
      }
      const auto found = variables_.find(symbol->name);
      if (found == variables_.end()) {
        fail(form.location, "unknown local or parameter");
      }
      return found->second;
    }

    const auto* list = as_list(form);
    if (list == nullptr || list->elements.empty()) {
      fail(form.location, "expected expression");
    }

    const auto head = symbol_name(element(*list, 0), "expression head must be a symbol");
    if (head == "let") {
      return emit_let(form, *list);
    }
    if (head == "do") {
      return emit_do(form, *list);
    }
    if (head == "+" || head == "-" || head == "*" || head == "/") {
      return emit_arithmetic(form, *list, head);
    }
    if (head == "=" || head == "!=" || head == "<" || head == "<=" || head == ">" || head == ">=") {
      return emit_comparison(form, *list, head);
    }
    return emit_call(form, *list, head);
  }

  Value emit_let(const Form& form, const List& list) {
    if (list.elements.size() < 3) {
      fail(form.location, "let expression requires bindings and a body");
    }
    const auto* bindings = as_list(element(list, 1));
    if (bindings == nullptr) {
      fail(element(list, 1).location, "let bindings must be a list");
    }

    auto previous = variables_;
    for (const auto& binding_form : bindings->elements) {
      const auto* binding = as_list(*binding_form);
      if (binding == nullptr || binding->elements.size() != 2) {
        fail(binding_form->location, "let binding requires name and value");
      }
      const auto name = symbol_name(element(*binding, 0), "let binding name must be a symbol");
      variables_[name] = emit_expression(element(*binding, 1));
    }

    Value result{"void", ""};
    for (std::size_t index = 2; index < list.elements.size(); ++index) {
      result = emit_expression(element(list, index));
    }
    variables_ = std::move(previous);
    return result;
  }

  Value emit_do(const Form& form, const List& list) {
    if (list.elements.size() < 2) {
      fail(form.location, "do expression requires at least one body form");
    }
    Value result{"void", ""};
    for (std::size_t index = 1; index < list.elements.size(); ++index) {
      result = emit_expression(element(list, index));
    }
    return result;
  }

  Value emit_arithmetic(const Form& form, const List& list, std::string_view op) {
    if (list.elements.size() != 3) {
      fail(form.location, "arithmetic expression requires two operands");
    }
    const auto left = emit_expression(element(list, 1));
    const auto right = emit_expression(element(list, 2));
    require_type(right, left.llvm_type, element(list, 2).location);
    if (left.llvm_type != "i64" && left.llvm_type != "i32") {
      fail(form.location, "arithmetic currently supports i32 and i64 values");
    }

    std::string instruction;
    if (op == "+") {
      instruction = "add";
    } else if (op == "-") {
      instruction = "sub";
    } else if (op == "*") {
      instruction = "mul";
    } else {
      instruction = "sdiv";
    }
    const auto temp = next_temp();
    out_ << "  " << temp << " = " << instruction << ' ' << left.llvm_type << ' ' << left.ref << ", "
         << right.ref << "\n";
    return Value{left.llvm_type, temp};
  }

  Value emit_comparison(const Form& form, const List& list, std::string_view op) {
    if (list.elements.size() != 3) {
      fail(form.location, "comparison expression requires two operands");
    }
    const auto left = emit_expression(element(list, 1));
    const auto right = emit_expression(element(list, 2));
    require_type(right, left.llvm_type, element(list, 2).location);
    std::string predicate;
    if (op == "=") {
      predicate = "eq";
    } else if (op == "!=") {
      predicate = "ne";
    } else if (op == "<") {
      predicate = "slt";
    } else if (op == "<=") {
      predicate = "sle";
    } else if (op == ">") {
      predicate = "sgt";
    } else {
      predicate = "sge";
    }
    const auto temp = next_temp();
    out_ << "  " << temp << " = icmp " << predicate << ' ' << left.llvm_type << ' ' << left.ref << ", "
         << right.ref << "\n";
    return Value{"i1", temp};
  }

  Value emit_call(const Form& form, const List& list, const std::string& name) {
    const auto found = functions_.find(name);
    if (found == functions_.end()) {
      fail(form.location, "unknown function");
    }
    const auto& signature = found->second;
    if (list.elements.size() - 1 != signature.parameters.size()) {
      fail(form.location, "function call argument count mismatch");
    }

    std::vector<Value> arguments;
    for (std::size_t index = 1; index < list.elements.size(); ++index) {
      auto argument = emit_expression(element(list, index));
      require_type(argument, signature.parameters[index - 1].second, element(list, index).location);
      arguments.push_back(std::move(argument));
    }

    const auto temp = signature.return_type == "void" ? "" : next_temp();
    out_ << "  ";
    if (signature.return_type != "void") {
      out_ << temp << " = ";
    }
    out_ << "call " << signature.return_type << " @" << name << '(';
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      if (index != 0) {
        out_ << ", ";
      }
      out_ << arguments[index].llvm_type << ' ' << arguments[index].ref;
    }
    out_ << ")\n";
    return Value{signature.return_type, temp};
  }

  void require_type(const Value& value, std::string_view expected, SourceLocation location) const {
    if (value.llvm_type != expected) {
      fail(location, "expression type mismatch");
    }
  }

  const std::unordered_map<std::string, FunctionSignature>& functions_;
  FunctionSignature signature_;
  std::unordered_map<std::string, Value> variables_;
  std::ostringstream out_;
  std::size_t next_temp_ = 0;
};

}  // namespace

CodegenError::CodegenError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.message), diagnostic_(std::move(diagnostic)) {}

const Diagnostic& CodegenError::diagnostic() const {
  return diagnostic_;
}

std::string emit_llvm_ir(const Program& program) {
  std::unordered_map<std::string, FunctionSignature> functions;
  for (const auto& node : program.forms) {
    if (node->kind == SemanticKind::function_declaration) {
      auto signature = parse_signature(*node->form, program.types);
      if (functions.contains(signature.name)) {
        fail(node->form->location, "function redefines existing function");
      }
      functions.emplace(signature.name, std::move(signature));
    }
  }

  std::ostringstream out;
  out << "; ModuleID = 'termis'\n";
  for (const auto& node : program.forms) {
    if (node->kind == SemanticKind::function_declaration) {
      FunctionEmitter emitter(functions, functions.at(parse_signature(*node->form, program.types).name));
      out << emitter.emit(*node->form) << '\n';
    } else if (node->kind != SemanticKind::type_declaration) {
      fail(node->form->location, "LLVM emission currently supports only type and function declarations");
    }
  }
  return out.str();
}

}  // namespace termis
