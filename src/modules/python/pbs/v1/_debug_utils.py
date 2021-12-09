# coding: utf-8
"""

# Copyright (C) 1994-2021 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of both the OpenPBS software ("OpenPBS")
# and the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# OpenPBS is free software. You can redistribute it and/or modify it under
# the terms of the GNU Affero General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# OpenPBS is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# PBS Pro is commercially licensed software that shares a common core with
# the OpenPBS software.  For a copy of the commercial license terms and
# conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
# Altair Legal Department.
#
# Altair's dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of OpenPBS and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair's trademarks, including but not limited to "PBS™",
# "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
# subject to Altair's trademark licensing policies.


"""
__doc__ = """
Python classes for debugging the PBS hooks subsystem.
"""

import inspect
import weakref
import _pbs_v1


def logmsgstk(
        obj, msg=None, errlvl=_pbs_v1.LOG_ERROR, include_c_stack=False,
        remove_n_entries=1):
    cls = obj.__class__
    # NOTE: this is not thread-safe
    try:
        msg_cnt = cls.__logmsgstk_cnt
    except AttributeError:
        msg_cnt = 0
    cls.__logmsgstk_cnt = msg_cnt + 1
    if msg:
        _pbs_v1.logmsg(
            errlvl,
            f"{cls.__name__}:{hex(id(obj))}:{msg_cnt}:{msg}")
    py_stack = inspect.stack()
    for _ in range(remove_n_entries):
        frame, file_name, line_num, func_name, lines, index = py_stack.pop(0)
        del frame
    c_stack = []
    if include_c_stack:
        c_stack = _pbs_v1._get_c_stack()
    frame_num = len(py_stack) + len(c_stack)
    while py_stack:
        frame_num -= 1
        frame, file_name, line_num, func_name, lines, index = py_stack.pop(0)
        _pbs_v1.logmsg(
            errlvl,
            f"{cls.__name__}:{hex(id(obj))}:{msg_cnt}[{frame_num}]:"
            f"{file_name}:{line_num}:{func_name}:{lines}")
        del frame
    if c_stack:
        while c_stack:
            frame_num -= 1
            _pbs_v1.logmsg(
                errlvl,
                f"{cls.__name__}:{hex(id(obj))}:{msg_cnt}[{frame_num}]:"
                f"C-code:{c_stack.pop(0)}")


class LogMutableMapping():
    def __init__(self, *args, **kwargs):
        self_mro = self.__class__.mro()
        super_mro = self_mro[self_mro.index(__class__)+1:]
        super_cls_names = \
            [repr(cls) for cls in super_mro if hasattr(cls, '__init__')]
        self._logmsgstk(
            f"LogMutableMapping calling init method of {super_cls_names[0]}")
        super().__init__(*args, **kwargs)

    def _logmsgstk(
            self, msg=None, errlvl=_pbs_v1.LOG_ERROR, include_c_stack=False,
            remove_n_entries=2):
        logmsgstk(
            self, msg=msg, errlvl=errlvl, include_c_stack=include_c_stack,
            remove_n_entries=remove_n_entries)

    def __class_getitem__(self, key):
        try:
            value = super().__class_getitem__(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            return value

    def __contains__(self, key):
        contains = super().__contains__(key)
        self._logmsgstk(
            f'key="{key}",key-id={hex(id(key))},contains="{contains}"')
        return contains

    def __delattr__(self, key):
        try:
            super().__delattr__(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(f'key="{key}",key-id={hex(id(key))}')

    def __delitem__(self, key):
        try:
            super().__delitem__(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(f'key="{key}",key-id={hex(id(key))}')

    def __getattr__(self, key):
        try:
            value = super().__getattr__(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            return value

    def __getttribute__(self, key):
        try:
            value = super().__getattribute__(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            return value

    def __getitem__(self, key):
        try:
            value = super().__getitem__(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            return value

    def __ior__(self, other):
        self._logmsgstk()
        super().__ior__(dict, other)

    def __iter__(self):
        self._logmsgstk()
        for key in super().__iter__():
            self._logmsgstk(f'key="{key}",key-id={hex(id(key))}')
            yield key

    def __len__(self):
        dlen = super().__len__()
        self._logmsgstk(f"len={dlen}")
        return dlen

    def __or__(self, other):
        self._logmsgstk()
        super().__or__(dict, other)

    def __repr__(self):
        drepr = super().__repr__()
        self._logmsgstk(f"repr={drepr}")
        return drepr

    def __ror__(self, other):
        self._logmsgstk()
        super().__ror__(dict, other)

    def __str__(self):
        try:
            dstr = super().__str__()
        except Exception as e:
            self._logmsgstk(f'EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(f"str={dstr}")
            return dstr

    def __setattr__(self, key, value):
        self._logmsgstk(f'key="{key}",key-id={hex(id(key))},value="{value}"')
        super().__setattr__(key, value)

    def __setitem__(self, key, value):
        self._logmsgstk(f'key="{key}",key-id={hex(id(key))},value="{value}"')
        super().__setitem__(key, value)

    def copy(self):
        self._logmsgstk()
        return super().copy()

    def get(self, key, default=None):
        value = super().get(key, default)
        self._logmsgstk(f'key="{key}",key-id={hex(id(key))},value="{value}"')
        return value

    def items(self):
        self._logmsgstk()
        for key, value in super().keys():
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            yield key, value

    def keys(self):
        self._logmsgstk()
        for key in super().keys():
            self._logmsgstk(f'key="{key}",key-id={hex(id(key))}')
            yield key

    def pop(self, key, *args):
        try:
            value = super().pop(key)
        except Exception as e:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            return value

    def popitem(self):
        try:
            key, value = super().popitem()
        except Exception as e:
            self._logmsgstk(f'EXCEPTION: {str(e)}"')
            raise
        else:
            self._logmsgstk(
                f'key="{key}",key-id={hex(id(key))},value="{value}"')
            return key, value

    def setdefault(self, key, default=None):
        self._logmsgstk(
            f'key="{key}",key-id={hex(id(key))},default="{default}"')
        return super().setdefault(key, default)

    def update(self, dict=None, **kwargs):
        self._logmsgstk()
        super().update(dict, **kwargs)

    def values(self):
        self._logmsgstk()
        for value in super().values():
            self._logmsgstk(f'value="{value}"')
            yield value


class LogWeakKeyDictionary(LogMutableMapping, weakref.WeakKeyDictionary):
    def __init__(self, *args, **kwargs):
        def remove(key, selfref=weakref.ref(self)):
            self._logmsgstk(f'key="{key}",key-id={hex(id(key))}')
            # Uncomment below to cause segmenation fault in when the weakref
            # is about to be removed from the dictionary
            # os.abort()
            _pbs_v1._gdb_bp()
        super().__init__(*args, **kwargs)
        self._remove = remove

    def __copy__(self):
        self._logmsgstk()
        return super().__copy__()

    def __deepcopy__(self, memo):
        self._logmsgstk()
        return super().__deepcopy__(memo)

    def _commit_removals(self):
        self._logmsgstk()
        super()._commit_removals()

    def _scrub_removals(self):
        self._logmsgstk()
        super()._scrub_removals()

    def keyrefs(self):
        self._logmsgstk()
        return super().keyrefs()


class LogStrongKeyDictionary(LogMutableMapping, dict):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    # @classmethod
    # def fromkeys(self, keys, value=None):
    #     self._logmsgstk()
    #     return super().fromkeys(keys, value)
