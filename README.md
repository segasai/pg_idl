pgsql_query IDL module
======================

This is a module for accessing PostgreSQL database from IDL. 
It was mostly written by Erin Sheldon. 
[Sergey Koposov](https://github.com/segasai/pg_idl) extracted it from [sdssidl](http://code.google.com/p/sdssidl/) and made the installation simpler.
I am making further improvements such as adding support for `NULL`s.

Postgres returns an empty C string for `NULL` values and this caused random values to appear in IDL query results.
Also, timestamps where cast to `IDL_STRING` which is incorrect when retrieving in binary format, they are really 64-bit longs (apparently) representing the number of microseconds since 01-01-00 00:00:00.

Installation
------------
The pre-requisite is that you have to download the postgresql [tarball](http://www.postgresql.org/ftp/source/) and put it in this directory (no need to extract it).
You also should have IDL in your path

After that just run
    make

This will create the directory called DLM which will have a dynamically loadable IDL module. 
You can put this directory anywhere and add the path to it to the environmental variable `IDL_DLM_PATH`, e.g.
    export IDL_DLM_PATH=some_path/DLM

The `make_dll` IDL facility didn't work out of the box for me (mac platform) so I added a `manual` target to the makefile.
In case `make` doesn't work, try make manual instead.

To enable further debug messages (other than those in `/verbose`), do:
    make extra_verbose

After that in any IDL session you should be able to use `pgsql_query()`

```ÌDL
IDL> res=pgsql_query('select ra,dec from sdssdr7.phototag limit 10',connect_info='host=some_host user=some_user password=some_password dbname=some_dbname')
% Loaded DLM: PGSQL_QUERY.
IDL> print, res
{       329.12152       44.397910}{       329.14333       44.390562}{329.14613       44.392761}{
       329.14461       44.392961}{       329.14460       44.392961}{ 329.14893       44.393050}{
       329.14863       44.393803}{       329.14229       44.395501}{ 329.14278       44.396347}{
       329.15523       44.395887}
IDL> help,res,/str
** Structure <2948e08>, 2 tags, length=16, data length=16, refs=1:
   RA              DOUBLE           329.12152
   DEC             DOUBLE           44.397910
```

IF you don't want to type all the time the connection info, you should 
define the environmental variables: `PGHOST`, `PGDATABASE`, `PGUSER` and `PGPASSWORD`:

```bash
export PGHOST=hostname_of_the_database_server
export PGDATABASE=dbname
export PGUSER=your_username
export PGPASSWORD=your_password
```

If you define these, you can easily query without additional arguments:

```ÌDL
Running the .idl_startup script...

IDL> res=pgsql_query('select ra,dec from sdssdr7.phototag limit 10')
```