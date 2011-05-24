/*------------------------------------------------------------------------

  IDL interface to the postgres C api.  Called from IDL as a function.
  Returns a structure containing the results, or -1 if there are no 
  results.

  struct = pgsql_query(query, connect_info=, file=, /append, 
                       nrows=,
                       field_names=, field_types=, field_lengths=, 
		       /nointerrupt, /verbose,
		       status=)

  Output:
    An IDL structure if there are results, an IDL scalar -1 otherwise.
    A file may be output if file= is entered.

  Inputs:
    query: An idl string containing the query.
  
  Optional Inputs:
    connect_info=:  postgres connect info.  If not sent, uses the default.
    file=: Filename to write output.  Currently only ascii is supported.
    /append: Append the file.
    /nointerrupt: Do not query the database asynchronously. Normally, a busy
       loop runs, waiting for the results or an interrupt signal.  This uses
       a lot of CPU.  Use /nointerrupt when there is a long query with little 
       data to be returned and you are SURE it will finish in a reasonable
       amount of time. You will not be able to cancel the query without help 
       from the administrator of the database.
    /verbose: Print informational messages.

  Optional outputs: 
    field_names=: Names of fields.
    field_types=: Types of fields.
    field_lengths=: Number of elements for each field.
    status=: Status of result.

       MYPG_SUCCESS 0
       MYPG_CONNECT_FAILURE 1
       MYPG_NO_RESULT 2
       MYPG_WRITE_FAILURE 3
       MYPG_FATAL_ERROR 4

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


 --------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "idl_export.h"
#include "libpq-fe.h"
#include "types.h"
//#include <byteswap.h>

#include "pgsql_query.h"

IDL_VPTR pgsql_query(int argc, IDL_VPTR *argv, char *argk)
{

    IDL_VPTR queryVptr;
    char *query;
    char *connect_info;

    IDL_VPTR resultVptr;

    int async = 1;

    int status;


    (void) IDL_KWProcessByOffset(argc, argv, argk, kw_pars, 
            (IDL_VPTR *) 0, 1, &kw);


    if (kw.nrows_there)
        IDL_StoreScalarZero(kw.nrows, IDL_TYP_ULONG64);

    /* Check number of input parameters */
    if (pgsql_query_nparams(argc) < 1)
    {
        IDL_Message(IDL_M_NAMED_GENERIC, IDL_MSG_INFO, 
                "-Syntax: result=pgsql_query(query, nrows=, connect_info=, file=, /append, /nointerrupt, /verbose, status=)\nIf there are results for the query, a structure is returned.  Otherwise the result is -1");
        return(IDL_GettmpInt(-1));
    }

    /* Get the input query */
    queryVptr = argv[0];
    query = IDL_VarGetString(queryVptr);



    /* The user may have input connection information */
    if (kw.connect_info_there)
        connect_info=IDL_STRING_STR(&kw.connect_info);
    else
        connect_info="";

    /* Should we query asynchronously?  This would allow cancel through ^C */
    if (kw.nointerrupt_there)
        if (kw.nointerrupt) 
            async = 0;

    if (async) 
        status = pgsql_query_send_async(query, connect_info, &resultVptr);
    else 
        status = pgsql_query_send(query, connect_info, &resultVptr);

    setStatus(status);

    IDL_KW_FREE;
    if (status == MYPG_SUCCESS)
        return(resultVptr);
    else
        return(IDL_GettmpInt(-1));
}




/*-----------------------------------------------------------------------
  The IDL interface code
  -----------------------------------------------------------------------*/

#define ARRLEN(arr) (sizeof(arr)/sizeof(arr[0]))

int IDL_Load(void)
{

  /* This must be static. It is a struct. */
  /* The name in strings is the name by which it will be called from IDL and
     MUST BE CAPITALIZED 
     5th parameter will say if it accepts keywords and some other flags 
     For more info see page 325 of external dev. guide */
  static IDL_SYSFUN_DEF2 function_addr[] = {
    { (IDL_SYSRTN_GENERIC) pgsql_query, 
        "PGSQL_QUERY", 
        0, 
        IDL_MAXPARAMS, 
        IDL_SYSFUN_DEF_F_KEYWORDS, 
        0},
  };

  /* False means it is not a function */
  return IDL_SysRtnAdd(function_addr, IDL_TRUE, ARRLEN(function_addr));

}
