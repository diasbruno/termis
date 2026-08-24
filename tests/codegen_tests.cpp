#include "codegen.hpp"
#include "reader.hpp"
#include "semantic.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "codegen test failed: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

std::string emit(std::string_view source) {
  auto forms = termis::read_forms(source);
  auto program = termis::analyze_forms(forms);
  return termis::emit_llvm_ir(program);
}

void contains(std::string_view haystack, std::string_view needle) {
  require(haystack.find(needle) != std::string_view::npos, "expected emitted LLVM fragment");
}

void emits_arithmetic_function() {
  const auto ir = emit("(fn add ((a i64) (b i64)) i64 (+ a b))");

  contains(ir, "define i64 @add(i64 %a, i64 %b)");
  contains(ir, "add i64 %a, %b");
  contains(ir, "ret i64");
}

void emits_let_do_and_unit() {
  const auto ir = emit(R"(
    (fn log () unit
      (do
        .
        .))
    (fn value () i64
      (let ((x 40)
            (y 2))
        (+ x y)))
  )");

  contains(ir, "define void @log()");
  contains(ir, "ret void");
  contains(ir, "define i64 @value()");
  contains(ir, "add i64 40, 2");
}

void emits_comparison_function() {
  const auto ir = emit("(fn greater ((a i64) (b i64)) bool (> a b))");

  contains(ir, "icmp sgt i64 %a, %b");
  contains(ir, "ret i1");
}

void emits_function_calls() {
  const auto ir = emit(R"(
    (fn add ((a i64) (b i64)) i64 (+ a b))
    (fn main () i64 (add 10 32))
  )");

  contains(ir, "call i64 @add(i64 10, i64 32)");
}

void emits_primitive_type_aliases() {
  const auto ir = emit(R"(
    (type UserId u64)
    (fn identity ((id UserId)) UserId id)
  )");

  contains(ir, "define i64 @identity(i64 %id)");
  contains(ir, "ret i64 %id");
}

void rejects_type_mismatch() {
  try {
    (void)emit("(fn bad () i64 true)");
  } catch (const termis::CodegenError& error) {
    require(error.diagnostic().message == "expression type mismatch",
            "expected type mismatch diagnostic");
    return;
  }

  require(false, "expected codegen failure");
}

}  // namespace

int main() {
  emits_arithmetic_function();
  emits_let_do_and_unit();
  emits_comparison_function();
  emits_function_calls();
  emits_primitive_type_aliases();
  rejects_type_mismatch();
}
