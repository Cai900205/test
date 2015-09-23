/*
 * Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


// the type of map holding alternate object mangling function and reverse one
// note we hold it by the original mangled type
// the type of map holding alternate object mangling function and reverse one
// note we hold it by the original mangled type
struct less_char_array : public binary_function<const char *, const char *, bool> {
        bool operator() (const char * x, const char * y) const { return (strcmp(x,y) < 0); }
};

#define charp_getname_map map<const char *, int (*)(Tcl_Obj *, void *,char *type),  less_char_array >
#define charp_getobjp_map map<const char *, int (*)(Tcl_Obj *, void **), less_char_array >

// two maps - for in and out of C++ space
charp_getname_map SWIG_AlternateObjMangling;
charp_getobjp_map SWIG_AlternateNameToObj;

/*---------------------------------------------------------------------
 * void SWIG_SetPointerObj(Tcl_Obj *objPtr, void *ptr, char *type)
 *
 * Sets a Tcl object to a pointer value.
 *           ptr = void pointer value
 *           type = string representing type
 *
 *---------------------------------------------------------------------*/
SWIGSTATIC
void SWIG_SetPointerObj(Tcl_Obj *objPtr, void *_ptr, char *type) {

  // if we have an alternate mangling use it:
  charp_getname_map::const_iterator I = SWIG_AlternateObjMangling.find(type);
  if (I != SWIG_AlternateObjMangling.end()) {
	 int (*getName)(Tcl_Obj *, void *, char *type) = (*I).second;
	 if (getName(objPtr, _ptr, type)) {
		cerr << "-E- Fail to convert object to string\n";
	 }
	 return;
  }

  static char _hex[16] =
  {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	  'a', 'b', 'c', 'd', 'e', 'f'};
  unsigned long _p, _s;
  char _result[20], *_r;    /* Note : a 64-bit hex number = 16 digits */
  char _temp[20], *_c;
  _r = _result;
  _p = (unsigned long) _ptr;
  if (_p > 0) {
    while (_p > 0) {
      _s = _p & 0xf;
      *(_r++) = _hex[_s];
      _p = _p >> 4;
    }
    *_r = '_';
    _c = &_temp[0];
    while (_r >= _result)
      *(_c++) = *(_r--);
    *_c = 0;
    Tcl_SetStringObj(objPtr,_temp,-1);
  } else {
    Tcl_SetStringObj(objPtr,"NULL",-1);
  }
  if (_ptr)
    Tcl_AppendToObj(objPtr,type,-1);
}

/*---------------------------------------------------------------------
 * char *SWIG_GetPointerObj(Tcl_Interp *interp, Tcl_Obj *objPtr, void **ptr, char *type)
 *
 * Attempts to extract a pointer value from our pointer type.
 * Upon failure, returns a string corresponding to the actual datatype.
 * Upon success, returns NULL and sets the pointer value in ptr.
 *---------------------------------------------------------------------*/

