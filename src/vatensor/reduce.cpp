#include "reduce.hpp"

#include "rearrange.hpp"
#include <cmath>                                        // for double_t
#include <stdexcept>                                    // for runtime_error
#include <tuple>                                        // for tuple
#include <utility>                                      // for forward, move

#include "allocate.hpp"
#include "varray.hpp"                            // for VArray, axes_...
#include "vcompute.hpp"                                   // for vreduce, xope...
#include "vmath.hpp"
#include "vpromote.hpp"                                   // for num_matching_...
#include "xtensor/xiterator.hpp"                        // for operator==
#include "xtensor/xlayout.hpp"                          // for layout_type
#include "xtensor/xmath.hpp"                            // for XTENSOR_REDUC...
#include "xtensor/xnorm.hpp"                            // for norm_l0, norm_l1
#include "xtensor/xoperation.hpp"                       // for logical_and
#include "xtensor/xsort.hpp"                            // for median
#include "xtl/xiterator_base.hpp"                       // for operator!=

namespace xt {
	namespace evaluation_strategy {
		struct lazy_type;
	}
}

using namespace va;

// TODO Passing EVS is required because norms don't support it without it, we should make a PR (though it's not bad to explicitly make it lazy).
#define REDUCER_LAMBDA(func) [](auto&& a) { return func(std::forward<decltype(a)>(a), std::tuple<xt::evaluation_strategy::lazy_type>())(); }
#define REDUCER_LAMBDA_NOECS(func) [](auto&& a) { return func(std::forward<decltype(a)>(a)); }
#define REDUCER_LAMBDA_AXES(axes, func) [&axes](auto&& a) { return func(std::forward<decltype(a)>(a), axes, std::tuple<xt::evaluation_strategy::lazy_type>()); }
#define REDUCER_LAMBDA_AXES_NOECS(axes, func) [&axes](auto&& a) { return func(std::forward<decltype(a)>(a), axes); }

// FIXME These don't support axes yet, see https://github.com/xtensor-stack/xtensor/issues/1555
using namespace xt;
XTENSOR_REDUCER_FUNCTION(va_any, xt::detail::logical_or, bool, true)
XTENSOR_REDUCER_FUNCTION(va_all, xt::detail::logical_and, bool, false)

VScalar va::sum(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_in_same_out, VScalar>(
		REDUCER_LAMBDA(xt::sum),
		array.read
	);
#endif
}

void va::sum(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_in_same_out>(
		REDUCER_LAMBDA_AXES(axes, xt::sum),
		target, array.read
	);
#endif
}

VScalar va::prod(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_at_least_int32_in_same_out, VScalar>(
		REDUCER_LAMBDA(xt::prod),
		array.read
	);
#endif
}

void va::prod(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_at_least_int32_in_same_out>(
		REDUCER_LAMBDA_AXES(axes, xt::prod),
		target, array.read
	);
#endif
}

VScalar va::mean(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::mean),
		array.read
	);
#endif
}

void va::mean(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::mean),
		target, array.read
	);
#endif
}

VScalar va::median(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_in_same_out, VScalar>(
		REDUCER_LAMBDA_NOECS(xt::median),
		array.read
	);
#endif
}

void va::median(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	if (axes.size() == 1 && array.layout() != xt::layout_type::dynamic) {
		// Supported by xtensor.
		auto axis = axes[0];
		va::xoperation_inplace<promote::num_in_same_out>(
			REDUCER_LAMBDA_AXES_NOECS(axis, xt::median),
			target, array.read
		);
		return;
	}

	// Multi-axis (and dynamic layout) not supported by xtensor. Gotta join the requested axes into one first.
	const auto joined = join_axes_into_last_dimension(array, axes);
	constexpr auto axis = -1;

	if (joined->layout() == xt::layout_type::dynamic) {
		// xtensor does not support dynamic layout, so we need a copy first.
		const auto joined_copy = copy_as_dtype(array.read, DTypeMax);
		va::xoperation_inplace<promote::num_in_same_out>(
			REDUCER_LAMBDA_AXES_NOECS(axis, xt::median),
			target, joined_copy->read
		);
	}
	else {
		va::xoperation_inplace<promote::num_in_same_out>(
			REDUCER_LAMBDA_AXES_NOECS(axis, xt::median),
			target, joined->read
		);
	}
#endif
}

VScalar va::var(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::variance),
		array.read
	);
#endif
}

void va::var(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::variance),
		target, array.read
	);
#endif
}

VScalar va::std(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::stddev),
		array.read
	);
#endif
}

void va::std(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::stddev),
		target, array.read
	);
