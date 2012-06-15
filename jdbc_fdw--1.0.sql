/*-------------------------------------------------------------------------
 *
 *                foreign-data wrapper for jdbc
 *
 * Copyright (c) 2012, PostgreSQL Global Development Group
 *
 * This software is released under the PostgreSQL Licence
 *
 * Author: Atri Sharma <atri.jiit@gmail.com>
 *
 * IDENTIFICATION
 *                jdbc_fdw/jdbc_fdw--1.0.sql
 *
 *-------------------------------------------------------------------------
 */

CREATE FUNCTION jdbc_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jdbc_fdw_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER jdbc_fdw
  HANDLER jdbc_fdw_handler
  VALIDATOR jdbc_fdw_validator;
