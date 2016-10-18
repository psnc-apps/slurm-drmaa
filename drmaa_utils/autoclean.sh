#!/bin/sh
# $Id: autoclean.sh 13 2011-04-20 15:41:43Z mmamonski $

echo "Removing all generated files. ($PWD)"

make maintainer-clean

rm -f aclocal.m4 configure configure.scan config.h.in confdefs.h libtool

rm -rf autom4te.cache
rm -rf scripts

find . -name Makefile.in -exec rm -f {} \;
find . -name Makefile -exec rm -f {} \;
find . -name \*~ -exec rm -f {} \;
find . -name \.#* -exec rm -f {} \;
find . -name \*.core -exec rm -f {} \;
find . -name \*.log -exec rm -f {} \;
find . -name \*.a -exec rm -f {} \;
find . -name \*.o -exec rm -f {} \;
find . -name \*.lo* -exec rm -f {} \;
find . -name \*.la* -exec rm -f {} \;

echo "done."
