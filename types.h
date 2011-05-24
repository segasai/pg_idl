/*

  This file is part of pgsql_query, an IDL DLM interface to the postgreSQL
  database.

  Copyright (C) 2005  Erin Sheldon, NYU.  erin.sheldon at gmail.com

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


#if !defined (_types_h)
#define _types_h

typedef char                    int8;
typedef unsigned char           uint8;
typedef short int               int16;
typedef unsigned short int      uint16;
typedef int                     int32;
typedef unsigned int            uint32;
typedef float                   float32;
typedef double                  float64;
#ifdef _WIN32
typedef __int64                 int64;
typedef unsigned __int64        uint64;
#else
typedef long long               int64;
typedef unsigned long long      uint64;
#endif


#endif /* _types_h */