#endif
}

VScalar va::max(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::common_in_same_out, VScalar>(
		REDUCER_LAMBDA(xt::amax),
		array.read
	);
#endif
}

void va::max(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::common_in_same_out>(
		REDUCER_LAMBDA_AXES(axes, xt::amax),
		target, array.read
	);
#endif
}

VScalar va::min(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::common_in_same_out, VScalar>(
		REDUCER_LAMBDA(xt::amin),
		array.read
	);
#endif
}

void va::min(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::common_in_same_out>(
		REDUCER_LAMBDA_AXES(axes, xt::amin),
		target, array.read
	);
#endif
}

VScalar va::norm_l0(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::norm_l0),
		array.read
	);
#endif
}

void va::norm_l0(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::norm_l0),
		target, array.read
	);
#endif
}

VScalar va::norm_l1(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::norm_l1),
		array.read
	);
#endif
}

void va::norm_l1(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::norm_l1),
		target, array.read
	);
#endif
}

VScalar va::norm_l2(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::norm_l2),
		array.read
	);
#endif
}

void va::norm_l2(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::norm_l2),
		target, array.read
	);
#endif
}

VScalar va::norm_linf(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::num_matching_float_or_default_in_same_out<double_t>, VScalar>(
		REDUCER_LAMBDA(xt::norm_linf),
		array.read
	);
#endif
}

void va::norm_linf(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::num_matching_float_or_default_in_same_out<double_t>>(
		REDUCER_LAMBDA_AXES(axes, xt::norm_linf),
		target, array.read
	);
#endif
}

VScalar va::count_nonzero(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	if (array.dtype() == va::Bool)
		return sum(array);

	const auto is_nonzero = va::copy_as_dtype(array.read, va::Bool);
	return va::sum(*is_nonzero);
#endif
}

void va::count_nonzero(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	if (array.dtype() == va::Bool)
		return sum(target, array, axes);

	const auto is_nonzero = va::copy_as_dtype(array.read, va::Bool);
	return va::sum(target, *is_nonzero, axes);
#endif
}

bool va::all(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::x_in_nat_out<bool>, bool>(
		REDUCER_LAMBDA_NOECS(xt::all),
		array.read
	);
#endif
}

void va::all(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::x_in_nat_out<bool>>(
		REDUCER_LAMBDA_AXES(axes, va_all),
		target, array.read
	);
#endif
}

bool va::any(const VArray& array) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	return vreduce<promote::x_in_nat_out<bool>, bool>(
		REDUCER_LAMBDA_NOECS(xt::any),
		array.read
	);
#endif
}

void va::any(VArrayTarget target, const VArray& array, const axes_type& axes) {
#ifdef NUMDOT_DISABLE_REDUCTION_FUNCTIONS
	throw std::runtime_error("function explicitly disabled; recompile without NUMDOT_DISABLE_REDUCTION_FUNCTIONS to enable it.");
#else
	va::xoperation_inplace<promote::x_in_nat_out<bool>>(
		REDUCER_LAMBDA_AXES(axes, va_any),
		target, array.read
	);
#endif
}

va::VScalar va::reduce_dot(const VArray& a, const VArray& b) {
	// Could also do this instead to avoid generating more code.
	// std::shared_ptr<va::VArray> prod_cache;
	// va::multiply(&prod_cache, a, b);
	// return sum(*prod_cache);

	return vreduce<promote::num_in_same_out, VScalar>(
		[](auto&& a, auto&& b) {
			 using A = decltype(a);
			 using B = decltype(b);

			 return xt::sum(
			 	std::forward<A>(a) * std::forward<B>(b),
			 	std::tuple<xt::evaluation_strategy::lazy_type>()
			)();
	   },
		a.read, b.read
	);
}

void va::reduce_dot(VArrayTarget target, const VArray& a, const VArray& b, const axes_type& axes) {
	// Could also do this instead to avoid generating more code.
	// std::shared_ptr<va::VArray> prod_cache;
	// va::multiply(&prod_cache, a, b);
	// va::sum(target, *prod_cache, axes);

	va::xoperation_inplace<promote::num_in_same_out>(
		 [&axes](auto&& a, auto&& b) {
			 using A = decltype(a);
			 using B = decltype(b);

			 return xt::sum(
		 		// These HAVE to be passed in directly as rvalue, otherwise they'll be
		 		// stored by sum as references, crashing the program.
			 	std::forward<A>(a) * std::forward<B>(b),
			 	axes,
			 	std::tuple<xt::evaluation_strategy::lazy_type>()
			);
		},
		target, a.read, b.read
	);
}
