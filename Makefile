##########################################################################
#
#                foreign-data wrapper for JDBC
#
# Copyright (c) 2012, PostgreSQL Global Development Group
#
# This software is released under the PostgreSQL Licence
#
# Author:Atri Sharma <atri.jiit@gmail.com>
#
# IDENTIFICATION
#                 jdbc_fdw/Makefile
# 
##########################################################################

MODULE_big = jdbc_fdw
OBJS = jdbc_fdw.o

EXTENSION = jdbc_fdw
DATA = jdbc_fdw--1.0.sql

REGRESS = jdbc_fdw

JDBC_CONFIG = jdbc_config

SHLIB_LINK = -ljvm

UNAME = $(shell uname)

ifeq ($(UNAME), Darwin)
	SHLIB_LINK = -I/System/Library/Frameworks/JavaVM.framework/Headers -L/System/Library/Frameworks/JavaVM.framework/Libraries -ljvm -framework JavaVM
endif

TRGTS = JAVAFILES

JAVA_SOURCES = \
        JDBCUtils.java \
	JDBCDriverLoader.java \
 
PG_CPPFLAGS=-D'PKG_LIB_DIR=$(pkglibdir)'

JFLAGS = -d $(pkglibdir)

all:$(TRGTS)

JAVAFILES:
	javac $(JFLAGS) $(JAVA_SOURCES)
 
ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/jdbc_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif


