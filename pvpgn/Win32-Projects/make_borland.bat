@echo off
echo ===================================================================
echo Parameter -DWITH_SQL_MYSQL enables MySQL SQL storage support
echo Parameter -DWITH_CDB enables CDB file storage support
echo Parameter -DWITH_GUI to build GUI versions of bnetd, d2cs and d2dbs
echo ===================================================================
echo Parameter clean deletes object files
echo ===================================================================
copy Makefile.BORLAND ..\src\Makefile.BORLAND
cd ..\src
make -fMakefile.BORLAND %1 %2
cd ..\Win32-Projects