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
#include <arpa/inet.h>

//#include <byteswap.h>

#include <time.h>
#include <unistd.h>
#include <ctype.h> // For toupper

#include "pgsql_query.h"


/* ---------------------------------------------------------------------
   Send the query and get the results 
   ---------------------------------------------------------------------*/

int
pgsql_query_send(char *query, char *connect_info, IDL_VPTR *resultVptr)
{

    /* connection info */

    PGconn *conn=NULL;
    PGresult *res=NULL;

    int query_status;


    /* Infor about each field */
    field_info *fi;

    /* Structure definition info */
    idl_tag_info *ti;
    //UCHAR *dataPtr;
    char *dataPtr;


    /* temporary pointer to tag data */
    UCHAR *tptr;

    /* loop variables */
    long long row;
    int tag;

    /* binary or ascii? Only need ascii for file output */
    int binary;

    int verbose=0;

    /* Attempt to establish the connection */
    conn = PQconnectdb(connect_info);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        pgsql_query_error("Could not establish connection",
                PQerrorMessage(conn));	
        PQfinish(conn);
        return(MYPG_CONNECT_FAILURE);
    }


    /* send the query and return the results */
    if (kw.file_there) 
        binary = 0;
    else
        binary = 1;

    if (kw.verbose_there)
        if (kw.verbose) 
            verbose = 1;

    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                "Querying database non-asynchronously: you may not cancel. For requests you may cancel don't send the /no_async keyword");

    res = PQexecParams(conn,
            query,
            0,    /* number of parameters (none) */
            NULL, /* see doc for the parameter info */
            NULL,
            NULL,
            NULL,
            binary); /* 0 for text, 1 for binary (network order) */


    /* Success? */
    query_status = pgsql_query_checkstatus(res);
    if (query_status != MYPG_SUCCESS)
    {
        prepExit(conn, res);
        return(query_status);
    }

    /* See if the user input a file to write to */
    if (kw.file_there) 
    {
        int write_status;
        write_status = pgsql_write_file(res);
        prepExit(conn, res);
        return(write_status);
    }


    /* Get information for each returned field */
    fi = pgsql_get_field_info(res);

    /* Copy into output keywords, if they exist */
    pgsql_copy_info(fi);

    /* Get info to make struct and copy data */
    ti = pgsql_get_idl_tag_info(fi->tagdefs);


    /* Create the output structure */  
    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Creating output struct");  


    dataPtr = 
        IDL_MakeTempStructVector(ti->sdef, (IDL_MEMINT) fi->nTuples, 
                resultVptr, IDL_TRUE);

    /* Copy into output variable */
    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Copying data");  

    for (row=0; row< fi->nTuples; row++)
        for (tag = 0; tag < fi->nFields; tag++)
        {
            tptr = 
                ( (*resultVptr)->value.s.arr->data +
                  row*( (*resultVptr)->value.arr->elt_len) + ti->tagOffsets[tag]);
            pgsql_store_binary(ti->tagDesc[tag]->type, fi->field_isarray[tag],
                    PQgetvalue(res, row, tag), tptr);	      
        }

    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Cleaning up");  

    pgsql_freemem(fi, ti);

    PQclear(res);
    PQfinish(conn);

    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Done");  


    return(MYPG_SUCCESS);

}







/* global, for signal handling */
int cancel_query = 0;

