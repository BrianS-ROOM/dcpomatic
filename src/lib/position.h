/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DCPOMATIC_POSITION_H
#define DCPOMATIC_POSITION_H

/** @struct Position
 *  @brief A position (x and y coordinates)
 */
template <class T>
class Position
{
public:
	Position ()
		: x (0)
		, y (0)
	{}

	Position (T x_, T y_)
		: x (x_)
		, y (y_)
	{}

	/** x coordinate */
	T x;
	/** y coordinate */
	T y;
};

template<class T>
Position<T>
operator+ (Position<T> const & a, Position<T> const & b)
{
	return Position<T> (a.x + b.x, a.y + b.y);
}

template<class T>
Position<T>
operator- (Position<T> const & a, Position<T> const & b)
{
	return Position<T> (a.x - b.x, a.y - b.y);
}

template<class T>
bool
operator== (Position<T> const & a, Position<T> const & b)
{
	return a.x == b.x && a.y == b.y;
}

template<class T>
bool
operator!= (Position<T> const & a, Position<T> const & b)
{
	return a.x != b.x || a.y != b.y;
}

#endif
