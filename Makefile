#  This file is part of pgsql_query, an IDL DLM interface to the postgreSQL
# database.  
#
#  Copyright (C) 2005  Erin Sheldon, NYU.  erin dot sheldon at gmail dot com
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


IDL_DIR=/usr/local/itt/idl/


CC = gcc
LD = gcc
CP = cp -f

OS_CFLAGS=-Wall -fPIC
OS_LDFLAGS=-shared
OS_ARFLAGS=-rvcs
PG_CFLAGS=
PG_LDFLAGS=

INCLUDE = -I${IDL_DIR}/external/include
LDLIBS = -lz 

SOFILES = pgsql_query.so 
OBJFILES = pgsql_query.o pgsql_query_util.o 
DLMS = pgsql_query.dlm 


PGVER := $(wildcard postgr*tar*)

default: all

TGZ := $(filter %.tar.gz,$(PGVER))
TBZ := $(filter %.tar.bz2,$(PGVER))

ifndef TGZ
TARARG = xfj $(TBZ)
PGPATH := $(subst .tar.bz2,,$(TBZ))
else
TARARG = xfz $(TGZ)
PGPATH := $(subst .tar.gz,,$(TGZ))
endif
	

PGDIR=$(PGPATH)
PG_CFLAGS=-I${PGDIR}/src/interfaces/libpq/ -I${PGDIR}/src/include/
CFLAGS = ${INCLUDE} ${OS_CFLAGS} ${PG_CFLAGS}
LIBPQA=${PGDIR}/src/interfaces/libpq/libpq.a
LDFLAGS = ${OS_LDFLAGS} ${LIBPQA} 

${LIBPQA}:
ifndef PGVER
	$(error "Please download postgresql source tarball from http://www.postgresql.org/ftp/source/ and put it in this directory")
endif
	tar $(TARARG) 
	cd $(PGPATH);./configure;make


all: ${SOFILES}
	mkdir -p DLM
	${CP} ${SOFILES} DLM
	${CP} ${DLMS} DLM


clean:
	- ${RM} ${SOFILES} ${OBJFILES}
	- ${RM} DLM/${SOFILES} DLM/${DLMS}

pgsql_query.so: pgsql_query.o pgsql_query_util.o ${LIBPQA}
	@ echo linking $@
	${LD} -o $@ pgsql_query.o pgsql_query_util.o ${LDFLAGS} ${LDLIBS}

pgsql_query.o: pgsql_query.c pgsql_query.h ${LIBPQA}
pgsql_query_util.o: pgsql_query_util.c pgsql_query.h ${LIBPQA}