void *sigint_func(int signo)
{
    cancel_query = 1;
}
int pgsql_query_send_async(char *query, char *connect_info, IDL_VPTR *resultVptr)
{

    /* connection info */
    int query_status;


    PGconn *conn=NULL;
    PGresult *res=NULL;

    PGcancel *cancel_obj; 

    /* Information about each field */
    field_info *fi;

    /* Structure definition info */
    idl_tag_info *ti;
    //UCHAR *dataPtr;
    char *dataPtr;


    /* temporary pointer to tag data */
    UCHAR *tptr;

    /* loop variables */
    long long row;
    int tag;

    /* binary or ascii? Only need ascii for file output */
    int binary;

    int estatus;

    int verbose=0;

    /* We must reset this each time */
    cancel_query = 0;

    /* Attempt to establish the connection */
    conn = PQconnectdb(connect_info);

    if (PQstatus(conn) != CONNECTION_OK)
    {
        pgsql_query_error("Could not establish connection",
                PQerrorMessage(conn));	
        PQfinish(conn);
        return(MYPG_CONNECT_FAILURE);
    }



    /* send the query and return the results */
    if (kw.file_there) 
        binary = 0;
    else
        binary = 1;

    if (kw.verbose_there)
        if (kw.verbose) 
            verbose = 1;

    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                "Querying database (^C to cancel)");


    if (! PQsendQueryParams(conn,
                query,
                0,
                NULL,
                NULL,
                NULL,
                NULL,
                binary) )
    {
        prepExit(conn, res);
        return(MYPG_DISPATCH_ERROR);
    }

    if (! (cancel_obj = PQgetCancel(conn)) )
    {
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                "Cancel object is NULL");
        return(MYPG_CANCEL_FAILURE);
    }

    /* Only allow SIGINT after this point, since it calls cancel */
    pgsql_sigint_register();


    /* note this is a busy loop. I tried sleeping, but it really slows
       down the job */
    PQconsumeInput(conn);        //Try to collect the results
    while (PQisBusy(conn))       // while not ready ...
    {

        if (cancel_query)
        {
            char errbuf[256];

            IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                    "Canceling query at user request");
            if (!PQcancel(cancel_obj, errbuf, 256) )
            {
                estatus = MYPG_CANCEL_FAILURE;
                IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                        errbuf);
            }
            else
                estatus = MYPG_QUERY_CANCELLED;

            pgsql_sigint_unregister();

            /* This will call PQfinish and PQclear clear the memory */
            prepExit(conn, res);
            return(estatus);

        }

        PQconsumeInput(conn); //...retry 
    }



    /* No signal handling beyond this point */
    pgsql_sigint_unregister();

    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Getting result");  

    res = PQgetResult(conn);

    /* Success? */
    query_status = pgsql_query_checkstatus(res);
    if (query_status != MYPG_SUCCESS)
    {
        prepExit(conn, res);
        return(query_status);
    }

    /* See if the user input a file to write to */
    if (kw.file_there) 
    {
        int write_status;
        write_status = pgsql_write_file(res);
        prepExit(conn, res);
        return(write_status);
    }


    /* Get information for each returned field */
    fi = pgsql_get_field_info(res);

    /* Copy into output keywords, if they exist */
    pgsql_copy_info(fi);

    /* Get info to make struct and copy data */
    ti = pgsql_get_idl_tag_info(fi->tagdefs);


    /* Create the output structure */  
    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Creating output struct");  

    dataPtr = 
        IDL_MakeTempStructVector(ti->sdef, (IDL_MEMINT) fi->nTuples, 
                resultVptr, IDL_TRUE);

    /* Copy into output variable */
    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Copying data");  

    for (row=0; row< fi->nTuples; row++)
        for (tag = 0; tag < fi->nFields; tag++)
        {
            tptr = 
                ( (*resultVptr)->value.s.arr->data +
                  row*( (*resultVptr)->value.arr->elt_len) + ti->tagOffsets[tag]);
            pgsql_store_binary(ti->tagDesc[tag]->type, fi->field_isarray[tag],
                    PQgetvalue(res, row, tag), tptr);	      
        }


    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Cleaning up");  

    pgsql_freemem(fi, ti);
    PQclear(res);
    PQfinish(conn);


    if (verbose)
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, "Done");  

    return(MYPG_SUCCESS);

}


void 
pgsql_sigint_register()
{
    if (!IDL_SignalRegister(2, (void *) sigint_func, IDL_MSG_LONGJMP))
    {
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                "Could not register signal handler");
    }
}

void
pgsql_sigint_unregister()
{
    if (!IDL_SignalUnregister(2, (void *) sigint_func, IDL_MSG_LONGJMP))
    {
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                "Could not Unregister signal handler");
    }

}







/*
 *
 * get the number of elements in a postgres array field
 *
 */

int getNoEle(char* m) 
{ 
    return ntohl(*(int*)(m + 3*sizeof(int)));
}

IDL_MEMINT* GetArrayDims(char* mptr, IDL_MEMINT* totlen)
{
    IDL_MEMINT ndim=0, i=0;
    IDL_MEMINT* dims=NULL;

    *totlen=0;
    ndim = (IDL_MEMINT) ntohl( *(int32 *) (mptr+0*sizeof(int32)) );
    if (VERBOSE) 
        printf("ndim = %ld\n", ndim);fflush(stdout);

    if (ndim > 0) 
    {
        int pos=3;
        dims = (IDL_MEMINT *) calloc(ndim+1, sizeof(IDL_MEMINT));
        dims[0] = ndim;
        for (i=0; i<ndim; i++)
        {
            dims[i+1] = 
                (IDL_MEMINT) ntohl( *(int32 *) (mptr+pos*sizeof(int32)) );
            *totlen += dims[i+1];
            if (VERBOSE) 
                printf("  dim[%ld] = %ld\n", i, dims[i+1]);fflush(stdout);
            pos=pos+2;
        }
    }

    if (VERBOSE) 
        printf("Total length = %ld\n", *totlen);fflush(stdout);

    return(dims);
}



