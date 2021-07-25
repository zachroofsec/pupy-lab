# -*- coding: utf8 -*-

# This file is part of PywerView.

# PywerView is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# PywerView is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with PywerView.  If not, see <http://www.gnu.org/licenses/>.

# Yannick Méheut [yannick (at) meheut (dot) org] - Copyright © 2016

from __future__ import unicode_literals

import inspect

class RPCObject:
    def __init__(self, obj):
        attributes = dict()
        try:
            for key in obj.fields.keys():
                attributes[key] = obj[key]
        except AttributeError:
            attributes = obj
        self.add_atributes(attributes)

    def add_atributes(self, attributes):
        for key, value in attributes.items():
            #if isinstance(value, int):
                #pass
            #else:
                #value = value.rstrip('\x00')

            setattr(self, key.lower(), value)

    def __str__(self):
        s = str()
        members = inspect.getmembers(self, lambda x: not(inspect.isroutine(x)))
        max_length = 0
        for member in members:
            if not member[0].startswith('_'):
                if len(member[0]) > max_length:
                    max_length = len(member[0])
        for member in members:
            if not member[0].startswith('_'):
                s += '{}: {}{}\n'.format(member[0], ' ' * (max_length - len(member[0])), member[1])

        s = s[:-1].encode('utf-8')
        return s

    def __repr__(self):
        return str(self)

class TargetUser(RPCObject):
    pass

class Session(RPCObject):
    pass

class Share(RPCObject):
    pass

class WkstaUser(RPCObject):
    pass

class Group(RPCObject):
    pass

class Disk(RPCObject):
    pass
