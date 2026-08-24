#include "layout.hpp"
#include "reader.hpp"
#include "semantic.hpp"
#include "type.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view version = "0.1.0";

void print_help(std::ostream& out) {
  out << "Usage: termisc [OPTIONS] <input.termis>\n"
      << "\n"
      << "Options:\n"
      << "  -h, --help       Show this help message\n"
      << "  --version        Show compiler version\n";
}

void print_version(std::ostream& out) {
  out << "termisc " << version << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 1) {
    print_help(std::cerr);
    return EXIT_FAILURE;
  }

  std::string_view input_path;

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

    std::cout << "parsed " << forms.size() << " top-level form";
    if (forms.size() != 1) {
      std::cout << 's';
    }
    std::cout << ", recognized " << program.forms.size() << " semantic form";
    if (program.forms.size() != 1) {
      std::cout << 's';
    }
    std::cout << ", collected " << program.types.size() << " type declaration";
    if (program.types.size() != 1) {
      std::cout << 's';
    }
    std::cout << ", computed " << concrete_layouts << " concrete layout";
    if (concrete_layouts != 1) {
      std::cout << 's';
    }
    std::cout << '\n';
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
  }

  return EXIT_SUCCESS;
}