/* 
 *
 * Get postgres field info, store in structure and return 
 *
 */

field_info * pgsql_get_field_info(PGresult *res)
{

    field_info *fi;
    int i;

    fi = (field_info *) calloc(1, sizeof(field_info) );

    /* How many fields and rows did we return? */
    fi->nFields = PQnfields(res);
    fi->nTuples = PQntuples(res);

    /* Keep stats for each field */
    fi->field_lengths = calloc(fi->nFields, sizeof(IDL_MEMINT));
    fi->field_isarray = calloc(fi->nFields, sizeof(int16));
    fi->field_types   = calloc(fi->nFields, sizeof(uint32));
    fi->field_names   = calloc(fi->nFields, sizeof(char *));
    /* Note we need the extra one nFields+1 that is all null */
    fi->tagdefs       = calloc(fi->nFields+1, sizeof(IDL_STRUCT_TAG_DEF));

    if (VERBOSE)
        printf("%-15s %-6s %-9s %-6s","name","type","nelements","length\n");

    for (i = 0; i < fi->nFields; i++)
    {

        fi->field_names[i] = string2upper( PQfname(res,i) );
        fi->field_types[i] = PQftype(res,i);

        fi->field_isarray[i] = isArray(fi->field_types[i]);

        if (fi->field_isarray[i])
        {
            fi->tagdefs[i].dims = 
                GetArrayDims( PQgetvalue(res, 0, i), &(fi->field_lengths[i]) );
        }
        else
        {
            fi->field_lengths[i] = 1;
            fi->tagdefs[i].dims = 0;
        }

        fi->tagdefs[i].name = fi->field_names[i];
        fi->tagdefs[i].type = (void *) pgsql2idltype(fi->field_types[i]);
        fi->tagdefs[i].flags = 0;

        if (VERBOSE) 
            printf("%-15s %-6d %-9ld %-6d\n", 
                    fi->field_names[i], 
                    fi->field_types[i], 
                    fi->field_lengths[i], 
                    PQgetlength(res, 0, i));

    }

    if (VERBOSE)
        fflush(stdout);

    return(fi);

}

/* 
 *
 * Info for IDL structure creation 
 *
 */

idl_tag_info *pgsql_get_idl_tag_info(IDL_STRUCT_TAG_DEF *tagdefs)
{
    idl_tag_info *ti;
    int32 i, tag;
    char *tagName;

    ti = (idl_tag_info *) calloc(1, sizeof(idl_tag_info));

    ti->sdef = (IDL_StructDefPtr) IDL_MakeStruct(0, tagdefs);

    /* get offsets and descriptions */
    ti->ntags = IDL_StructNumTags(ti->sdef);

    ti->tagOffsets = calloc(ti->ntags, sizeof(IDL_MEMINT));
    ti->tagDesc    = calloc(ti->ntags, sizeof(IDL_VPTR));
    ti->tagNelts   = calloc(ti->ntags, sizeof(int32));

    i=0;
    for (tag=0; tag< ti->ntags; tag++)
    {
        tagName = 
            IDL_StructTagNameByIndex(ti->sdef, tag, 
                    IDL_MSG_INFO, NULL);

        ti->tagOffsets[tag] = 
            IDL_StructTagInfoByIndex(ti->sdef, 
                    tag, 
                    IDL_MSG_LONGJMP,
                    &(ti->tagDesc[tag]) );

        if (VERBOSE)
            printf("    Tag %d = \"%s\" ", tag, tagName);

        /* Is this an array? */
        if ( (ti->tagDesc[tag]->flags & IDL_V_ARR) != 0)
        {
            ti->tagNelts[tag] = ti->tagDesc[tag]->value.arr->n_elts;
            if (VERBOSE)
                printf(" ARRAY[%d] ", ti->tagNelts[tag]);
        }
        else
        {
            ti->tagNelts[tag] = 1;
            if (VERBOSE)
                printf(" SCALAR ");
        }
        if (VERBOSE)
            printf("\n");

    }

    return(ti);

}




/* Extracting data */


