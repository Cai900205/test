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

/*--------------------------------------------------------------------
 * This file holds an extended implementation for SWIG TCL Objects.
 * The idea is to provide a generic interface for introducing new object
 * types and providing a nicer object mangling for them.
 *
 */

/*
 * We keep track of the object types we are handling in the following
 * map that provides the mapping from SWIG obj type to obj prefix
 */
cl_map_t SWIG_AltMnglTypeToPrefix;

/*
 * All provided objects should be registered upon creation
 * using the interface: SWIG_RegNewObj(type, ptr)
 *
 * All objects should be de-registered on deletion
 * using the interface: SWIG_DeregObj(ptr)
 */

/*
 * We are going to simply assign an ID for each
 * legally assigned object.
 *
 * TWO Static Global Maps will hold pointer to obj num and
 * obj num to object mapping.
 * Object numbers will be allocated sequentialy
 */

static long int SWIG_AltMnglObjIdx = 0;
static cl_map_t SWIG_AltMnglTypeByPtr;
static cl_map_t SWIG_AltMnglIdxByPtr;
static cl_map_t SWIG_AltMnglPtrByIdx;

/* Given a ptr convert it into a search key */
static void ptr_to_key(void *ptr, uint64_t *key) {
  *key = 0;
# if __WORDSIZE == 64
  *key = (uint64_t)ptr;
#else
  *key |= (uint32_t)ptr;
#endif
}

/*--------------------------------------------------------------------
 * CL MAPS work off a unit64 key so we need to copy the given string
 * to a uint64_t to search for it
 */
void strToUInt64(const char *swig_type, uint64_t *res_p) {
  int i;
  swig_type = swig_type + 4; // assuming all objects types start with "_ib_"
  i = strlen(swig_type);
  if ( i > 8 ) i = 8;
  memset(res_p, 0, 8);
  memcpy(res_p, swig_type, i);
}

/* introduce a new valid object by registering it into the maps */
int SWIG_AltMnglRegObj(const char * type, void * ptr) {
  cl_map_t *p_typeByPtrMap = &(SWIG_AltMnglTypeByPtr);
  char *p_type;
  uint64_t key;

  /* convert the ptr into a key */
  ptr_to_key(ptr, &key);

  /* is such a pointer already registered */
  if ((p_type = (char *)cl_map_get(p_typeByPtrMap, key)))
  {
    printf("-W- Object of type:%s already exists for ptr:%p.\n",
           p_type, ptr);
    return 1;
  }

  // printf("-V- Registering type:%s ptr:%p.\n", type, ptr);
  /* advance our objects counter */
  SWIG_AltMnglObjIdx++;
  cl_map_insert(&(SWIG_AltMnglTypeByPtr), key, (void *)type);
  cl_map_insert(&(SWIG_AltMnglIdxByPtr), key, (void *)SWIG_AltMnglObjIdx);
  cl_map_insert(&(SWIG_AltMnglPtrByIdx),(uint64_t)SWIG_AltMnglObjIdx, ptr);

  return 0;
}

/* Remove an objects from the Maps */
int SWIG_AltMnglUnregObj( void * ptr) {

  uint64_t key;
  unsigned long int idx;

  /* convert the ptr into a key */
  ptr_to_key(ptr, &key);

  /* is such a pointer already registered for a type? */
  if (!cl_map_remove(&(SWIG_AltMnglTypeByPtr), key))
  {
    printf("-W- Fail to find object type for ptr:%p.\n", ptr);
  }

  /* but we must know it's idx ! */
  if (! (idx = (unsigned long int)cl_map_remove(&(SWIG_AltMnglIdxByPtr), key)))
  {
    printf("-W- Fail to find object idx for ptr:%p.\n", ptr);
    return 1;
  }

  /* remove from the map of ptr by idx */
  if (!cl_map_remove(&(SWIG_AltMnglPtrByIdx),(uint64_t)idx))
  {
    printf("-W- Fail to find object idx for ptr:%p.\n", ptr);
    return 1;
  }

  return 0;
}

