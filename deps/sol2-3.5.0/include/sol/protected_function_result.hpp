// sol2

// The MIT License (MIT)

// Copyright (c) 2013-2022 Rapptz, ThePhD and contributors

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SOL_PROTECTED_FUNCTION_RESULT_HPP
#define SOL_PROTECTED_FUNCTION_RESULT_HPP

#include <sol/reference.hpp>
#include <sol/tuple.hpp>
#include <sol/stack.hpp>
#include <sol/proxy_base.hpp>
#include <sol/stack_iterator.hpp>
#include <sol/stack_proxy.hpp>
#include <sol/error.hpp>
#include <sol/stack.hpp>
#include <cstdint>

namespace sol {
	struct protected_function_result : public proxy_base<protected_function_result> {
	private:
		lua_State* L;
		int index;
		int returncount;
		int popcount;
		call_status err;

	public:
		typedef stack_proxy reference_type;
		typedef stack_proxy value_type;
		typedef stack_proxy* pointer;
		typedef std::ptrdiff_t difference_type;
		typedef std::size_t size_type;
		typedef stack_iterator<stack_proxy, false> iterator;
		typedef stack_iterator<stack_proxy, true> const_iterator;
		typedef std::reverse_iterator<iterator> reverse_iterator;
		typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

		protected_function_result() noexcept : protected_function_result(nullptr) {}
		protected_function_result(lua_State* Ls, int idx = -1, int retnum = 0, int popped = 0, call_status pferr = call_status::ok) noexcept
		: L(Ls), index(idx), returncount(retnum), popcount(popped), err(pferr) {
		}

		// We do not want anyone to copy these around willy-nilly
		// Will likely break people, but also will probably get rid of quiet bugs that have
		// been lurking. (E.g., Vanilla Lua will just quietly discard over-pops and under-pops:
		// LuaJIT and other Lua engines will implode and segfault at random later times.)
		protected_function_result(const protected_function_result&) = delete;
		protected_function_result& operator=(const protected_function_result&) = delete;

		protected_function_result(protected_function_result&& o) noexcept
		: L(o.L), index(o.index), returncount(o.returncount), popcount(o.popcount), err(o.err) {
			// Must be manual, otherwise destructor will screw us
			// return count being 0 is enough to keep things clean
			// but we will be thorough
			o.abandon();
		}
		protected_function_result& operator=(protected_function_result&& o) noexcept {
			L = o.L;
			index = o.index;
			returncount = o.returncount;
			popcount = o.popcount;
			err = o.err;
			// Must be manual, otherwise destructor will screw us
			// return count being 0 is enough to keep things clean
			// but we will be thorough
			o.abandon();
			return *this;
		}

		protected_function_result(const unsafe_function_result& o) = delete;
		protected_function_result& operator=(const unsafe_function_result& o) = delete;
		protected_function_result(unsafe_function_result&& o) noexcept;
		protected_function_result& operator=(unsafe_function_result&& o) noexcept;

		call_status status() const noexcept {
			return err;
		}

		bool valid() const noexcept {
			return status() == call_status::ok || status() == call_status::yielded;
		}

#if SOL_IS_ON(SOL_COMPILER_GCC)
#pragma GCC diagnostic push
#if !SOL_IS_ON(SOL_COMPILER_CLANG)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif

		template <typename T>
		decltype(auto) get(int index_offset = 0) const {
			using UT = meta::unqualified_t<T>;
			int target = index + index_offset;
			if constexpr (meta::is_optional_v<UT>) {
				using ValueType = typename UT::value_type;
				if constexpr (std::is_same_v<ValueType, error>) {
					if (valid()) {
						return UT();
					}
					return UT(stack::stack_detail::get_error(L, target));
				}
				else {
					if (!valid()) {
						return UT();
					}
					return stack::get<UT>(L, target);
				}
			}
			else {
				if constexpr (std::is_same_v<T, error>) {
#if SOL_IS_ON(SOL_SAFE_PROXIES)
					if (valid()) {
						type t = type_of(L, target);
						type_panic_c_str(L, target, t, type::none, "bad get from protected_function_result (is an error)");
					}
#endif // Check Argument Safety
					return stack::stack_detail::get_error(L, target);
				}
				else {
#if SOL_IS_ON(SOL_SAFE_PROXIES)
					if (!valid()) {
						type t = type_of(L, target);
						type_panic_c_str(L, target, t, type::none, "bad get from protected_function_result (is not an error)");
					}
#endif // Check Argument Safety
					return stack::get<T>(L, target);
				}
			}
		}

#if SOL_IS_ON(SOL_COMPILER_GCC)
#pragma GCC diagnostic pop
#endif

		type get_type(int index_offset = 0) const noexcept {
			return type_of(L, index + static_cast<int>(index_offset));
		}

		stack_proxy operator[](difference_type index_offset) const {
			return stack_proxy(L, index + static_cast<int>(index_offset));
		}

		iterator begin() {
			return iterator(L, index, stack_index() + return_count());
		}
		iterator end() {
			return iterator(L, stack_index() + return_count(), stack_index() + return_count());
		}
		const_iterator begin() const {
			return const_iterator(L, index, stack_index() + return_count());
		}
		const_iterator end() const {
			return const_iterator(L, stack_index() + return_count(), stack_index() + return_count());
		}
		const_iterator cbegin() const {
			return begin();
		}
		const_iterator cend() const {
			return end();
		}

		reverse_iterator rbegin() {
			return std::reverse_iterator<iterator>(begin());
		}
		reverse_iterator rend() {
			return std::reverse_iterator<iterator>(end());
		}
		const_reverse_iterator rbegin() const {
			return std::reverse_iterator<const_iterator>(begin());
		}
		const_reverse_iterator rend() const {
			return std::reverse_iterator<const_iterator>(end());
		}
		const_reverse_iterator crbegin() const {
			return std::reverse_iterator<const_iterator>(cbegin());
		}
		const_reverse_iterator crend() const {
			return std::reverse_iterator<const_iterator>(cend());
		}

		lua_State* lua_state() const noexcept {
			return L;
		};
		int stack_index() const noexcept {
			return index;
		};
		int return_count() const noexcept {
			return returncount;
		};
		int pop_count() const noexcept {
			return popcount;
		};
		void abandon() noexcept {
			// L = nullptr;
			index = 0;
			returncount = 0;
			popcount = 0;
			err = call_status::runtime;
		}
		~protected_function_result() {
			if (L == nullptr)
				return;
			stack::remove(L, index, popcount);
		}
	};

	namespace stack {
		template <>
		struct unqualified_pusher<protected_function_result> {
			static int push(lua_State* L, const protected_function_result& pfr) {
#if SOL_IS_ON(SOL_SAFE_STACK_CHECK)
				luaL_checkstack(L, static_cast<int>(pfr.pop_count()), detail::not_enough_stack_space_generic);
#endif // make sure stack doesn't overflow
				int p = 0;
				for (int i = 0; i < pfr.pop_count(); ++i) {
					lua_pushvalue(L, i + pfr.stack_index());
					++p;
				}
				return p;
			}
		};
	} // namespace stack
} // namespace sol

#endif // SOL_PROTECTED_FUNCTION_RESULT_HPP
