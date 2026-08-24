#include "reader.hpp"

#include <cerrno>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace termis {
namespace {

enum class TokenKind {
  left_paren,
  right_paren,
  symbol,
  integer,
  floating,
  string,
  dot,
  end,
};

struct Token {
  TokenKind kind = TokenKind::end;
  SourceLocation location;
  std::string text;
};

class Lexer {
 public:
  explicit Lexer(std::string_view source) : source_(source) {}

  Token next() {
    skip_whitespace();

    const auto start = location_;
    if (is_at_end()) {
      return Token{TokenKind::end, start, ""};
    }

    const char current = peek();
    if (current == '(') {
      advance();
      return Token{TokenKind::left_paren, start, "("};
    }
    if (current == ')') {
      advance();
      return Token{TokenKind::right_paren, start, ")"};
    }
    if (current == '.') {
      if (offset_ + 1 < source_.size() && std::isdigit(as_unsigned(source_[offset_ + 1]))) {
        fail(start, "floating-point literals must start with a digit");
      }
      advance();
      return Token{TokenKind::dot, start, "."};
    }
    if (current == '"') {
      return read_string();
    }
    if (is_number_start(current)) {
      return read_number();
    }
    return read_symbol();
  }

 private:
  static unsigned char as_unsigned(char value) {
    return static_cast<unsigned char>(value);
  }

  bool is_at_end() const {
    return offset_ >= source_.size();
  }

  char peek() const {
    return source_[offset_];
  }

  char advance() {
    const char value = source_[offset_++];
    ++location_.offset;
    if (value == '\n') {
      ++location_.line;
      location_.column = 1;
    } else {
      ++location_.column;
    }
    return value;
  }

  void skip_whitespace() {
    while (!is_at_end() && std::isspace(as_unsigned(peek()))) {
      advance();
    }
  }

  bool is_delimiter(char value) const {
    return std::isspace(as_unsigned(value)) || value == '(' || value == ')' || value == '"';
  }

  bool is_number_start(char value) const {
    if (std::isdigit(as_unsigned(value))) {
      return true;
    }
    if ((value == '-' || value == '+') && offset_ + 1 < source_.size()) {
      return std::isdigit(as_unsigned(source_[offset_ + 1]));
    }
    return false;
  }

  Token read_number() {
    const auto start = location_;
    std::string text;

    if (peek() == '-' || peek() == '+') {
      text.push_back(advance());
    }

    while (!is_at_end() && std::isdigit(as_unsigned(peek()))) {
      text.push_back(advance());
    }

    bool is_float = false;
    if (!is_at_end() && peek() == '.') {
      is_float = true;
      text.push_back(advance());
      if (is_at_end() || !std::isdigit(as_unsigned(peek()))) {
        fail(start, "floating-point literal requires digits after decimal point");
      }
      while (!is_at_end() && std::isdigit(as_unsigned(peek()))) {
        text.push_back(advance());
      }
    }

    if (!is_at_end() && !is_delimiter(peek())) {
      fail(start, "invalid numeric literal");
    }

    return Token{is_float ? TokenKind::floating : TokenKind::integer, start, std::move(text)};
  }

  Token read_string() {
    const auto start = location_;
    std::string value;
    advance();

    while (!is_at_end()) {
      const char current = advance();
      if (current == '"') {
        return Token{TokenKind::string, start, std::move(value)};
      }
      if (current == '\\') {
        if (is_at_end()) {
          fail(start, "unterminated string literal");
        }
        const char escaped = advance();
        switch (escaped) {
          case 'n':
            value.push_back('\n');
            break;
          case 't':
            value.push_back('\t');
            break;
          case '"':
            value.push_back('"');
            break;
          case '\\':
            value.push_back('\\');
            break;
          default:
            fail(location_, "unsupported string escape");
        }
      } else {
        value.push_back(current);
      }
    }

    fail(start, "unterminated string literal");
  }

  Token read_symbol() {
    const auto start = location_;
    std::string text;
    while (!is_at_end() && !is_delimiter(peek())) {
      if (peek() == '.') {
        fail(location_, "'.' is reserved for the unit literal");
      }
      text.push_back(advance());
    }
    return Token{TokenKind::symbol, start, std::move(text)};
  }

  [[noreturn]] void fail(SourceLocation location, std::string message) const {
    throw ReadError(Diagnostic{location, std::move(message)});
  }

  std::string_view source_;
  std::size_t offset_ = 0;
  SourceLocation location_;
};

class Parser {
 public:
  explicit Parser(std::string_view source) : lexer_(source) {
    advance();
  }

  std::vector<FormPtr> parse_all() {
    std::vector<FormPtr> forms;
    while (current_.kind != TokenKind::end) {
      if (current_.kind == TokenKind::right_paren) {
        fail(current_.location, "unexpected ')'");
      }
      forms.push_back(parse_form());
    }
    return forms;
  }

 private:
  void advance() {
    current_ = lexer_.next();
  }

  FormPtr parse_form() {
    const auto location = current_.location;
    switch (current_.kind) {
      case TokenKind::left_paren:
        return parse_list();
      case TokenKind::symbol: {
        auto form = std::make_unique<Form>();
        form->location = location;
        form->kind = Symbol{std::move(current_.text)};
        advance();
        return form;
      }
      case TokenKind::integer:
        return parse_integer();
      case TokenKind::floating:
        return parse_float();
      case TokenKind::string: {
        auto form = std::make_unique<Form>();
        form->location = location;
        form->kind = StringLiteral{std::move(current_.text)};
        advance();
        return form;
      }
      case TokenKind::dot: {
        auto form = std::make_unique<Form>();
        form->location = location;
        form->kind = UnitLiteral{};
        advance();
        return form;
      }
      case TokenKind::right_paren:
        fail(location, "unexpected ')'");
      case TokenKind::end:
        fail(location, "unexpected end of input");
    }
  }

  FormPtr parse_list() {
    auto form = std::make_unique<Form>();
    form->location = current_.location;
    form->kind = List{};
    advance();

    auto& elements = std::get<List>(form->kind).elements;
    while (current_.kind != TokenKind::right_paren) {
      if (current_.kind == TokenKind::end) {
        fail(form->location, "unterminated list");
      }
      elements.push_back(parse_form());
    }

    advance();
    return form;
  }

  FormPtr parse_integer() {
    auto form = std::make_unique<Form>();
    form->location = current_.location;

    std::int64_t value = 0;
    const auto result =
        std::from_chars(current_.text.data(), current_.text.data() + current_.text.size(), value);
    if (result.ec != std::errc{} || result.ptr != current_.text.data() + current_.text.size()) {
      fail(current_.location, "integer literal is out of range");
    }

    form->kind = IntegerLiteral{value};
    advance();
    return form;
  }

  FormPtr parse_float() {
    auto form = std::make_unique<Form>();
    form->location = current_.location;

    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(current_.text.c_str(), &end);
    if (errno == ERANGE || end != current_.text.c_str() + current_.text.size()) {
      fail(current_.location, "invalid floating-point literal");
    }

    form->kind = FloatLiteral{value};
    advance();
    return form;
  }

  [[noreturn]] void fail(SourceLocation location, std::string message) const {
    throw ReadError(Diagnostic{location, std::move(message)});
  }

  Lexer lexer_;
  Token current_;
};

}  // namespace

ReadError::ReadError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.message), diagnostic_(std::move(diagnostic)) {}

const Diagnostic& ReadError::diagnostic() const {
  return diagnostic_;
}

std::vector<FormPtr> read_forms(std::string_view source) {
  Parser parser(source);
  return parser.parse_all();
}

}  // namespace termis
