#include <gtest/gtest.h>

#include <array>
#include <tuple>

struct bit_range {
  uint32_t begin{0};
  uint32_t end{0};

  [[nodiscard]] constexpr bool empty() const { return begin == end; }

  template <std::unsigned_integral T>
  constexpr T value_mask() {
    return (T{1} << (end - begin)) - 1;
  };

  template <std::unsigned_integral T>
  constexpr T mask() {
    return value_mask<T>() << begin;
  }

  template <std::unsigned_integral T>
  constexpr T value(T value) {
    return (value & value_mask<T>()) << begin;
  }
};

constexpr bit_range next_bits(bit_range const& prev, uint32_t length) {
  return bit_range{prev.end, prev.end + length};
}

constexpr bit_range lo(bit_range const& rng, uint32_t length) {
  return {rng.begin, length};
}

constexpr bit_range hi(bit_range const& rng, uint32_t length) {
  return {rng.end - length, rng.end};
}

constexpr bit_range mid(bit_range const& rng, uint32_t begin, uint32_t end) {
  return {rng.begin + begin, rng.end - end};
}

template <std::unsigned_integral T>
constexpr T bits_value_mask(bit_range const& rng) {
  return (T{1} << (rng.end - rng.begin)) - 1;
}

template <std::unsigned_integral T>
constexpr T bits_mask(bit_range const& rng) {
  return ((T{1} << (rng.end - rng.begin)) - 1) << rng.begin;
}

template <std::unsigned_integral T, typename... Args>
constexpr T bits_value(bit_range b_rng, T value, Args&&... args) {
  if constexpr (sizeof...(args) == 0)
    return b_rng.value<T>(value);
  else
    return b_rng.value<T>(value) | bits(std::forward<Args>(args)...);
}

struct bits_layout_trait {};

template <typename Tag, typename... SubLayout>
struct bits_layout : bits_layout_trait {
  bool is_union;
  Tag tag;
  uint32_t width;
  std::tuple<SubLayout...> sub_layouts;

  template <typename Tag2>
  [[nodiscard]] constexpr bit_range operator[](Tag2 t) const noexcept {
    if (t == tag) {
      return bit_range{0, width};
    }

    constexpr auto sub_size = std::tuple_size_v<decltype(sub_layouts)>;
    if constexpr (sub_size == 0) {
      return bit_range{};
    }

    auto sub_ranges = std::apply(
        [t](auto... subs) { return std::array<bit_range, sub_size>{subs[t]...}; }, sub_layouts);

    auto sub_width = std::apply(
        [](auto... subs) { return std::array<uint32_t, sub_size>{subs.width...}; }, sub_layouts);

    // accumulate offsets
    auto sub_offset = 0;
    for (size_t i = 0; i < sub_size; ++i) {
      auto sub_range = sub_ranges[i];
      if (!sub_range.empty()) {
        return bit_range{sub_range.begin + sub_offset, sub_range.end + sub_offset};
      }
      if (!is_union) {
        sub_offset += sub_width[i];
      }
    }

    return bit_range{0, 0};
  }

  template <typename Tag2, std::unsigned_integral T, typename... Args>
  [[nodiscard]] constexpr uint32_t value(Tag2 t, T v, Args... args) const noexcept {
    if constexpr (sizeof...(Args) == 0)
      return (*this)[t].value(v);
    else
      return (*this)[t].value(v) | value(std::forward<Args>(args)...);
  }
};

template <auto tag>
constexpr auto b_struct(std::derived_from<bits_layout_trait> auto... sub_layouts) {
  return bits_layout<decltype(tag), decltype(sub_layouts)...>{
      {}, false, tag, (sub_layouts.width + ...), {sub_layouts...}};
}

template <auto tag>
constexpr auto b_union(std::derived_from<bits_layout_trait> auto... sub_layouts) {
  return bits_layout<decltype(tag), decltype(sub_layouts)...>{
      {}, true, tag, std::max({sub_layouts.width...}), {sub_layouts...}};
}

constexpr auto def_(auto tag, uint32_t width) {
  return bits_layout<decltype(tag)>{{}, false, tag, width, {}};
}

