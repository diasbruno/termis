#include "layout.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace termis {
namespace {

[[noreturn]] void fail(SourceLocation location, std::string message) {
  throw LayoutError(Diagnostic{location, std::move(message)});
}

std::uint64_t align_to(std::uint64_t value, std::uint64_t alignment) {
  const auto remainder = value % alignment;
  if (remainder == 0) {
    return value;
  }
  return value + alignment - remainder;
}

std::uint64_t array_size_value(const Type& type) {
  if (type.array_size < 0) {
    fail(type.location, "array size must be non-negative");
  }
  return static_cast<std::uint64_t>(type.array_size);
}

Layout primitive_layout(PrimitiveType primitive, SourceLocation location) {
  switch (primitive) {
    case PrimitiveType::i8:
    case PrimitiveType::u8:
    case PrimitiveType::bool_:
      return Layout{1, 1, {}};
    case PrimitiveType::i16:
    case PrimitiveType::u16:
      return Layout{2, 2, {}};
    case PrimitiveType::i32:
    case PrimitiveType::u32:
    case PrimitiveType::f32:
      return Layout{4, 4, {}};
    case PrimitiveType::i64:
    case PrimitiveType::u64:
    case PrimitiveType::f64:
      return Layout{8, 8, {}};
    case PrimitiveType::unit:
      return Layout{0, 1, {}};
    case PrimitiveType::void_:
      fail(location, "void does not have a source-language value layout");
  }
}

}  // namespace

LayoutError::LayoutError(Diagnostic diagnostic)
    : std::runtime_error(diagnostic.message), diagnostic_(std::move(diagnostic)) {}

const Diagnostic& LayoutError::diagnostic() const {
  return diagnostic_;
}

LayoutEngine::LayoutEngine(const TypeEnvironment& types) : types_(types) {}

Layout LayoutEngine::compute(const Type& type) const {
  switch (type.kind) {
    case TypeKind::primitive:
      return primitive_layout(type.primitive, type.location);

    case TypeKind::name: {
      const auto* declaration = types_.find(type.name);
      if (declaration == nullptr) {
        fail(type.location, "unknown type name");
      }
      if (!declaration->parameters.empty()) {
        fail(type.location, "generic type requires type arguments");
      }
      return compute(*declaration->body);
    }

    case TypeKind::pointer:
    case TypeKind::function:
      return Layout{8, 8, {}};

    case TypeKind::slice:
      return Layout{16, 8, {}};

    case TypeKind::array: {
      const auto element = compute(*type.element);
      const auto count = array_size_value(type);
      if (element.size != 0 && count > std::numeric_limits<std::uint64_t>::max() / element.size) {
        fail(type.location, "array layout size overflow");
      }
      return Layout{element.size * count, element.alignment, {}};
    }

    case TypeKind::product: {
      Layout layout;
      std::uint64_t offset = 0;
      for (const auto& field : type.fields) {
        const auto field_layout = compute(*field.type);
        offset = align_to(offset, field_layout.alignment);
        layout.fields.push_back(FieldLayout{
            field.name,
            offset,
            field_layout.size,
            field_layout.alignment,
        });
        offset += field_layout.size;
        layout.alignment = std::max(layout.alignment, field_layout.alignment);
      }
      layout.size = align_to(offset, layout.alignment);
      return layout;
    }

    case TypeKind::union_: {
      Layout layout;
      for (const auto& field : type.fields) {
        const auto field_layout = compute(*field.type);
        layout.fields.push_back(FieldLayout{
            field.name,
            0,
            field_layout.size,
            field_layout.alignment,
        });
        layout.size = std::max(layout.size, field_layout.size);
        layout.alignment = std::max(layout.alignment, field_layout.alignment);
      }
      layout.size = align_to(layout.size, layout.alignment);
      return layout;
    }

    case TypeKind::sum: {
      std::uint64_t payload_size = 0;
      std::uint64_t payload_alignment = 1;
      for (const auto& alternative : type.alternatives) {
        std::uint64_t alternative_size = 0;
        std::uint64_t alternative_alignment = 1;
        for (const auto& payload : alternative.payload) {
          const auto payload_layout = compute(*payload);
          alternative_size = align_to(alternative_size, payload_layout.alignment);
          alternative_size += payload_layout.size;
          alternative_alignment = std::max(alternative_alignment, payload_layout.alignment);
        }
        alternative_size = align_to(alternative_size, alternative_alignment);
        payload_size = std::max(payload_size, alternative_size);
        payload_alignment = std::max(payload_alignment, alternative_alignment);
      }

      constexpr std::uint64_t tag_size = 4;
      constexpr std::uint64_t tag_alignment = 4;
      const auto alignment = std::max(tag_alignment, payload_alignment);
      const auto payload_offset = align_to(tag_size, payload_alignment);
      return Layout{align_to(payload_offset + payload_size, alignment), alignment, {}};
    }

    case TypeKind::application:
      fail(type.location, "generic type layout is not implemented yet");
  }
}

}  // namespace termis
