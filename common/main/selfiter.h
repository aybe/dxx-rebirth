/*
 * This file is part of the DXX-Rebirth project <http://www.dxx-rebirth.com/>.
 * It is copyright by its individual contributors, as recorded in the
 * project's Git history.  See COPYING.txt at the top level for license
 * terms and a link to the Git history.
 */
#pragma once

#include <iterator>
#include <utility>

template <typename T>
class self_return_iterator :
	public std::iterator<std::forward_iterator_tag, T>,
	T
{
public:
	self_return_iterator(T &&i) :
		T(std::move(i))
	{
	}
	T base() const
	{
		return *this;
	}
	T operator*() const
	{
		return *this;
	}
	/* Some STL algorithms require:
	 *
	 *	!!std::is_same<decltype(iter), decltype(++iter)>::value
	 *
	 * If this requirement is not met, template argument deduction
	 * fails when the algorithm calls a helper function.
	 *
	 * If not for this requirement, `using T::operator++` would have
	 * been sufficient here.
	 */
	self_return_iterator &operator++()
	{
		/* Use a static_cast instead of ignoring the return value and
		 * returning `*this`, to encourage the compiler to implement
		 * this as a tail-call when
		 *
		 *	offsetof(self_return_iterator, T) == 0
		 */
		return static_cast<self_return_iterator &>(this->T::operator++());
	}
	/* operator++(int) is currently unused, but is required to satisfy
	 * the concept check on forward iterator.
	 */
	self_return_iterator operator++(int)
	{
		auto result = *this;
		this->T::operator++();
		return result;
	}
	/* Since `T` is inherited privately, the base class `operator==` and
	 * `operator!=` cannot implicitly convert `rhs` to `T`.  Define
	 * comparison operators to perform the conversion explicitly.
	 */
	bool operator==(const self_return_iterator &rhs) const
	{
		return this->T::operator==(static_cast<const T &>(rhs));
	}
	bool operator!=(const self_return_iterator &rhs) const
	{
		return this->T::operator!=(static_cast<const T &>(rhs));
	}
};
