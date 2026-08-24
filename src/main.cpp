#include "codegen.hpp"
#include "layout.hpp"
#include "reader.hpp"
#include "semantic.hpp"
#include "type.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view version = "0.1.0";

void print_help(std::ostream& out) {
  out << "Usage: termisc [OPTIONS] <input.termis>\n"
      << "\n"
      << "Options:\n"
      << "  -h, --help           Show this help message\n"
      << "  -o, --output <path>  Write the compiled binary to path\n"
      << "  --dump-llvm          Emit LLVM IR to stdout after validation\n"
      << "  --version            Show compiler version\n";
}

void print_version(std::ostream& out) {
  out << "termisc " << version << '\n';
}

std::string shell_quote(std::string_view value) {
  std::string quoted = "'";
  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += "'";
  return quoted;
}

std::filesystem::path make_temporary_ir_path() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory = std::filesystem::temp_directory_path();
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto path = directory / ("termis-" + std::to_string(now) + "-" +
                             std::to_string(attempt) + ".ll");
    if (!std::filesystem::exists(path)) {
      return path;
    }
  }
  return directory / ("termis-" + std::to_string(now) + ".ll");
}

bool compile_ir_to_binary(std::string_view ir,
                          const std::filesystem::path& output_path,
                          std::ostream& err) {
  const auto ir_path = make_temporary_ir_path();
  {
    std::ofstream ir_file{ir_path};
    if (!ir_file) {
      err << "termisc: unable to write temporary LLVM IR: " << ir_path << '\n';
      return false;
    }
    ir_file << ir;
  }

  const auto command = "clang++ " + shell_quote(ir_path.string()) + " -o " +
                       shell_quote(output_path.string());
  const int status = std::system(command.c_str());

  std::error_code remove_error;
  std::filesystem::remove(ir_path, remove_error);

  if (status != 0) {
    err << "termisc: LLVM compiler failed while producing " << output_path << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) {
    print_help(std::cerr);
    return EXIT_FAILURE;
  }

  std::string_view input_path;
  std::optional<std::filesystem::path> output_path;
  bool dump_llvm = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);

    if (arg == "-h" || arg == "--help") {
      print_help(std::cout);
      return EXIT_SUCCESS;
    }

    if (arg == "--version") {
      print_version(std::cout);
      return EXIT_SUCCESS;
    }

    if (arg == "--dump-llvm") {
      dump_llvm = true;
      continue;
    }

    if (arg == "-o" || arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "termisc: " << arg << " requires a path\n";
        return EXIT_FAILURE;
      }
      output_path = argv[++i];
      continue;
    }

    if (!arg.empty() && arg.front() == '-') {
      std::cerr << "termisc: unknown option: " << arg << '\n';
      return EXIT_FAILURE;
    }

    if (!input_path.empty()) {
      std::cerr << "termisc: expected one input file\n";
      return EXIT_FAILURE;
    }

    input_path = arg;
  }

  if (input_path.empty()) {
    std::cerr << "termisc: expected an input file\n";
    return EXIT_FAILURE;
  }

  std::ifstream input{std::string(input_path)};
  if (!input) {
    std::cerr << "termisc: unable to open input file: " << input_path << '\n';
    return EXIT_FAILURE;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  try {
    const auto forms = termis::read_forms(buffer.str());
    const auto program = termis::analyze_forms(forms);
    termis::LayoutEngine layout_engine(program.types);
    std::size_t concrete_layouts = 0;
    for (const auto& declaration : program.types.declarations()) {
      if (declaration.parameters.empty()) {
        (void)layout_engine.compute(*declaration.body);
        ++concrete_layouts;
      }
    }

    if (dump_llvm) {
      std::cout << termis::emit_llvm_ir(program);
    } else {
      (void)forms;
      (void)concrete_layouts;
      const auto ir = termis::emit_llvm_ir(program);
      const auto binary_path = output_path.value_or("a.out");
      if (!compile_ir_to_binary(ir, binary_path, std::cerr)) {
        return EXIT_FAILURE;
      }
      std::cout << "wrote " << binary_path << '\n';
    }
  } catch (const termis::ReadError& error) {
    const auto& diagnostic = error.diagnostic();
    std::cerr << input_path << ':' << diagnostic.location.line << ':'
              << diagnostic.location.column << ": reader error: " << diagnostic.message << '\n';
    return EXIT_FAILURE;
  } catch (const termis::SemanticError& error) {
    const auto& diagnostic = error.diagnostic();
    std::cerr << input_path << ':' << diagnostic.location.line << ':'
              << diagnostic.location.column << ": semantic error: " << diagnostic.message << '\n';
    return EXIT_FAILURE;
  } catch (const termis::TypeError& error) {
    const auto& diagnostic = error.diagnostic();
    std::cerr << input_path << ':' << diagnostic.location.line << ':'
              << diagnostic.location.column << ": type error: " << diagnostic.message << '\n';
    return EXIT_FAILURE;
  } catch (const termis::LayoutError& error) {
    const auto& diagnostic = error.diagnostic();
    std::cerr << input_path << ':' << diagnostic.location.line << ':'
              << diagnostic.location.column << ": layout error: " << diagnostic.message << '\n';
    return EXIT_FAILURE;
  } catch (const termis::CodegenError& error) {
    const auto& diagnostic = error.diagnostic();
    std::cerr << input_path << ':' << diagnostic.location.line << ':'
              << diagnostic.location.column << ": codegen error: " << diagnostic.message << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
