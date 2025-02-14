/// \file
// Range v3 library
//
//  Copyright Eric Niebler 2013-present
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3
//

#ifndef RANGES_V3_VIEW_PARTIAL_SUM_HPP
#define RANGES_V3_VIEW_PARTIAL_SUM_HPP

#include <iterator>
#include <type_traits>
#include <utility>

#include <meta/meta.hpp>

#include <range/v3/range_fwd.hpp>

#include <range/v3/functional/arithmetic.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/functional/invoke.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/utility/addressof.hpp>
#include <range/v3/utility/semiregular_box.hpp>
#include <range/v3/utility/static_const.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/facade.hpp>
#include <range/v3/view/view.hpp>

namespace ranges
{
    /// \cond
    namespace detail
    {
        // clang-format off
        CPP_def
        (
            template(typename Rng, typename Fun)
            concept partial_sum_view_constraints,
                input_range<Rng> &&
                copy_constructible<Fun> &&
                copy_constructible<range_value_t<Rng>> &&
                constructible_from<range_value_t<Rng>, range_reference_t<Rng>> &&
                assignable_from<range_value_t<Rng> &, range_reference_t<Rng>> &&
                indirectly_binary_invocable_<Fun &, iterator_t<Rng>, iterator_t<Rng>> &&
                assignable_from<
                    range_value_t<Rng> &,
                    indirect_result_t<Fun &, iterator_t<Rng>, iterator_t<Rng>>>
        );
        // clang-format on
    } // namespace detail
    /// \endcond

    /// \addtogroup group-views
    /// @{
    template<typename Rng, typename Fun>
    struct partial_sum_view
      : view_facade<partial_sum_view<Rng, Fun>, range_cardinality<Rng>::value>
    {
    private:
        friend range_access;
        CPP_assert(view_<Rng>);
        CPP_assert(detail::partial_sum_view_constraints<Rng, Fun>);

        RANGES_NO_UNIQUE_ADDRESS Rng base_{};
        RANGES_NO_UNIQUE_ADDRESS semiregular_box_t<Fun> fun_;

        template<bool IsConst>
        struct cursor
        {
        private:
            friend cursor<true>;

            using Parent = meta::const_if_c<IsConst, partial_sum_view>;
            using Base = meta::const_if_c<IsConst, Rng>;

            Parent * parent_ = nullptr;
            RANGES_NO_UNIQUE_ADDRESS iterator_t<Base> current_{};
            RANGES_NO_UNIQUE_ADDRESS semiregular_box_t<range_value_t<Rng>> sum_;

        public:
            using single_pass = meta::bool_<single_pass_iterator_<iterator_t<Base>>>;

            cursor() = default;
            constexpr explicit cursor(Parent * rng)
              : parent_{rng}
              , current_(ranges::begin(rng->base_))
            {
                if(current_ != ranges::end(rng->base_))
                    sum_ = *current_;
            }
            CPP_template(bool Other)( //
                requires IsConst && (!Other) &&
                convertible_to<iterator_t<Rng> const &,
                               iterator_t<Base>>) //
                constexpr cursor(cursor<Other> const & that)
              : parent_{that.parent_}
              , current_(that.current_)
              , sum_(that.sum_)
            {}
            constexpr range_value_t<Rng> read() const
            {
                RANGES_EXPECT(current_ != ranges::end(parent_->base_));
                return sum_;
            }
            constexpr void next()
            {
                auto last = ranges::end(parent_->base_);
                RANGES_EXPECT(current_ != last);
                if(++current_ != last)
                {
                    auto & sum = static_cast<range_value_t<Rng> &>(sum_);
                    using F = meta::const_if_c<IsConst, Fun>;
                    auto & f = static_cast<F &>(parent_->fun_);
                    sum = invoke(f, sum, *current_);
                }
            }
            constexpr bool equal(default_sentinel_t) const
            {
                return current_ == ranges::end(parent_->base_);
            }
            CPP_member
            constexpr bool CPP_fun(equal)(cursor const & that)(
                const requires equality_comparable<iterator_t<Base>>)
            {
                RANGES_EXPECT(parent_ == that.parent_);
                return current_ == that.current_;
            }
        };

        constexpr cursor<false> begin_cursor()
        {
            return cursor<false>{this};
        }
        template<typename CRng = Rng const>
        constexpr auto begin_cursor() const -> CPP_ret(cursor<true>)( //
            requires detail::partial_sum_view_constraints<CRng, Fun const>)
        {
            return cursor<true>{this};
        }

    public:
        partial_sum_view() = default;
        constexpr partial_sum_view(Rng rng, Fun fun) noexcept(
            std::is_nothrow_move_constructible<Rng>::value &&
                std::is_nothrow_move_constructible<Fun>::value)
          : base_(std::move(rng))
          , fun_(std::move(fun))
        {}
        CPP_member
        constexpr auto CPP_fun(size)()(requires sized_range<Rng>)
        {
            return ranges::size(base_);
        }
        CPP_member
        constexpr auto CPP_fun(size)()(const requires sized_range<Rng const>)
        {
            return ranges::size(base_);
        }
    };

#if RANGES_CXX_DEDUCTION_GUIDES >= RANGES_CXX_DEDUCTION_GUIDES_17
    CPP_template(typename Rng, typename Fun)(requires copy_constructible<Fun>)
        partial_sum_view(Rng &&, Fun)
            ->partial_sum_view<views::all_t<Rng>, Fun>;
#endif

    namespace views
    {
        struct partial_sum_fn
        {
        private:
            friend view_access;
            template<typename Fun>
            static constexpr auto bind(partial_sum_fn partial_sum, Fun fun)
            {
                return make_pipeable(bind_back(partial_sum, std::move(fun)));
            }
            template<typename Fun = plus>
            RANGES_DEPRECATED(
                "Use \"ranges::views::partial_sum\" instead of "
                "\"ranges::views::partial_sum()\".")
            static constexpr auto bind(partial_sum_fn partial_sum)
            {
                return make_pipeable(bind_back(partial_sum, Fun{}));
            }

        public:
            template<typename Rng, typename Fun = plus>
            constexpr auto operator()(Rng && rng, Fun fun = {}) const
                -> CPP_ret(partial_sum_view<all_t<Rng>, Fun>)( //
                    requires detail::partial_sum_view_constraints<all_t<Rng>, Fun>)
            {
                return {all(static_cast<Rng &&>(rng)), std::move(fun)};
            }
        };

        /// \relates partial_sum_fn
        /// \ingroup group-views
        RANGES_INLINE_VARIABLE(view<partial_sum_fn>, partial_sum)
    } // namespace views
    /// @}
} // namespace ranges

#include <range/v3/detail/satisfy_boost_range.hpp>
RANGES_SATISFY_BOOST_RANGE(::ranges::partial_sum_view)

#endif
