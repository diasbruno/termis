#include <cstdlib>
#include <iostream>
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
  }

  std::cerr << "termisc: no compiler pipeline is implemented yet\n";
  return EXIT_FAILURE;
}
