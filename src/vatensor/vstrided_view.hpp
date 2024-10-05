#ifndef VSTRIDED_VIEW_HPP
#define VSTRIDED_VIEW_HPP

#include <xtensor/xstrided_view.hpp>

namespace va {
    // Like xt::strided_view_args, but fill_args does bounds checks and suppors negative indices.
	template <class adj_strides_policy>
    struct strided_view_args : adj_strides_policy
    {
        using base_type = adj_strides_policy;

	    template<typename T, typename S>
        static void mod_idx(T& idx, S& dim) {
	        if (idx < 0) idx = dim + idx;
	        if (idx >= dim || idx < 0) throw std::runtime_error("Slice index out of range.");
	    }

        template <class S, class ST, class V>
        void
        fill_args(const S& shape, ST&& old_strides, std::size_t base_offset, xt::layout_type layout, const V& slices)
        {
            // Compute dimension
            std::size_t dimension = shape.size(), n_newaxis = 0, n_add_all = 0;
            std::ptrdiff_t dimension_check = static_cast<std::ptrdiff_t>(shape.size());

            bool has_ellipsis = false;
            for (const auto& el : slices)
            {
                if (xtl::get_if<xt::xnewaxis_tag>(&el) != nullptr)
                {
                    ++dimension;
                    ++n_newaxis;
                }
                else if (xtl::get_if<std::ptrdiff_t>(&el) != nullptr)
                {
                    --dimension;
                    --dimension_check;
                }
                else if (xtl::get_if<xt::xellipsis_tag>(&el) != nullptr)
                {
                    if (has_ellipsis == true)
                    {
                        XTENSOR_THROW(std::runtime_error, "Ellipsis can only appear once.");
                    }
                    has_ellipsis = true;
                }
                else
                {
                    --dimension_check;
                }
            }

            if (dimension_check < 0)
            {
                XTENSOR_THROW(std::runtime_error, "Too many slices for view.");
            }

            if (has_ellipsis)
            {
                // replace ellipsis with N * xt::all
                // remove -1 because of the ellipsis slize itself
                n_add_all = shape.size() - (slices.size() - 1 - n_newaxis);
            }

            // Compute strided view
            new_offset = base_offset;
            new_shape.resize(dimension);
            new_strides.resize(dimension);
            base_type::resize(dimension);

            auto old_shape = shape;
            using old_strides_value_type = typename std::decay_t<ST>::value_type;

            std::ptrdiff_t axis_skip = 0;
            std::size_t idx = 0, i = 0, i_ax = 0;

            auto slice_getter = xt::detail::slice_getter_impl<S>(shape);

            for (; i < slices.size(); ++i)
            {
                i_ax = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i) - axis_skip);
                auto ptr = xtl::get_if<std::ptrdiff_t>(&slices[i]);
                if (ptr != nullptr)
                {
                    auto slice0 = static_cast<old_strides_value_type>(*ptr);
                    mod_idx(slice0, shape[i_ax]);  // THIS IS THE ONLY CHANGE WE MADE
                    new_offset += static_cast<std::size_t>(slice0 * old_strides[i_ax]);
                }
                else if (xtl::get_if<xt::xnewaxis_tag>(&slices[i]) != nullptr)
                {
                    new_shape[idx] = 1;
                    base_type::set_fake_slice(idx);
                    ++axis_skip, ++idx;
                }
                else if (xtl::get_if<xt::xellipsis_tag>(&slices[i]) != nullptr)
                {
                    for (std::size_t j = 0; j < n_add_all; ++j)
                    {
                        new_shape[idx] = old_shape[i_ax];
                        new_strides[idx] = old_strides[i_ax];
                        base_type::set_fake_slice(idx);
                        ++idx, ++i_ax;
                    }
                    axis_skip = axis_skip - static_cast<std::ptrdiff_t>(n_add_all) + 1;
                }
                else if (xtl::get_if<xt::xall_tag>(&slices[i]) != nullptr)
                {
                    new_shape[idx] = old_shape[i_ax];
                    new_strides[idx] = old_strides[i_ax];
                    base_type::set_fake_slice(idx);
                    ++idx;
                }
                else if (base_type::fill_args(slices, i, idx, old_shape[i_ax], old_strides[i_ax], new_shape, new_strides))
                {
                    ++idx;
                }
                else
                {
                    slice_getter.idx = i_ax;
                    auto info = xtl::visit(slice_getter, slices[i]);
                    new_offset += static_cast<std::size_t>(info[0] * old_strides[i_ax]);
                    new_shape[idx] = static_cast<std::size_t>(info[1]);
                    new_strides[idx] = info[2] * old_strides[i_ax];
                    base_type::set_fake_slice(idx);
                    ++idx;
                }
            }

            i_ax = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i) - axis_skip);
            for (; i_ax < old_shape.size(); ++i_ax, ++idx)
            {
                new_shape[idx] = old_shape[i_ax];
                new_strides[idx] = old_strides[i_ax];
                base_type::set_fake_slice(idx);
            }

            new_layout = do_strides_match(new_shape, new_strides, layout, true) ? layout : xt::layout_type::dynamic;
        }

        using shape_type = xt::dynamic_shape<std::size_t>;
        shape_type new_shape;
        using strides_type = xt::dynamic_shape<std::ptrdiff_t>;
        strides_type new_strides;
        std::size_t new_offset;
        xt::layout_type new_layout;
    };
}

#endif //VSTRIDED_VIEW_HPP