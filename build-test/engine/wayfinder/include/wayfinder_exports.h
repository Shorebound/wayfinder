
#ifndef WAYFINDER_API_H
#define WAYFINDER_API_H

#ifdef WAYFINDER_STATIC_DEFINE
#  define WAYFINDER_API
#  define WAYFINDER_NO_EXPORT
#else
#  ifndef WAYFINDER_API
#    ifdef wayfinder_EXPORTS
        /* We are building this library */
#      define WAYFINDER_API 
#    else
        /* We are using this library */
#      define WAYFINDER_API 
#    endif
#  endif

#  ifndef WAYFINDER_NO_EXPORT
#    define WAYFINDER_NO_EXPORT 
#  endif
#endif

#ifndef WAYFINDER_DEPRECATED
#  define WAYFINDER_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef WAYFINDER_DEPRECATED_EXPORT
#  define WAYFINDER_DEPRECATED_EXPORT WAYFINDER_API WAYFINDER_DEPRECATED
#endif

#ifndef WAYFINDER_DEPRECATED_NO_EXPORT
#  define WAYFINDER_DEPRECATED_NO_EXPORT WAYFINDER_NO_EXPORT WAYFINDER_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef WAYFINDER_NO_DEPRECATED
#    define WAYFINDER_NO_DEPRECATED
#  endif
#endif

#endif /* WAYFINDER_API_H */