/* array must already be allocated */
/*
   first 4 bytes    don't know what it is but it is always 1
   --second 4 bytes    "     "    "    "  " "    "  "   "    0
   --third 4 bytes    oid of the datatype in the array
   --4th 4 bytes      number of  elements in the array
   --5th 4 bytes      dimension of the array
   */

void fillNumArray(char *mem, UCHAR* array, int32 nbytes)
{

    //char *memptr, *arrptr;
    UCHAR* memptr;
    UCHAR* arrptr;
    int i, n_elements;

    memptr = (UCHAR *) mem;
    arrptr = (UCHAR *) array;

    /* skip some elements to get to data */
    memptr += 3*SIZE32;

    /* get number of elements */
    n_elements = ntohl( *(int32 *) memptr );
    memptr += 2*SIZE32;

    switch(nbytes)
    {
        case 1:
            for (i=0; i< n_elements; i++)
            {

                /* Need to skip an int, which is size of this array element */
                /* This is always same for float32 */
                memptr += SIZE32;

                *arrptr = *memptr;

                /* next element */
                memptr += nbytes;
                arrptr += nbytes; 
            }
            break;
        case 2:
            for(i=0; i<n_elements; i++)
            {

                /* Need to skip an int, which is size of this array element */
                /* This is always same for float32 */
                memptr += SIZE32;

                /* copy in converted to host */
                NTOH16(memptr, arrptr);

                /* next element */
                memptr += nbytes;
                arrptr += nbytes; 
            }
            break;
        case 4:
            for(i=0; i<n_elements; i++)
            {

                /* Need to skip an int, which is size of this array element */
                /* This is always same for float32 */
                memptr += SIZE32;

                /* copy in converted to host */
                NTOH32(memptr, arrptr);

                /* next element */
                memptr += nbytes;
                arrptr += nbytes; 
            }
            break;
        case 8:
            for(i=0; i<n_elements; i++)
            {

                /* Need to skip an int, which is size of this array element */
                /* This is always same for float32 */
                memptr += SIZE32;

                /* copy in converted to host */
                NTOH64(memptr, arrptr);

                /* next element */
                memptr += nbytes;
                arrptr += nbytes; 
            }
            break;
        default:
            printf("nbytes must be 2,4, or 8\n");
            fflush(stdout);
            break;
    }
}


void fillStringArray(char *mem, IDL_STRING *array)
{

    char *memptr;
    IDL_STRING *arrptr;
    int i, n_elements, elsize;

    memptr = mem;
    arrptr = array;

    /* skip some elements to get to data */
    memptr += 3*SIZE32;

    /* get number of elements */
    n_elements = ntohl( *(int32 *) memptr );
    memptr += 2*SIZE32;

    for(i=0; i<n_elements; i++){

        /* Size of this array element */

        elsize = ntohl( *(int32 *) memptr );
        memptr += SIZE32;

        IDL_StrStore(arrptr, memptr);

        /* next element */
        memptr += elsize;
        arrptr++;

    }
}



char *string2upper(char *st)
{
    char *s = st;
    while ( (*s = toupper(*s))) ++s;
    return st;
}

/*
 * 
 * Store the binary data into IDL variables 
 *
 */

void pgsql_store_binary(int32 idlType, 
        int16 isarray, 
        char *input, 
        UCHAR *output)
{

    switch(idlType)
    {
        case IDL_TYP_FLOAT: 
            /* atof actually returns double */
            if (isarray)
                fillNumArray(input, output, 4);
            else
                NTOH32(input, output);
            break;

        case IDL_TYP_DOUBLE:
            if (isarray)
                fillNumArray(input, output, 8);
            else
                NTOH64(input, output);
            break;

        case IDL_TYP_BYTE:
            if (isarray)
                fillNumArray(input, output, 1);
            else
                *(UCHAR *) output = *(UCHAR *) input;
            break;

        case IDL_TYP_INT:
            if (isarray)
                fillNumArray(input, output, 2);
            else
                NTOH16(input, output);
            break;
        case IDL_TYP_UINT:
            if (isarray)
                fillNumArray(input, output, 2);
            else
                NTOH16(input, output);
            break;

        case IDL_TYP_LONG:
            if (isarray)
                fillNumArray(input, output, 4);
            else
                NTOH32(input, output);
            break;
        case IDL_TYP_ULONG:
            if (isarray)
                fillNumArray(input, output, 4);
            else
                NTOH32(input, output);
            break;

        case IDL_TYP_LONG64:
            if (isarray)
                fillNumArray(input, output, 8);
            else
                NTOH64(input, output);
            break;
        case IDL_TYP_ULONG64:
            if (isarray)
                fillNumArray(input, output, 8);
            else
                NTOH64(input, output);
            break;


        case IDL_TYP_STRING:
            if (isarray)
                fillStringArray(input, (IDL_STRING *) output);
            else
                IDL_StrStore( (IDL_STRING *) output, input);
            break;


        default: 
            printf("Unsupported type %d found\n", idlType);
            fflush(stdout);
            break;
    }

}


