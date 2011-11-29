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
 * $Id: packed_section_end.h,v 1.1.6.3 2008/12/10 00:27:54 Exp $
 */




#ifdef BWL_PACKED_SECTION
	#undef BWL_PACKED_SECTION
#else
	#error "BWL_PACKED_SECTION is NOT defined!"
#endif


#if defined(_MSC_VER)
	
	#pragma warning(disable:4103)

	
	#if defined(BWL_DEFAULT_PACKING)
		
		#pragma pack(pop)
		#undef BWL_DEFAULT_PACKING
	#else   
		#pragma pack()
	#endif   
#endif   



#undef	BWL_PRE_PACKED_STRUCT
#undef	BWL_POST_PACKED_STRUCT