TEST(NewEnums, NewEnums) {
  enum tags { t, t0, t1, t00, t01, t10, t11 };

  {
    constexpr auto layout0 = b_struct<t>(def_(t0, 8));
    constexpr auto t0_bits_layout0 = layout0[t0];
    static_assert(t0_bits_layout0.begin == 0);
    static_assert(t0_bits_layout0.end == 8);
  }

  {
    constexpr auto layout1 = b_struct<t>(def_(t0, 8), def_(t1, 8));
    static_assert(layout1.width == 16);
    static_assert(!layout1.is_union);

    constexpr auto t0_bits_layout1 = layout1[t0];
    static_assert(t0_bits_layout1.begin == 0);
    static_assert(t0_bits_layout1.end == 8);

    constexpr auto t1_bits_layout1 = layout1[t1];
    static_assert(t1_bits_layout1.begin == 8);
    static_assert(t1_bits_layout1.end == 16);
  }

  {
    // clang-format off
    constexpr auto layout_ssu =
        b_struct<t>(
          b_struct<t0>(
            def_(t00, 8),
            def_(t01, 8)),
          b_union<t1>(
            def_(t10, 10),
            def_(t11, 14)));
    // clang-format on
    static_assert(layout_ssu.width == 30);

    static_assert(layout_ssu[t0].begin == 0);
    static_assert(layout_ssu[t0].end == 16);
    static_assert(layout_ssu[t11].begin == 16);
    static_assert(layout_ssu[t11].end == 30);

    constexpr auto t10_bits = layout_ssu[t10];
    static_assert(t10_bits.begin == 16);
    static_assert(t10_bits.end == 26);

    constexpr auto t00_bits = layout_ssu[t00];
    static_assert(bits_mask<uint32_t>(t00_bits) == 0xFF);
    static_assert(bits_mask<uint32_t>(layout_ssu[t01]) == 0xFF00);

    constexpr auto t10_v = 0b1011U;
    static_assert(bits_value(t10_bits, t10_v) == (t10_v << 16));
    static_assert(bits_value(t10_bits, 1U) == (1U << 16));
    static_assert(bits_mask<uint32_t>(t10_bits) == ((1U << 26) - (1U << 16)));
  }
}

namespace builtin_types_detail {

enum class layout_ids : uint32_t { none, dim_count, bytes_log2, sign, type_class, dim0, dim1 };

using enum layout_ids;

constexpr auto layout_ = b_struct<none>(
    def_(bytes_log2, 4),
    def_(sign, 4),
    def_(dim_count, 4),
    def_(dim0, 8),
    def_(dim1, 8),
    def_(type_class, 4));

static_assert(layout_.width == 32);

constexpr uint32_t c_void = 1, c_sampler = 2, c_booleans = 3, c_integers = 4, c_floats = 5;

constexpr auto e_value(auto... args) {
  return layout_.value(args...);
}

enum class builtin_types : uint32_t {
  none = 0,

  int64 = e_value(bytes_log2, 3U, sign, 1U, type_class, c_integers),
  int32 = e_value(bytes_log2, 2U, sign, 1U, type_class, c_integers),
  int16 = e_value(bytes_log2, 1U, sign, 1U, type_class, c_integers),
  int8 = e_value(bytes_log2, 0U, sign, 1U, type_class, c_integers),

  uint64 = e_value(bytes_log2, 3U, sign, 0U, type_class, c_integers),
  uint32 = e_value(bytes_log2, 2U, sign, 0U, type_class, c_integers),
  uint16 = e_value(bytes_log2, 1U, sign, 0U, type_class, c_integers),
  uint8 = e_value(bytes_log2, 0U, sign, 0U, type_class, c_integers),

  float16 = e_value(bytes_log2, 1U, sign, 1U, type_class, c_floats),
  float32 = e_value(bytes_log2, 2U, sign, 1U, type_class, c_floats),
  float64 = e_value(bytes_log2, 3U, sign, 1U, type_class, c_floats),

  e_bool = e_value(type_class, c_booleans),
  sampler = e_value(type_class, c_sampler),
  e_void = e_value(type_class, c_void)
};
}  // namespace builtin_types_detail

using builtin_types_detail::builtin_types;

template <auto... args>
struct constants_to_tuple {
  using type = std::tuple<std::integral_constant<decltype(args), args>...>;
};

