/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_ITERATOR_H__
#define __BLI_ITERATOR_H__

/** \file BLI_iterator.h
 *  \ingroup bli
 */

typedef struct Iterator {
	void **array;
	int tot, cur;

	void *data;
	int valid;
} Iterator;

typedef void (*IteratorCb)(Iterator *iter, void *data);

void BLI_iterator_begin(Iterator *iter, IteratorCb callback, void *data);
void BLI_iterator_next(Iterator *iter);
void BLI_iterator_end(Iterator *iter);

#define ITER_BEGIN(callback, _data_in, _data_out)                             \
	{                                                                         \
	Iterator iter_macro;                                                      \
	for (BLI_iterator_begin(&iter_macro, callback, _data_in);                 \
	     iter_macro.valid;                                                    \
	     BLI_iterator_next(&iter_macro))                                      \
	{                                                                         \
		_data_out = iter_macro.data;

#define ITER_END                                                              \
	    }                                                                     \
	    BLI_iterator_end(&iter_macro);                                        \
	}

#endif /* __BLI_ITERATOR_H__ */