/*
 * Convert postgres type code to idl type code
 */


IDL_MEMINT pgsql2idltype(int pgsql_type)
{
    switch(pgsql_type)
    {
        /* 
           Scalar types 
           */

        /* boolean */
        case 16: return(IDL_TYP_BYTE);
                 /* "binary" data */
        case 17: return(IDL_TYP_BYTE);
                 /* char data */
        case 18: return(IDL_TYP_STRING);
                 /* "name" */
        case 19: return(IDL_TYP_STRING);
                 /* bigint */
        case 20: return(IDL_TYP_LONG64);
                 /* smallint */
        case 21: return(IDL_TYP_INT);
                 /* int2vector ??*/
                 /* 22: int2vector?? */
                 /* integer, int4 */
        case 23: return(IDL_TYP_LONG);
                 /* text */
        case 25: return(IDL_TYP_STRING);
                 /* oid */
        case 26: return(IDL_TYP_ULONG);
                 /* tid */
        case 27: return(IDL_TYP_ULONG);
                 /* xid */
        case 28: return(IDL_TYP_ULONG);
                 /* cid */
        case 29: return(IDL_TYP_ULONG);

                 /* float */
        case 700: return(IDL_TYP_FLOAT);
                  /* double */
        case 701: return(IDL_TYP_DOUBLE);

                  /* bpchar */
        case 1042: return(IDL_TYP_STRING);
                   /* varchar */
        case 1043: return(IDL_TYP_STRING);
                   /* date */
        case 1082: return(IDL_TYP_STRING);
                   /* time */
        case 1083: return(IDL_TYP_STRING);
                   /* timestamp */
        case 1114: return(IDL_TYP_STRING);

                   /*
                      Array types 
                      */

                   /* bool */
        case 1000: return(IDL_TYP_BYTE);
                   /* binary */
        case 1001: return(IDL_TYP_BYTE);
                   /* char */
        case 1002: return(IDL_TYP_BYTE);
                   /* name */
        case 1003: return(IDL_TYP_BYTE);

                   /* smallint */
        case 1005: return(IDL_TYP_INT);
                   /* int2vector */
                   /* case 1006: ?? */
                   /* integer, int4 */
        case 1007: return(IDL_TYP_LONG);
                   /* bigint, int8 */
        case 1016: return(IDL_TYP_LONG64);

                   /* text */
        case 1009: return(IDL_TYP_STRING);

                   /* oid */
        case 1028: return(IDL_TYP_ULONG);
                   /* tid */
        case 1010: return(IDL_TYP_ULONG);
                   /* xid */
        case 1011: return(IDL_TYP_ULONG);
                   /* cid */
        case 1012: return(IDL_TYP_ULONG);

                   /* character(n), fixed length */
        case 1014: return(IDL_TYP_STRING);
                   /* varchar */
        case 1015: return(IDL_TYP_STRING);

                   /* float */
        case 1021: return(IDL_TYP_FLOAT);
                   /* double */
        case 1022: return(IDL_TYP_DOUBLE);

                   /* abstime */
        case 1023: return(IDL_TYP_STRING);
                   /* reltime */
        case 1024: return(IDL_TYP_STRING);
                   /* tinterval */
        case 1025: return(IDL_TYP_STRING);

                   /* timestamp */
        case 1115: return(IDL_TYP_STRING);
                   /* time */
        case 1183: return(IDL_TYP_STRING);
                   /* timestampz */
        case 1185: return(IDL_TYP_STRING);
                   /* timetz */
        case 1270: return(IDL_TYP_STRING);

                   /* numeric */
        case 1231: return(IDL_TYP_DOUBLE);

        default: return(IDL_TYP_STRING);
    }
}

/* 
 * Given the type whether this is an array or not 
 */

