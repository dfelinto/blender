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

/** \file blender/blenlib/intern/iterator.c
 *  \ingroup bli
 *
 * Iterator defines
 */

#include <string.h>

#include "BLI_iterator.h"
#include "MEM_guardedalloc.h"

void BLI_iterator_begin(Iterator *iter, IteratorCb callback, void *data)
{
	memset(iter, 0, sizeof(*iter));
	callback(iter, data);

	if (iter->tot) {
		iter->cur = 0;
		iter->data = iter->array[iter->cur];
		iter->valid = 1;
	}
}

void BLI_iterator_next(Iterator *iter)
{
	if (++iter->cur < iter->tot) {
		iter->data = iter->array[iter->cur];
	}
	else {
		iter->valid = 0;
	}
}

void BLI_iterator_end(Iterator *iter)
{
	if (iter->array) {
		MEM_freeN(iter->array);
	}
	iter->valid = 0;
}

