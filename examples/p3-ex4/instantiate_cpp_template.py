#!/usr/bin/env python3

import ctypes
import os

libpath = os.path.dirname(__file__) + "/../../build/lib/libp3-ex4-lib.so"

libInterop = ctypes.CDLL(libpath, mode = ctypes.RTLD_GLOBAL)
_cpp_compile = libInterop.Clang_Parse
_cpp_compile.argtypes = [ctypes.c_char_p]

def cpp_compile(arg):
    return _cpp_compile(arg.encode("ascii"))

# define some classes to play with
cpp_compile(r"""\
void* operator new(__SIZE_TYPE__, void* __p) noexcept;
extern "C" int printf(const char*,...);
class A {};
class C {};
struct B : public A {
  template<typename T, typename S, typename U>
  void callme(T, S, U*) { printf(" call me may B! \n"); }
};
""")

class InterOpLayerWrapper:
  # Responsible to provide a python wrapper over the interop layer.
  _get_scope = libInterop.Clang_LookupName
  _get_scope.restype = ctypes.c_size_t
  _get_scope.argtypes = [ctypes.c_char_p]

  _construct = libInterop.Clang_CreateObject
  _construct.restype = ctypes.c_void_p
  _construct.argtypes = [ctypes.c_size_t]

  _get_template_ct = libInterop.Clang_InstantiateTemplate
  _get_template_ct.restype = ctypes.c_size_t
  _get_template_ct.argtypes = [ctypes.c_size_t, ctypes.c_char_p, ctypes.c_char_p]

  def _get_template(self, scope, name, args):
    return self._get_template_ct(scope, name.encode("ascii"), args.encode("ascii"))

  def get_scope(self, name):
    return self._get_scope(name.encode("ascii"))

  def get_template(self, scope, name, tmpl_args = [], tpargs = []):
    if tmpl_args:
      # Instantiation is explicit from full name
      full_name = name + '<' + ', '.join([a for a in tmpl_args]) + '>'
      meth = self._get_template(scope, full_name, '')
    elif tpargs:
      # Instantiation is implicit from argument types
      meth = self._get_template(scope, name, ', '.join([a.__name__ for a in tpargs]))
    return CallCPPFunc(meth)
    
  def construct(self, cpptype):
    return self._construct(cpptype)

class TemplateWrapper:
  # Responsible for finding a template which matches the arguments.
  def __init__(self, scope, name):
    self._scope = scope
    self._name  = name

  def __getitem__(self, *args, **kwds):
    # Look up the template and return the overload.
    return gIL.get_template(
      self._scope, self._name, tmpl_args = args)

  def __call__(self, *args, **kwds):
    # Keyword arguments are not supported for this demo.
    assert not kwds

    # Construct the template arguments from the types and find the overload.
    ol = gIL.get_template(
      self._scope, self._name, tpargs = [type(a) for a in args])

    # Call actual method.
    ol(*args, **kwds)


class CallCPPFunc:
  # Responsible for calling low-level function pointers.
  _get_funcptr = libInterop.Clang_GetFunctionAddress
  _get_funcptr.restype = ctypes.c_void_p
  _get_funcptr.argtypes = [ctypes.c_size_t]

  def __init__(self, func):
    # In real life this would normally go through the interop layer to know
    # whether to pass pointer, reference, or value of which type etc.
    proto = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)
    self._funcptr = proto(self._get_funcptr(func))

  def __call__(self, *args, **kwds):
    # See the comment above.
    a0 = ctypes.cast(args[0].cppobj, ctypes.POINTER(ctypes.c_void_p))
    a1 = args[1]
    a2 = args[2].cppobj
    return self._funcptr(a0, a1, a2)

gIL = InterOpLayerWrapper()

def cpp_allocate(proxy):
  pyobj = object.__new__(proxy)
  proxy.__init__(pyobj)
  pyobj.cppobj = gIL.construct(proxy.handle)
  return pyobj


if __name__ == '__main__':
  # create a couple of types to play with
  CppA = type('A', (), {
      'handle'  : gIL.get_scope('A'),
      '__new__' : cpp_allocate
  })
  h = gIL.get_scope('B')
  CppB = type('B', (CppA,), {
      'handle'  : h,
      '__new__' : cpp_allocate,
      'callme'  : TemplateWrapper(h, 'callme')
  })
  CppC = type('C', (), {
      'handle'  : gIL.get_scope('C'),
      '__new__' : cpp_allocate
  })

  # call templates
  a = CppA()
  b = CppB()
  c = CppC()

  # explicit template instantiation
  b.callme['A, int, C*'](a, 42, c)

  # implicit template instantiation
  b.callme(a, 42, c)

  # Based on thas approach we can make this work: std.vector['int'] v = ...;