int isArray(unsigned int field_type)
{

    int isarray;

    /* to get type codes: select oid,typname from pg_type; */
    switch(field_type)
    {
        case 1000:
            /* boolean array */
            isarray=1;
            break;
        case 1001:
            /* binary data: "byte array" */
            isarray=1;
            break;
        case 1002:
            /* char array */
            isarray=1;
            break;
        case 1005:
            /* int2 array */
            isarray = 1;
            break;
        case 1007:
            /* int4 array */
            isarray = 1;
            break;
        case 1009:
            /* text array */
            isarray = 1;
            break;
        case 1015:
            /* varchar array */
            isarray = 1;
            break;
        case 1016:
            /* int8 array */
            isarray = 1;
            break;
        case 1021:
            /* Float (real) array */
            isarray = 1;
            break;
        case 1022:
            /* float8 array */
            isarray = 1;
            break;
        case 1561:
            /* bit array */
            isarray = 1;
            break;
        default :
            isarray = 0;
            break;
    }

    return(isarray);

}

/*
 * copy the field info into output keywords
 */

void pgsql_copy_info(field_info *fi) 

{

    int i;

    /* For storing field info */
    IDL_VPTR fieldNamesVptr;
    IDL_STRING *namesPtr;
    IDL_VPTR fieldTypesVptr;
    IDL_LONG *typesPtr;
    IDL_VPTR fieldLengthsVptr;
    IDL_LONG *lengthsPtr;

    if (kw.field_names_there) 
    {
        namesPtr = (IDL_STRING *) 
            IDL_MakeTempVector(IDL_TYP_STRING, fi->nFields, 
                    IDL_ARR_INI_ZERO, &fieldNamesVptr);
        for (i=0; i<fi->nFields; i++)
            IDL_StrStore(&namesPtr[i], fi->field_names[i]);

        IDL_VarCopy(fieldNamesVptr, kw.field_names);
    }

    if (kw.field_types_there)
    {
        typesPtr = (IDL_LONG *) 
            IDL_MakeTempVector(IDL_TYP_ULONG, fi->nFields, 
                    IDL_ARR_INI_ZERO, &fieldTypesVptr);

        for (i=0; i<fi->nFields; i++)
            typesPtr[i] = fi->field_types[i];

        IDL_VarCopy(fieldTypesVptr, kw.field_types);
    }

    if (kw.field_lengths_there)
    {
        lengthsPtr = (IDL_LONG *) 
            IDL_MakeTempVector(IDL_TYP_LONG, fi->nFields, 
                    IDL_ARR_INI_ZERO, &fieldLengthsVptr);

        for (i=0; i<fi->nFields; i++)
            lengthsPtr[i] = fi->field_lengths[i];

        IDL_VarCopy(fieldLengthsVptr, kw.field_lengths);
    }

    if (kw.nrows_there)
        kw.nrows->value.ul64 = fi->nTuples;

}





/*
 * write the results to a file
 */


int pgsql_write_file(PGresult *res)
{

    int nFields;
    long long nTuples;
    int field;
    long long row;

    char *file;
    FILE *OFILEPTR;

    file = IDL_STRING_STR(&kw.file);
    nFields = PQnfields(res);
    nTuples = PQntuples(res);

    /* Are we creating new or appending? */
    if ( (kw.append_there) && (kw.append != 0) )
        OFILEPTR = fopen(file, "a");
    else
        OFILEPTR = fopen(file, "w");

    if (!OFILEPTR)
    {
        pgsql_query_error("Could not open file",file);
        return(MYPG_WRITE_FAILURE);
    }

    /* Loop through and write everything to file, tab-separated */
    for (row=0; row<nTuples; row++)
        for (field=0;field<nFields;field++)
        {
            fprintf(OFILEPTR,"%s", PQgetvalue(res, row, field) );
            /* If last field print a new line, else a tab*/
            if (field == nFields-1)
                fprintf(OFILEPTR,"\n");
            else
                fprintf(OFILEPTR,"\t");
        }
    fclose(OFILEPTR);

    /* No matter what there are no results */
    return(MYPG_NO_RESULT);

}


/* Complex checking of the query status */
    int
