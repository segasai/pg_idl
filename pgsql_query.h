/*

  This file is part of pgsql_query, an IDL DLM interface to the postgreSQL
  database.

  Copyright (C) 2005  Erin Sheldon, NYU.  erin dot sheldon at gmail dot com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "idl_export.h"
#include "libpq-fe.h"
#include "types.h"
//#include <byteswap.h>

#if !defined (_pgsql_query_h)
#define _pgsql_query_h

#define LEFT_CURLY 123
#define RIGHT_CURLY 125
#define COMMA 44

/* Exit status values */
#define MYPG_SUCCESS 0
#define MYPG_CONNECT_FAILURE 1
#define MYPG_NO_RESULT 2
#define MYPG_WRITE_FAILURE 3
#define MYPG_FATAL_ERROR 4
#define MYPG_DISPATCH_ERROR 5
#define MYPG_QUERY_CANCELLED 6
#define MYPG_CANCEL_FAILURE 7

#define SIZE16 2
#define SIZE32 4
#define SIZE64 8

#define VERBOSE 0

typedef struct {

  int32 nFields;
  int32 nTuples;

  IDL_MEMINT *field_lengths;
  int16 *field_isarray;
  uint32 *field_types;
  char **field_names;

  IDL_STRUCT_TAG_DEF *tagdefs;

} field_info;

typedef struct {

  int32 ntags;

  IDL_MEMINT *tagOffsets;
  IDL_VPTR *tagDesc;
  int32 *tagNelts;

  IDL_StructDefPtr sdef;

} idl_tag_info;



/* Macros to convert to host byte-ordering */

#define NTOH16(p16, newp16) \
  *( ((unsigned short *)newp16)   ) = ntohs( *((unsigned short *)p16) );

#define NTOH32(p32, newp32) \
  *( ((unsigned int *)newp32)   ) = ntohl( *((unsigned int *)p32) );

#define NTOH64(p64, newp64) \
  *( ((unsigned int *)newp64)+1 ) = ntohl( *( ((unsigned int *)p64)   ) ); \
  *( ((unsigned int *)newp64)   ) = ntohl( *( ((unsigned int *)p64)+1 ) );

/* prepare and send the sql query */
int
pgsql_query_send(char *query, char *connect_info, IDL_VPTR *resultVptr);
int
pgsql_query_send_async(char *query, char *connect_info, IDL_VPTR *resultVptr);
void
pgsql_sigint_register();
void
pgsql_sigint_unregister();


/* Get postgres field info, store in structure and return */
field_info *
pgsql_get_field_info(PGresult *res);

/* Info for IDL structure creation */		      
idl_tag_info *
pgsql_get_idl_tag_info(IDL_STRUCT_TAG_DEF *tagdefs);

/* Store the binary data in an idl tag variable */
void pgsql_store_binary(int idlType, 
			int16 isarray, 
			char *input, 
			UCHAR *output);

/* Determine if field is an array */
int isArray(unsigned int field_type);

/* Copy array data */
//void fillNumArray(char *mem, char *array, int32 nbytes);
void fillNumArray(char *mem, UCHAR *array, int32 nbytes);
void fillStringArray(char *mem, IDL_STRING *array);

/* Get number of elements in postgres array */
int getNoEle(char* m);
IDL_MEMINT* GetArrayDims(char* mptr, IDL_MEMINT* totlen);


/* convert postgres type to idl type */
IDL_MEMINT pgsql2idltype(int pgsql_type);

/* copy field info into keywords */
void pgsql_copy_info(field_info *fi);

char *string2upper(char *st);







/* obsolete */
void fillFloat32Array(char *mem, float32 *array);
void fill32array(char *mem, char *array);
void fillInt16Array(char *mem, int16 *array);




/* Write results to an ascii file.  Requires non-binary data */
int
pgsql_write_file(PGresult *res);

/* Check the status of the get result statements */
int
pgsql_query_checkstatus(PGresult *res);

/* Write errore messages */
void pgsql_query_error(char *text, char *errMessage);