/* given a pointer return it's object name */
int SWIG_AltMnglGetObjNameByPtr(Tcl_Obj *objPt, const char *p_expType, void * ptr) {
  char res[64];
  unsigned long int idx;
  char *p_type;
  uint64_t key;

  ptr_to_key(ptr, &key);

  /* first get the idx */
  idx = (unsigned long int)cl_map_get(&(SWIG_AltMnglIdxByPtr), key);
  if (!idx)
  {
    printf("-E- Fail to find object idx for ptr:%p.\n", ptr);
    return TCL_ERROR;
  }

  /* now get the type */
  p_type = (char *)cl_map_get(&(SWIG_AltMnglTypeByPtr), key);
  if (!p_type)
  {
    printf("-E- Fail to find object type for ptr:%p.\n", ptr);
    return TCL_ERROR;
  }

  /* check we got the expected type */
  if (strcmp(p_type, p_expType))
  {
    printf("-E- Expected type:%s but the objet has type:%s.\n",
           p_expType, p_type);
    return TCL_ERROR;
  }

  sprintf(res, "%s:%lu", p_type, idx);
  Tcl_SetStringObj(objPt, res, -1);
  return TCL_OK;
}

// given an object name return the object pointer:
int SWIG_AltMnglGetObjPtrByName(Tcl_Obj *objPtr, void **ptr) {
  char buf[256];
  char *colonIdx;
  char *idStr;
  unsigned long int idx;

  /* Format for the objects is always <type>:<idx> */
  strcpy(buf, Tcl_GetStringFromObj(objPtr,0));
  colonIdx = index(buf,':');
  idStr = colonIdx + 1;

  if (!colonIdx)
  {
    sprintf(ibis_tcl_error_msg,"-E- Bad formatted object:%s\n", buf);
    return TCL_ERROR;
  }

  *colonIdx = '\0';

  /* we can count on the object idx for the ptr */
  idx = strtoul( idStr, NULL, 10);
  *ptr = cl_map_get(&(SWIG_AltMnglPtrByIdx), (uint64_t)idx);
  if (!*ptr)
  {
    printf("-E- fail to find object by idx:%lu\n", idx);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/* Register a SWIG type to Object Prefix */
void SWIG_AltMnglRegTypeToPrefix (const char *swig_type, const char *objNamePrefix) {
  uint64_t type_key;

  strToUInt64(swig_type, &type_key);
  cl_map_insert(&SWIG_AltMnglTypeToPrefix, type_key, (void *)objNamePrefix);
}

/* initialize the alternate mangling code */
int SWIG_AltMnglInit(void) {
  // init the swig object maps
  cl_map_construct(&SWIG_AltMnglTypeToPrefix);
  cl_map_construct(&SWIG_AltMnglTypeByPtr);
  cl_map_construct(&SWIG_AltMnglIdxByPtr);
  cl_map_construct(&SWIG_AltMnglPtrByIdx);

  cl_map_init(&SWIG_AltMnglTypeToPrefix,10);
  cl_map_init(&SWIG_AltMnglTypeByPtr,20);
  cl_map_init(&SWIG_AltMnglIdxByPtr,20);
  cl_map_init(&SWIG_AltMnglPtrByIdx,20);
  return 0;
}

/*---------------------------------------------------------------------
 * The functions below are replicates for the standard SWIG
 * mapper functions. They will call the alternate object mangling
 * for the types available in SWIG_AltMnglTypeToPrefix map.
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
 * void SWIG_SetPointerObj(Tcl_Obj *objPtr, void *ptr, const char *type)
 *
 * Sets a Tcl object to a pointer value.
 *           ptr = void pointer value
 *           type = string representing type
 *
 * CONVERT THE OBJECT POINTER GIVEN INTO A TCL NAME
 *---------------------------------------------------------------------*/
SWIGSTATIC
void SWIG_SetPointerObj(Tcl_Obj *objPtr, void *_ptr, const char *type) {

  // if we have an alternate mangling use it:
  static char _hex[16] =
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
     'a', 'b', 'c', 'd', 'e', 'f'};
  unsigned long _p, _s;
  char _result[20], *_r;    /* Note : a 64-bit hex number = 16 digits */
  char _temp[20], *_c;
  char *p_type;
  uint64_t type_key;

  /* only if this type was pre-registered as SWIG Alt Mangling */
  strToUInt64(type, &type_key);
  //  printf("Looking for key:%s\n", type);
  p_type = (char *)cl_map_get(&SWIG_AltMnglTypeToPrefix, type_key);

  if (p_type != NULL)
  {
    if (SWIG_AltMnglGetObjNameByPtr(objPtr, p_type, _ptr))
    {
      printf("-E- Fail to convert object %p to %s obj.\n", _ptr, type);
    }
    return;
  }

  memset(_result, 0,20);
  _r = &_result[0];
  _p = (unsigned long) _ptr;
  if (_p > 0)
  {
    while (_p > 0) {
      _s = _p & 0xf;
      *_r = _hex[_s];
      _r++;
      _p = _p >> 4;
    }
    *_r = '_';
    _c = &_temp[0];
    while (_r >= _result)
      *(_c++) = *(_r--);
    *_c = 0;
    Tcl_SetStringObj(objPtr,_temp,-1);
  }
  else
  {
    Tcl_SetStringObj(objPtr,"NULL",-1);
  }
  if (_ptr)
    Tcl_AppendToObj(objPtr,type,-1);
}

/*---------------------------------------------------------------------
 * char *SWIG_GetPointerObj(Tcl_Interp *interp, Tcl_Obj *objPtr, void **ptr, const char *type)
 *
 * Attempts to extract a pointer value from our pointer type.
 * Upon failure, returns a string corresponding to the actual datatype.
 * Upon success, returns NULL and sets the pointer value in ptr.
 *
 * SET THE PTR GIVEN TO THE LOOKUP OF THE OBJECT BY NAME
 *---------------------------------------------------------------------*/

SWIGSTATIC
char *SWIG_GetPointerObj(Tcl_Interp *interp, Tcl_Obj *objPtr, void **ptr, const char *_t) {
  unsigned long _p;
  char temp_type[256];
  char *name;
  int  i, len;
  SwigPtrType *sp,*tp;
  SwigCacheType *cache;
  int  start, end;
  char *_c;
  uint64_t type_key;
  char *p_type;

  /* only we have this type registered for alt mangling */
  strToUInt64(_t, &type_key);
  p_type = (char *)cl_map_get(&SWIG_AltMnglTypeToPrefix, type_key);
  if (p_type != NULL)
  {
    /* get the object name by the given pointer */
    if (SWIG_AltMnglGetObjPtrByName(objPtr, ptr))
    {
      printf("-E- fail to get object by name %s\n",
             Tcl_GetStringFromObj(objPtr, &i));
      return Tcl_GetStringFromObj(objPtr, &i);
    }
    return 0;
  }

  _p = 0;

  /* Extract the pointer value as a string */
  _c = Tcl_GetStringFromObj(objPtr, &i);

  /* Pointer values must start with leading underscore */
  if (*_c == '_')
  {
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

    if (_t)
    {
      if (strcmp(_t,_c))
      {
        if (!SwigPtrSort)
        {
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
          if (cache->stat)
          {
            if (strcmp(_t,cache->name) == 0)
            {
              if (strcmp(_c,cache->mapped) == 0)
              {
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
        if (sp)
        {
          while (swigcmp(_t,sp) == 0) {
            name = sp->name;
            len = sp->len;
            tp = sp->next;
            /* Try to find entry for our given datatype */
            while(tp) {
              if (tp->len >= 255)
              {
                return _c;
              }
              strcpy(temp_type,tp->name);
              strncat(temp_type,_t+len,255-tp->len);
              if (strcmp(_c,temp_type) == 0)
              {

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
      }
      else
      {
        /* Found a match on the first try.  Return pointer value */
        *ptr = (void *) _p;
        return (char *) 0;
      }
    }
    else
    {
      /* No type specified.  Good luck */
      *ptr = (void *) _p;
      return (char *) 0;
    }
  }
  else
  {
    if (strcmp (_c, "NULL") == 0)
    {
      *ptr = (void *) 0;
      return (char *) 0;
    }
    *ptr = (void *) 0;
    return _c;
  }
}
