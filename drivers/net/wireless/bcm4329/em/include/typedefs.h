/*
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: typedefs.h,v 1.85.34.1.2.5 2009/01/27 04:09:40 Exp $
 */


#ifndef _TYPEDEFS_H_
#define _TYPEDEFS_H_

#ifdef SITE_TYPEDEFS



#include "site_typedefs.h"

#else



#ifdef __cplusplus

#define TYPEDEF_BOOL
#ifndef FALSE
#define FALSE	false
#endif
#ifndef TRUE
#define TRUE	true
#endif

#else	

#if defined(_WIN32)

#define TYPEDEF_BOOL
typedef	unsigned char	bool;			

#endif 

#endif	

#if defined(_WIN64) && !defined(EFI)

#include <basetsd.h>
#define TYPEDEF_UINTPTR
typedef ULONG_PTR uintptr;
#elif defined(__x86_64__)
#define TYPEDEF_UINTPTR
typedef unsigned long long int uintptr;
#endif

#if defined(_HNDRTE_) && !defined(_HNDRTE_SIM_)
#define _NEED_SIZE_T_
#endif

#if defined(_MINOSL_)
#define _NEED_SIZE_T_
#endif

#if defined(EFI) && !defined(_WIN64)
#define _NEED_SIZE_T_
#endif

#if defined(TARGETOS_nucleus)

#include <stddef.h>


#define TYPEDEF_FLOAT_T
#endif   

#if defined(_NEED_SIZE_T_)
typedef long unsigned int size_t;
#endif

#ifdef __DJGPP__
typedef long unsigned int size_t;
#endif 

#ifdef _MSC_VER	    
#define TYPEDEF_INT64
#define TYPEDEF_UINT64
typedef signed __int64	int64;
typedef unsigned __int64 uint64;
#endif

#if defined(MACOSX)
#define TYPEDEF_BOOL
#endif

#if defined(__NetBSD__)
#define TYPEDEF_ULONG
#endif

#if defined(vxworks)
#define TYPEDEF_USHORT
#endif

#ifdef	linux
#define TYPEDEF_UINT
#ifndef TARGETENV_android
#define TYPEDEF_USHORT
#define TYPEDEF_ULONG
#endif 
#ifdef __KERNEL__
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19))
#define TYPEDEF_BOOL
#endif	
#endif	
#endif	

#if defined(__ECOS)
#define TYPEDEF_UINT
#define TYPEDEF_USHORT
#define TYPEDEF_ULONG
#define TYPEDEF_BOOL
#endif

#if !defined(linux) && !defined(vxworks) && !defined(_WIN32) && !defined(_CFE_) && \
	!defined(_HNDRTE_) && !defined(_MINOSL_) && !defined(__DJGPP__) && !defined(__ECOS) && \
	!defined(__BOB__) && !defined(TARGETOS_nucleus)
#define TYPEDEF_UINT
#define TYPEDEF_USHORT
#endif

#if defined(vxworks)

#define TYPEDEF_INT64
#endif


#if defined(__GNUC__) && defined(__STRICT_ANSI__)
#define TYPEDEF_INT64
#define TYPEDEF_UINT64
#endif


#if defined(__ICL)

#define TYPEDEF_INT64

#if defined(__STDC__)
#define TYPEDEF_UINT64
#endif

#endif 

#if !defined(_WIN32) && !defined(_CFE_) && !defined(_HNDRTE_) && !defined(_MINOSL_) && \
	!defined(__DJGPP__) && !defined(__BOB__) && !defined(TARGETOS_nucleus)


#if defined(linux) && defined(__KERNEL__)

#include <linux/types.h>	

#else

#if defined(__ECOS)
#include <pkgconf/infra.h>
#include <cyg/infra/cyg_type.h>
#include <stdarg.h>
#endif

#include <sys/types.h>

#endif 

#endif 

#if defined(MACOSX)

#ifdef __BIG_ENDIAN__
#define IL_BIGENDIAN
#else
#ifdef IL_BIGENDIAN
#error "IL_BIGENDIAN was defined for a little-endian compile"
#endif
#endif 

