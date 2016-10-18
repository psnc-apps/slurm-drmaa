/* $Id: lookup3.h 13 2011-04-20 15:41:43Z mmamonski $ */

#ifndef __LOOKUP3_H
#define __LOOKUP3_H

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <drmaa_utils/compat.h>

uint32_t hashword( const uint32_t *k, size_t length, uint32_t initval );
uint32_t hashlittle( const void *key, size_t length, uint32_t initval );
uint32_t hashbig( const void *key, size_t length, uint32_t initval );

#if BYTEORDER == 1234 /* little endian */
#	define hashstr hashlittle
#endif

#if BYTEORDER == 4321  /* big endian */
#	define hashstr hashbig
#endif

#endif /* __LOOKUP3_H */