/* Determine number of parameters */
int pgsql_query_nparams(int argc);

/* prepare for exit */
void prepExit(PGconn *conn, PGresult *res);

/* Set the status */
void setStatus(int statusVal);

/* free memory in fi and ti structs */
void pgsql_freemem(field_info *fi, idl_tag_info *ti);



/* For ascii function */
int countArrayElements(char *value);

int elementLen(char *str);
char *extractVal(char *value);
char **extractElements(char *value, int nel);

void pgsql_store_ascii(int idlType, UCHAR *tptr, char *value);



typedef struct {
  IDL_KW_RESULT_FIRST_FIELD; /* Must be first entry in structure */

  /* These must be in alpabetical order */

  int append;
  int append_there;


  IDL_STRING connect_info;
  int connect_info_there;

  IDL_VPTR field_lengths;
  int field_lengths_there;

  IDL_VPTR field_names;
  int field_names_there;

  IDL_VPTR field_types;
  int field_types_there;

  IDL_STRING file;
  int file_there;

  int nointerrupt;
  int nointerrupt_there;

  /* Number of rows returned */
  IDL_VPTR nrows;
  int nrows_there;

  /* Status of the call */
  IDL_VPTR status;
  int status_there;

  int verbose;
  int verbose_there;

} KW_RESULT;

static IDL_KW_PAR kw_pars[] = {

  IDL_KW_FAST_SCAN, 

  /* These must be in alphabetical order */

  /* 
     IDL_KW_VIN: can be just an input, like var=3
     IDL_KW_OUT: can be output variable, var=var
     IDL_KW_ZERO: If not there, variable is set to 0

     If the VIN is set, then the _there variables will be
     0 when an undefined name variable is sent: no good for
     returning variables!
  */

  {"APPEND", IDL_TYP_INT, 1, 0, 
   IDL_KW_OFFSETOF(append_there), IDL_KW_OFFSETOF(append) },


  {"CONNECT_INFO", IDL_TYP_STRING, 1, 0, 
   IDL_KW_OFFSETOF(connect_info_there), IDL_KW_OFFSETOF(connect_info) },

  {"FIELD_LENGTHS", IDL_TYP_UNDEF, 1, IDL_KW_OUT | IDL_KW_ZERO, 
   IDL_KW_OFFSETOF(field_lengths_there), IDL_KW_OFFSETOF(field_lengths) },

  {"FIELD_NAMES", IDL_TYP_UNDEF, 1, IDL_KW_OUT | IDL_KW_ZERO, 
   IDL_KW_OFFSETOF(field_names_there), IDL_KW_OFFSETOF(field_names) },

  {"FIELD_TYPES", IDL_TYP_UNDEF, 1, IDL_KW_OUT | IDL_KW_ZERO, 
   IDL_KW_OFFSETOF(field_types_there), IDL_KW_OFFSETOF(field_types) },

  {"FILE", IDL_TYP_STRING, 1, 0, 
   IDL_KW_OFFSETOF(file_there), IDL_KW_OFFSETOF(file) },


  {"NOINTERRUPT", IDL_TYP_INT, 1, 0, 
   IDL_KW_OFFSETOF(nointerrupt_there), IDL_KW_OFFSETOF(nointerrupt) },


  {"NROWS", IDL_TYP_UNDEF, 1, IDL_KW_OUT | IDL_KW_ZERO, 
   IDL_KW_OFFSETOF(nrows_there), IDL_KW_OFFSETOF(nrows) },


  {"STATUS", IDL_TYP_UNDEF, 1, IDL_KW_OUT | IDL_KW_ZERO, 
   IDL_KW_OFFSETOF(status_there), IDL_KW_OFFSETOF(status) },

  {"VERBOSE", IDL_TYP_INT, 1, 0, 
   IDL_KW_OFFSETOF(verbose_there), IDL_KW_OFFSETOF(verbose) },

  { NULL }
};



/* we now declare keywords as global variable */
KW_RESULT kw;

#endif /* pgsql_query_h */