#if !defined(__cplusplus)

#if defined(__i386__)
typedef unsigned char bool;
#else
typedef unsigned int bool;
#endif
#define TYPE_BOOL 1
enum {
    false	= 0,
    true	= 1
};

#if defined(KERNEL)
#include <IOKit/IOTypes.h>
#endif 

#endif 

#endif 

#if defined(vxworks)
#include <private/cplusLibP.h>
#endif


#define USE_TYPEDEF_DEFAULTS

#endif 




#ifdef USE_TYPEDEF_DEFAULTS
#undef USE_TYPEDEF_DEFAULTS

#ifndef TYPEDEF_BOOL
typedef	 unsigned char	bool;
#endif



#ifndef TYPEDEF_UCHAR
typedef unsigned char	uchar;
#endif

#ifndef TYPEDEF_USHORT
typedef unsigned short	ushort;
#endif

#ifndef TYPEDEF_UINT
typedef unsigned int	uint;
#endif

#ifndef TYPEDEF_ULONG
typedef unsigned long	ulong;
#endif



#ifndef TYPEDEF_UINT8
typedef unsigned char	uint8;
#endif

#ifndef TYPEDEF_UINT16
typedef unsigned short	uint16;
#endif

#ifndef TYPEDEF_UINT32
typedef unsigned int	uint32;
#endif

#ifndef TYPEDEF_UINT64
typedef unsigned long long uint64;
#endif

#ifndef TYPEDEF_UINTPTR
typedef unsigned int	uintptr;
#endif

#ifndef TYPEDEF_INT8
typedef signed char	int8;
#endif

#ifndef TYPEDEF_INT16
typedef signed short	int16;
#endif

#ifndef TYPEDEF_INT32
typedef signed int	int32;
#endif

#ifndef TYPEDEF_INT64
typedef signed long long int64;
#endif



#ifndef TYPEDEF_FLOAT32
typedef float		float32;
#endif

#ifndef TYPEDEF_FLOAT64
typedef double		float64;
#endif



#ifndef TYPEDEF_FLOAT_T

#if defined(FLOAT32)
typedef float32 float_t;
#else 
typedef float64 float_t;
#endif

#endif 



#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE	1  
#endif

#ifndef NULL
#define	NULL	0
#endif

#ifndef OFF
#define	OFF	0
#endif

#ifndef ON
#define	ON	1  
#endif

#define	AUTO	(-1) 



#ifndef PTRSZ
#define	PTRSZ	sizeof(char*)
#endif



#ifdef _MSC_VER
	#define BWL_COMPILER_MICROSOFT
#elif defined(__GNUC__)
	#define BWL_COMPILER_GNU
#elif defined(__CC_ARM)
	#define BWL_COMPILER_ARMCC
#else
	#error "Unknown compiler!"
#endif 


#ifndef INLINE
	#if defined(BWL_COMPILER_MICROSOFT)
		#define INLINE __inline
	#elif defined(BWL_COMPILER_GNU)
		#define INLINE __inline__
	#elif defined(BWL_COMPILER_ARMCC)
		#define INLINE	__inline
	#else
		#define INLINE
	#endif 
#endif 

#undef TYPEDEF_BOOL
#undef TYPEDEF_UCHAR
#undef TYPEDEF_USHORT
#undef TYPEDEF_UINT
#undef TYPEDEF_ULONG
#undef TYPEDEF_UINT8
#undef TYPEDEF_UINT16
#undef TYPEDEF_UINT32
#undef TYPEDEF_UINT64
#undef TYPEDEF_UINTPTR
#undef TYPEDEF_INT8
#undef TYPEDEF_INT16
#undef TYPEDEF_INT32
#undef TYPEDEF_INT64
#undef TYPEDEF_FLOAT32
#undef TYPEDEF_FLOAT64
#undef TYPEDEF_FLOAT_T

#endif 


#define UNUSED_PARAMETER(x) (void)(x)


#include <bcmdefs.h>

#endif 