pgsql_query_checkstatus(PGresult *res)
{

    int status;
    int nFields;
    long long nTuples;

    /* 
       For successful queries, there are two options: either the query returns 
       results or not.  If it does not return data, but successfully completed, 
       then the status will be set to 
       PGRES_COMMAND_OK.  
       If success and returns tuples, then it will be set to 
       PGRES_TUPLES_OK 
       */


    status = PQresultStatus(res);

    if (PQresultStatus(res) == PGRES_COMMAND_OK)
    {
        /* Success but no results */
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, PQcmdStatus(res));
        return(MYPG_NO_RESULT);
    }

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                PQresultErrorMessage(res));

        switch (status)
        {
            /* No results */
            case PGRES_COPY_IN:        return(MYPG_NO_RESULT);
            case PGRES_COPY_OUT:       return(MYPG_NO_RESULT);
            case PGRES_BAD_RESPONSE:   return(MYPG_NO_RESULT);
            case PGRES_NONFATAL_ERROR: return(MYPG_NO_RESULT);
                                       /* An error */
            case PGRES_FATAL_ERROR:    return(MYPG_FATAL_ERROR);
            default: break;
        } 
    }

    /* How many fields and rows did we return? */
    nFields = PQnfields(res);
    nTuples = PQntuples(res);

    /* Result is empty, either an error or this query returns no data */
    if (nTuples == 0) 
    {
        if (nFields == 0)
        {
            /* Query returns no data. Try to print a message to clarify */
            IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, PQcmdStatus(res));
            return(MYPG_NO_RESULT);
        }
        else
            /* Probably nothing matched the query */
            return(MYPG_NO_RESULT);

    }

    return(MYPG_SUCCESS);

}


/*-----------------------------------------------------------------------
  mysql_query_database_error
  Print error stataments using IDL_Message
  -----------------------------------------------------------------------*/

    void
pgsql_query_error(char *text, char *errMessage)
{

    char errmsg[256];

    if (errMessage) {
        sprintf(errmsg, "Error: %s: %s\n", text, errMessage);
    } else {
        sprintf(errmsg, "Error: %s\n", text);
    }
    IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, errmsg);

}


/*-----------------------------------------------------------------------
  nParams
  return the number of parameters.  Accounts for keywords
  -----------------------------------------------------------------------*/

int pgsql_query_nparams(int argc)
{

    int nKeywords;

    nKeywords =
        kw.append_there + 
        kw.field_lengths_there + 
        kw.field_names_there + kw.field_types_there + 
        kw.file_there + 
        kw.nointerrupt_there + 
        kw.nrows_there + 
        kw.status_there +
        kw.verbose_there;

    return 
        argc - nKeywords;
}

/* ----------------------------------------------------------------------
   Set the status and free the postgres connection and result
   ----------------------------------------------------------------------*/

    void
prepExit(PGconn *conn, PGresult *res)
{

    if (conn != NULL)
        PQfinish(conn);

    if (res != NULL)
        PQclear(res);

}

/*----------------------------------------------------------------------- 
  set the status keyword if it is there 
  -----------------------------------------------------------------------*/

    void
setStatus(int statusVal)
{

    if (kw.status_there) {
        /* This frees any existing memory and sets type to INT with value zero */
        IDL_StoreScalarZero(kw.status, IDL_TYP_INT);
        kw.status->value.i = statusVal;
    }

}



void pgsql_freemem(field_info *fi, idl_tag_info *ti)
{
    int i;
    /* 
       Free memory

       No need to free field_names since just pointers into 
       the result field names 
       */
    free(fi->field_lengths);
    free(fi->field_isarray);
    free(fi->field_types);
    
    // Might not need to do this
    free(fi->field_names);

    for (i=0; i<fi->nFields; i++)
        free(fi->tagdefs[i].dims);
    free(fi->tagdefs); 

    free(fi);

    free(ti->tagOffsets);
    free(ti->tagDesc);
    free(ti->tagNelts);

    free(ti);
}

























/* Functions I used with the old ascii retrieval */
void pgsql_store_ascii(int idlType, UCHAR *tptr, char *value)
{

    /* Note, not all types can be converted from atoi directly, 
       I am doing the conversion in those cases, hope it works
       */

    switch(idlType)
    {
        case IDL_TYP_FLOAT: 
            /* atof actually returns double */
            *(float *)tptr = (float) atof(value);
            break;
        case IDL_TYP_DOUBLE:
            *(double *)tptr = atof(value);
            break;
        case IDL_TYP_BYTE:
            *(UCHAR *)tptr = (UCHAR) atoi(value);
            break;
        case IDL_TYP_INT:
            *(short *)tptr = (short) atoi(value);
            break;
        case IDL_TYP_UINT:
            *(unsigned short *)tptr = (unsigned short ) atoi(value);
            break;
        case IDL_TYP_LONG:
            *(IDL_LONG *)tptr = (IDL_LONG) atoi(value);
            break;
        case IDL_TYP_ULONG:
            *(IDL_ULONG *)tptr = (IDL_ULONG) atoi(value);
            break;
        case IDL_TYP_LONG64:
            *(IDL_LONG64 *)tptr = (IDL_LONG64) atoll(value);
            break;
        case IDL_TYP_ULONG64:
            *(IDL_ULONG64 *)tptr = (IDL_ULONG64) atoll(value);
            break;
        case IDL_TYP_STRING:
            IDL_StrStore( (IDL_STRING *) tptr, value);
            break;
        default: 
            printf("Unsupported type %d found\n", idlType);
            fflush(stdout);
            break;
    }

}


