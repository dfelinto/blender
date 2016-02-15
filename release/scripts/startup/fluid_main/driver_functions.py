# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

def IF(statement,true,false):
    """ Returns true if statement is true Returns false if statement is false:
        statement - conditional statement
        true - value to return if statement is True
        false - value to return if statement is False
    """
    if statement == True:
        return true
    else:
        return false

def OR(*vars):
    for var in vars:
        if var:
            return True
    return False

def AND(*vars):
    for var in vars:
        if not var:
            return False
    return True

def EQ1(opening_quantity,start_point,end_point):
    """ Returns equal spacing based on the quantity and start and end point:
        Par1 - opening_quantity - Number of spliters in opening
        Par2 - start_point - Start point to calculate opening size (always smaller number)
        Par3 - end_point - End point to calculate opening size (always larger number)
    """
    opening_size = end_point-start_point
    if opening_quantity == 0:
        return 0
    else:
        mid_point = opening_size/(opening_quantity + 1)
    return mid_point

def INCH(value):
    return value * .0254

def LIMIT(val,val_min,val_max):
    if val>val_max:
        return val_max
    elif val<val_min:
        return val_min
    else:
        return val
    
    
def PERCENTAGE(value,min,max):
    return (value - min)/(max - min)
    

