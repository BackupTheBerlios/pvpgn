@echo off
echo Parameter -DWITH_SQL_MYSQL enables MySQL SQL storage support
echo Parameter -DWITH_CDB enables CDB file storage support
echo Parameter -DWITH_GUI to build GUI versions of bnetd, d2cs and d2dbs
echo 
echo Parameter clean deletes object files
echo

make -fMakefile.BORLAND %1 %2