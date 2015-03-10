#  This file is part of pgsql_query, an IDL DLM interface to the postgreSQL
#  database.  
#
#  Copyright (C) 2005  Erin Sheldon, NYU.  erin dot sheldon at gmail dot com
#            (C) 2011  Sergey Koposov, IoA, Cambridge. koposov at ast dot cam dot ac dot uk
#            (C) 2015  Henri Chain, Observatoire de Paris, henri dot chain at obspm dot fr
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


CP = cp -f
NAME = pgsql_query

SOFILES = $(NAME).so
OBJFILES = $(NAME).o $(NAME)_util.o
DLMS = $(NAME).dlm


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
	

PGDIR = $(PGPATH)
PG_CFLAGS = -I${PGDIR}/src/interfaces/libpq/ -I${PGDIR}/src/include/
LIBPQA = ${PGDIR}/src/interfaces/libpq/libpq.a

IDL_CFLAGS = -I$(IDL_DIR)/external/include -fPIC -D_REENTRANT -dynamic
LDFLAGS = -bundle -flat_namespace -undefined suppress
CC = gcc
LD = gcc

${LIBPQA}:
	ifndef PGVER
		$(error "Please download postgresql source tarball from http://www.postgresql.org/ftp/source/ and put it in this directory")
	endif
	tar $(TARARG) 
	cd $(PGPATH);./configure;make

all: link
	${CP} ${DLMS} DLM

%.o: %.c
	$(CC) $(PG_CFLAGS) $(IDL_CFLAGS) -c $< -o $@

manual: $(LIBPQA) $(OBJFILES)
	mkdir -p DLM
	$(LD) $(LDFLAGS) $(LIBPQA) $(OBJFILES) -o DLM/$(SOFILES)

extra_verbose: PG_CFLAGS += -DEXTRA_VERBOSE
extra_verbose: manual
	echo "compiling with extra verbosity."

clean:
	- ${RM} -r $(OBJFILES)

fclean: clean
	- ${RM} -r DLM

link: ${LIBPQA}
	mkdir -p DLM
	echo "make_dll,['$(NAME)','$(NAME)_util'],'$(NAME)','IDL_Load',input_directory='./',extra_cflags='"$(PG_CFLAGS)"',extra_lflags='"$(LIBPQA)"',compile_directory='./',/show_all_output,output_directory='DLM'" | idl
