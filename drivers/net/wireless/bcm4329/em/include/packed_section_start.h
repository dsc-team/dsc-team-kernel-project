/*
 * Declare directives for structure packing. No padding will be provided
 * between the members of packed structures, and therefore, there is no
 * guarantee that structure members will be aligned.
 *
 * Declaring packed structures is compiler specific. In order to handle all
 * cases, packed structures should be delared as:
 *
 * #include <packed_section_start.h>
 *
 * typedef BWL_PRE_PACKED_STRUCT struct foobar_t {
 *    some_struct_members;
 * } BWL_POST_PACKED_STRUCT foobar_t;
 *
 * #include <packed_section_end.h>
 *
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: packed_section_start.h,v 1.1.6.3 2008/12/10 00:27:54 Exp $
 */




#ifdef BWL_PACKED_SECTION
	#error "BWL_PACKED_SECTION is already defined!"
#else
	#define BWL_PACKED_SECTION
#endif


#if defined(_MSC_VER)
	
	#pragma warning(disable:4103)

	
	#if defined(BWL_DEFAULT_PACKING)
		
		#pragma pack(push, 8)
	#else   
		#pragma pack(1)
	#endif   
#endif   



#if defined(_MSC_VER)
	#define	BWL_PRE_PACKED_STRUCT
	#define	BWL_POST_PACKED_STRUCT
#elif defined(__GNUC__)
	#define	BWL_PRE_PACKED_STRUCT
	#define	BWL_POST_PACKED_STRUCT	__attribute__((packed))
#elif defined(__CC_ARM)
	#define	BWL_PRE_PACKED_STRUCT	__packed
	#define	BWL_POST_PACKED_STRUCT
#else
	#error "Unknown compiler!"
#endif