template <typename ContainerT, typename ArgsTuple>
ContainerT tuple_to_container(ArgsTuple&& args_tuple) {
  return std::apply([](auto&&... args) { return ContainerT{args...}; }, ((ArgsTuple&&)args_tuple));
}

template <typename A, typename TupleB, typename TemplateFn>
struct sv_mul {};

template <typename A, typename... TupleBArgs, typename TemplateFn>
struct sv_mul<A, std::tuple<TupleBArgs...>, TemplateFn> {
  using type = std::tuple<typename TemplateFn::template apply<A, TupleBArgs>::type...>;
};

struct combine_enum {
  static constexpr auto combine_value(auto a, auto b) {
    if constexpr (std::is_enum_v<decltype(a)>) {
      return static_cast<decltype(a)>(
          static_cast<std::underlying_type_t<decltype(a)>>(a) |
          static_cast<std::underlying_type_t<decltype(a)>>(b));
    } else {
      return a | b;
    }
  }

  template <typename A, typename B>
  struct apply {
    using type = std::integral_constant<typename A::value_type, combine_value(A::value, B::value)>;
  };
};

template <typename TupleA, typename TupleB>
struct prod {};

template <typename... TupleAArgs, typename TupleB>
struct prod<std::tuple<TupleAArgs...>, TupleB> {
  using type = decltype(std::tuple_cat(
      std::declval<typename sv_mul<TupleAArgs, TupleB, combine_enum>::type>()...));
};

template <typename ScalarTuple, typename IndexSequence>
struct make_vector_types {};

template <typename ScalarTuple, size_t... Is>
struct make_vector_types<ScalarTuple, std::index_sequence<Is...>> {
  static constexpr auto vector_part(size_t I) {
    return static_cast<builtin_types>(builtin_types_detail::e_value(
        builtin_types_detail::dim0, I, builtin_types_detail::dim_count, 1U));
  }

  using type =
      typename prod<ScalarTuple, typename constants_to_tuple<vector_part(Is)...>::type>::type;
};

template <typename ScalarTuple, typename IndexSequence0, typename IndexSequence1>
struct make_matrix_types {};

template <typename ScalarTuple, size_t... I0s, size_t... I1s>
struct make_matrix_types<ScalarTuple, std::index_sequence<I0s...>, std::index_sequence<I1s...>> {
  static constexpr auto dim0s(size_t I) {
    return static_cast<builtin_types>(builtin_types_detail::e_value(
        builtin_types_detail::dim0, I, builtin_types_detail::dim_count, 2U));
  }

  static constexpr auto dim1s(size_t I) {
    return static_cast<builtin_types>(builtin_types_detail::e_value(
        builtin_types_detail::dim1, I));
  }

  using dim0_types = typename prod<
      ScalarTuple,
      typename constants_to_tuple<dim0s(I0s)...>::type>::type;

  using type = typename prod<
      dim0_types,
      typename constants_to_tuple<dim1s(I1s)...>::type>::type;
};

template <typename ContainerT>
ContainerT listing_builtin_types() {
  using scalar_types = constants_to_tuple<
      builtin_types::int64,
      builtin_types::int32,
      builtin_types::int16,
      builtin_types::int8,
      builtin_types::uint64,
      builtin_types::uint32,
      builtin_types::uint16,
      builtin_types::uint8,
      builtin_types::float16,
      builtin_types::float32,
      builtin_types::float64,
      builtin_types::e_bool>::type;

  using Seq4 = std::make_index_sequence<4>;

  using vector_types = make_vector_types<scalar_types, Seq4>::type;

  using matrix_types = typename make_matrix_types<scalar_types, Seq4, Seq4>::type;

  using combined_tuple = decltype(std::tuple_cat(std::declval<scalar_types>(),
                                                std::declval<vector_types>(),
                                                std::declval<matrix_types>()));

  return std::apply([](auto... args) {
    return ContainerT{decltype(args)::value...};
  }, combined_tuple{});
}

TEST(NewEnums, BuiltinTypes) {
  auto types = listing_builtin_types<std::vector<builtin_types>>();
  constexpr auto scalar_count = 12;
  EXPECT_EQ(types.size(), 12 * (16+4+1));
}