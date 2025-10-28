/*
 * Copyright © 2025 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

/* Some parts copied from glib/gobject/gtype.h:
 *
 * GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright © 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 */

#pragma once

#include <glib-object.h>

/*
 * _VTE_DEFINE_TYPE:
 * @TN: The name of the new type, in Camel case.
 * @t_n: The name of the new type, in lowercase, with words
 *  separated by `_`.
 * @T_P: The #GType of the parent type.
 *
 * Like G_DEFINE_TYPE, but for C++ private class.
 */
#define _VTE_DEFINE_TYPE(TN, t_n, T_P, PTN) _VTE_DEFINE_TYPE_EXTENDED(TN, t_n, T_P, PTN, 0, {})

/**
 * _VTE_DEFINE_TYPE_WITH_CODE:
 * @TN: The name of the new type, in Camel case.
 * @t_n: The name of the new type in lowercase, with words separated by `_`.
 * @T_P: The #GType of the parent type.
 * @_C_: Custom code that gets inserted in the `*_get_type()` function.
 *
 * Like G_DEFINE_TYPE_WITH_CODE, but for C++ private class.
 */
#define _VTE_DEFINE_TYPE_WITH_CODE(TN, t_n, T_P, PTN, _C_) _VTE_DEFINE_TYPE_EXTENDED_BEGIN(TN, t_n, T_P, PTN, 0) {_C_;} _VTE_DEFINE_TYPE_EXTENDED_END()

/**
 * _VTE_DEFINE_TYPE_EXTENDED:
 * @TN: The name of the new type, in Camel case.
 * @t_n: The name of the new type, in lowercase, with words
 *    separated by `_`.
 * @T_P: The #GType of the parent type.
 * @PTN: the name of the private type
 * @_f_: #GTypeFlags to pass to g_type_register_static()
 * @_C_: Custom code that gets inserted in the `*_get_type()` function.
 *
 * The most general convenience macro for type implementations, on which
 * _VTE_DEFINE_TYPE(), etc are based.
 *
 * The only pieces which have to be manually provided are the definitions of
 * the instance, and gobject class structures, the private class definition,
 * and the definition of the gobject class init function.
 */
#define _VTE_DEFINE_TYPE_EXTENDED(TN, t_n, T_P, PTN,_f_, _C_) _VTE_DEFINE_TYPE_EXTENDED_BEGIN (TN, t_n, T_P, PTN, _f_) {_C_;} _VTE_DEFINE_TYPE_EXTENDED_END()

#define _VTE_DEFINE_TYPE_EXTENDED_BEGIN_PRE(TypeName, type_name, PrivateTypeName) \
\
static void     type_name##_class_init        (TypeName##Class *klass); \
static GType    type_name##_get_type_once     (void); \
static void* type_name##_parent_class = NULL; \
static gint     TypeName##_private_offset; \
\
G_GNUC_UNUSED \
static inline void* \
type_name##_get_instance_private (TypeName *self) \
{ \
  return (G_STRUCT_MEMBER_P (self, TypeName##_private_offset)); \
} \
\
template<typename T> \
static inline PrivateTypeName* \
type_name##_get_impl(T* that) \
{ \
  auto const tn_that = reinterpret_cast<TypeName*>(that); \
  return std::launder(reinterpret_cast<PrivateTypeName*>(type_name##_get_instance_private(tn_that))); \
} \
\
template<typename T> \
static inline PrivateTypeName const* \
type_name##_get_impl(T const* that) \
{ \
  return const_cast<PrivateTypeName const*>(type_name##_get_impl(const_cast<T*>(that))); \
} \
\
static void type_name##_init(TypeName* self) noexcept \
try \
{                                                          \
  auto const ptr = type_name##_get_instance_private(self); \
  new (ptr) PrivateTypeName{self}; \
} \
catch (...) \
{ \
  vte::log_exception(); \
\
  /* There's not really anything we can do after the */ \
  /* construction of PrivateTypeName failed... we'll crash soon anyway. */ \
  g_error(#PrivateTypeName " constructor threw\n"); \
} \
\
static void type_name##_finalize(GObject* object) noexcept \
{ \
  auto const self = reinterpret_cast<TypeName*>(object); \
  auto impl = type_name##_get_impl(self); \
  impl->~PrivateTypeName(); \
  G_OBJECT_CLASS(type_name##_parent_class)->finalize(object); \
} \
\
static void type_name##_class_intern_init(void* klass) noexcept \
{ \
  type_name##_parent_class = g_type_class_peek_parent (klass); \
  if (TypeName##_private_offset != 0) \
    g_type_class_adjust_private_offset (klass, &TypeName##_private_offset); \
  type_name##_class_init ((TypeName##Class*) klass); \
  auto gobject_class = G_OBJECT_CLASS(klass); \
  gobject_class->finalize = type_name##_finalize; \
} \
\
GType \
type_name##_get_type (void) \
{ \
  static _g_type_once_init_type static_g_define_type_id = 0;
  /* Prelude goes here */

/* Added for _VTE_DEFINE_TYPE_EXTENDED_WITH_PRELUDE */
#define _VTE_DEFINE_TYPE_EXTENDED_BEGIN_REGISTER(TypeName, type_name, TYPE_PARENT, PrivateTypeName, flags) \
  if (_g_type_once_init_enter (&static_g_define_type_id)) \
    { \
      GType g_define_type_id = type_name##_get_type_once (); \
      _g_type_once_init_leave (&static_g_define_type_id, g_define_type_id); \
    }          \
  return static_g_define_type_id; \
} /* closes type_name##_get_type() */ \
\
G_NO_INLINE \
static GType \
type_name##_get_type_once (void) \
{ \
  GType g_define_type_id = \
        g_type_register_static_simple (TYPE_PARENT, \
                                       g_intern_static_string (#TypeName), \
                                       sizeof (TypeName##Class), \
                                       (GClassInitFunc)(void (*)(void)) type_name##_class_intern_init, \
                                       sizeof (TypeName), \
                                       (GInstanceInitFunc)(void (*)(void)) type_name##_init, \
                                       (GTypeFlags) flags); \
  TypeName##_private_offset = \
     g_type_add_instance_private(g_define_type_id, sizeof(PrivateTypeName)); \
  { /* custom code follows */

#define _VTE_DEFINE_TYPE_EXTENDED_END()  \
      /* following custom code */  \
    } \
  return g_define_type_id; \
} /* closes type_name##_get_type_once() */

#define _VTE_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, PrivateTypeName, flags) \
  _VTE_DEFINE_TYPE_EXTENDED_BEGIN_PRE (TypeName, type_name, PrivateTypeName) \
  _VTE_DEFINE_TYPE_EXTENDED_BEGIN_REGISTER (TypeName, type_name, TYPE_PARENT, PrivateTypeName, flags)
