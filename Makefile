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


CP = cp -f

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
LIBPQA=${PGDIR}/src/interfaces/libpq/libpq.a

${LIBPQA}:
ifndef PGVER
	$(error "Please download postgresql source tarball from http://www.postgresql.org/ftp/source/ and put it in this directory")
endif
	tar $(TARARG) 
	cd $(PGPATH);./configure;make


all: link
	${CP} ${DLMS} DLM


clean:
	- ${RM} -r DLM

link: ${LIBPQA}
	mkdir -p DLM
	echo "make_dll,['pgsql_query','pgsql_query_util'],'pgsql_query','IDL_Load',input_directory='./',extra_cflags='"$(PG_CFLAGS)"',extra_lflags='"$(LIBPQA)"',compile_directory='./',/show_all_output,output_directory='DLM'" | idl