SWIGSTATIC
char *SWIG_GetPointerObj(Tcl_Interp *interp, Tcl_Obj *objPtr, void **ptr, char *_t) {
  unsigned long _p;
  char temp_type[256];
  char *name;
  int  i, len;
  SwigPtrType *sp,*tp;
  SwigCacheType *cache;
  int  start, end;
  char *_c;
  _p = 0;

  // if altername mangling is defined use it:
  charp_getobjp_map::const_iterator I = SWIG_AlternateNameToObj.find(_t);
  if (I != SWIG_AlternateNameToObj.end()) {
	 int (* getObjByName)(Tcl_Obj *, void **) = (*I).second;
	 if (getObjByName(objPtr, ptr)) {
		cerr << "-E- fail to get object by name\n";
		return Tcl_GetStringFromObj(objPtr, &i);
	 }
	 return 0;
  }

  /* Extract the pointer value as a string */
  _c = Tcl_GetStringFromObj(objPtr, &i);

  /* Pointer values must start with leading underscore */
  if (*_c == '_') {
	 _c++;
	 /* Extract hex value from pointer */
	 while (*_c) {
		if ((*_c >= '0') && (*_c <= '9'))
		  _p = (_p << 4) + (*_c - '0');
		else if ((*_c >= 'a') && (*_c <= 'f'))
		  _p = (_p << 4) + ((*_c - 'a') + 10);
		else
		  break;
		_c++;
	 }

	 if (_t) {
		if (strcmp(_t,_c)) {
		  if (!SwigPtrSort) {
			 qsort((void *) SwigPtrTable, SwigPtrN, sizeof(SwigPtrType), swigsort);
			 for (i = 0; i < 256; i++) {
				SwigStart[i] = SwigPtrN;
			 }
			 for (i = SwigPtrN-1; i >= 0; i--) {
				SwigStart[(int) (SwigPtrTable[i].name[1])] = i;
			 }
			 for (i = 255; i >= 1; i--) {
				if (SwigStart[i-1] > SwigStart[i])
				  SwigStart[i-1] = SwigStart[i];
			 }
			 SwigPtrSort = 1;
			 for (i = 0; i < SWIG_CACHESIZE; i++)
				SwigCache[i].stat = 0;
		  }

		  /* First check cache for matches.  Uses last cache value as starting point */
		  cache = &SwigCache[SwigLastCache];
		  for (i = 0; i < SWIG_CACHESIZE; i++) {
			 if (cache->stat) {
				if (strcmp(_t,cache->name) == 0) {
				  if (strcmp(_c,cache->mapped) == 0) {
					 cache->stat++;
					 *ptr = (void *) _p;
					 if (cache->tp->cast) *ptr = (*(cache->tp->cast))(*ptr);
					 return (char *) 0;
				  }
				}
			 }
			 SwigLastCache = (SwigLastCache+1) & SWIG_CACHEMASK;
			 if (!SwigLastCache) cache = SwigCache;
			 else cache++;
		  }
		  /* We have a type mismatch.  Will have to look through our type
			  mapping table to figure out whether or not we can accept this datatype */

		  start = SwigStart[(int) _t[1]];
		  end = SwigStart[(int) _t[1]+1];
		  sp = &SwigPtrTable[start];
		  while (start < end) {
			 if (swigcmp(_t,sp) == 0) break;
			 sp++;
			 start++;
		  }
		  if (start >= end) sp = 0;
		  /* Try to find a match for this */
		  if (sp) {
			 while (swigcmp(_t,sp) == 0) {
				name = sp->name;
				len = sp->len;
				tp = sp->next;
				/* Try to find entry for our given datatype */
				while(tp) {
				  if (tp->len >= 255) {
					 return _c;
				  }
				  strcpy(temp_type,tp->name);
				  strncat(temp_type,_t+len,255-tp->len);
				  if (strcmp(_c,temp_type) == 0) {

					 strcpy(SwigCache[SwigCacheIndex].mapped,_c);
					 strcpy(SwigCache[SwigCacheIndex].name,_t);
					 SwigCache[SwigCacheIndex].stat = 1;
					 SwigCache[SwigCacheIndex].tp = tp;
					 SwigCacheIndex = SwigCacheIndex & SWIG_CACHEMASK;

					 /* Get pointer value */
					 *ptr = (void *) _p;
					 if (tp->cast) *ptr = (*(tp->cast))(*ptr);
					 return (char *) 0;
				  }
				  tp = tp->next;
				}
				sp++;
				/* Hmmm. Didn't find it this time */
			 }
		  }
		  /* Didn't find any sort of match for this data.
			  Get the pointer value and return the received type */
		  *ptr = (void *) _p;
		  return _c;
		} else {
		  /* Found a match on the first try.  Return pointer value */
		  *ptr = (void *) _p;
		  return (char *) 0;
		}
	 } else {
		/* No type specified.  Good luck */
		*ptr = (void *) _p;
		return (char *) 0;
	 }
  } else {
    if (strcmp (_c, "NULL") == 0) {
		*ptr = (void *) 0;
		return (char *) 0;
    }
    *ptr = (void *) 0;
    return _c;
  }
}