/* count the number of elements in an array literal */

int countArrayElements(char *value)
{

    int num, valLen, i;
    char t;  

    valLen = strlen(value);

    num = 0;
    for (i=0;i<valLen;i++)
    {
        t = value[i];

        if (t == LEFT_CURLY) 
            num += 1;
        else if (t == COMMA)
            num += 1;

    }

    return(num);
}

/* Extract a copy of the value. For IDL we will just do a store */

char *extractVal(char *value)
{

    int i;
    short len;
    char *tmp;

    len = strlen(value);
    tmp = calloc(len+1, sizeof(char));
    for (i=0;i<len+1;i++)
        tmp[i] = value[i];

    return(tmp);
}

/* Get the length of the next element */
int elementLen(char *str)
{

    char *tmp;
    int len;

    /* loop over and find end of this element in order to count size */
    tmp = str;
    len = 0;
    while (tmp != NULL && tmp[0] != COMMA && tmp[0] != RIGHT_CURLY)
    {
        len++;
        tmp++;
    }

    return(len);

}

/* Extract all the elements */

char **extractElements(char *value, int nel)
{

    char **elements;
    char *end, *el;
    int len;
    int i,j;

    elements = calloc(nel, sizeof(char *));

    /* find the beginning of the array */
    end = strtok(value, "{");

    for (i=0;i<nel; i++)
    {
        /* length of first element in this array */
        len = elementLen(end);

        /* Copy from value into an element string */
        el = calloc(len+1, sizeof(char));
        for (j=0;j<len;j++)
        {
            el[j] = end[0];
            end++;
        }
        el[j] = '\0';

        /* Make elements point to this string */
        elements[i] = el;

        if (end == NULL || end[0] == RIGHT_CURLY)
        { 
            if (i != nel-1)
            {
                IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO,
                        "Encountered end before extracting all elements");
                return(elements);
            }
        }

        /* skip the comma */
        end++;
    }

    return(elements);
}





















/* obsolete functions */
void fillFloat32Array(char *mem, float32 *array)
{

    char *memptr;
    int i, n_elements;

    memptr = mem;

    /* skip some elements to get to data */
    memptr += 3*SIZE32;

    /* get number of elements */
    n_elements = ntohl( *(int32 *) memptr );
    memptr += 2*SIZE32;

    for(i=0; i<n_elements; i++){

        /* Need to skip an int, which is size of this array element */
        /* This is always same for float32 */
        memptr += SIZE32;

        /* copy in converted to host */
        NTOH32(memptr, &array[i]);

        /* next element */
        memptr += SIZE32;

    }
}


void fill32array(char *mem, char *array)
{

    char *memptr, *arrptr;
    int i, n_elements;

    memptr = mem;
    arrptr = array;

    /* skip some elements to get to data */
    memptr += 3*SIZE32;

    /* get number of elements */
    n_elements = ntohl( *(int32 *) memptr );
    memptr += 2*SIZE32;

    for(i=0; i<n_elements; i++){

        /* Need to skip an int, which is size of this array element */
        /* This is always same for float32 */
        memptr += SIZE32;

        /* copy in converted to host */
        NTOH32(memptr, arrptr);

        /* next element */
        memptr += SIZE32;
        arrptr += SIZE32;

    }
}

void fillInt16Array(char *mem, int16 *array)
{

    char *memptr;
    int i, n_elements;

    memptr = mem;

    /* skip some elements to get to data */
    memptr += 3*SIZE32;

    /* get number of elements */
    n_elements = ntohl( *(int32 *) memptr );
    memptr += 2*SIZE32;

    for(i=0; i<n_elements; i++){

        /* Need to skip an int, which is size of this array element */
        /* This is always same for float32 */
        memptr += SIZE32;

        /* copy in converted to host */
        NTOH16(memptr, &array[i]);

        /* next element */
        memptr += SIZE16;

    }
}





