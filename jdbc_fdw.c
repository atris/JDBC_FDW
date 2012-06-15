/*-------------------------------------------------------------------------
 *
 *		  foreign-data wrapper for JDBC
 *
 * Copyright (c) 2012, PostgreSQL Global Development Group
 *
 * This software is released under the PostgreSQL Licence
 *
 * Author: Atri Sharma <atri.jiit@gmail.com>
 *
 * IDENTIFICATION
 *		  jdbc_fdw/jdbc_fdw.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "funcapi.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "mb/pg_wchar.h"
#include "optimizer/cost.h"
#include "storage/fd.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "storage/ipc.h"
#include "jni.h"

PG_MODULE_MAGIC;

static JNIEnv *env;
static JavaVM *jvm;
static jobject java_call;   /* Used for calling methods of JDBCUtils Java class */


/*
 * Describes the valid options for objects that use this wrapper.
 */
struct jdbcFdwOption
{
	const char	*optname;
	Oid		optcontext;	/* Oid of catalog in which option may appear */
};

/*
 * Valid options for jdbc_fdw.
 *
 */
static struct jdbcFdwOption valid_options[] =
{

	/* Connection options */
	{ "drivername",		ForeignServerRelationId },
	{ "url",		ForeignServerRelationId },
	{ "username",		UserMappingRelationId },
	{ "password",		UserMappingRelationId },
	{ "query",		ForeignTableRelationId },
	{ "table",		ForeignTableRelationId },

	/* Sentinel */
	{ NULL,			InvalidOid }
};

/*
 * FDW-specific information for ForeignScanState.fdw_state.
 */

typedef struct jdbcFdwExecutionState
{
	char		*query;
	int		NumberOfRows;
	int 		RowCount;
	int 		NumberOfColumns;
} jdbcFdwExecutionState;

/*
 * SQL functions
 */
extern Datum jdbc_fdw_handler(PG_FUNCTION_ARGS);
extern Datum jdbc_fdw_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(jdbc_fdw_handler);
PG_FUNCTION_INFO_V1(jdbc_fdw_validator);

/*
 * FDW callback routines
 */
static FdwPlan *jdbcPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel);
static void jdbcExplainForeignScan(ForeignScanState *node, ExplainState *es);
static void jdbcBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *jdbcIterateForeignScan(ForeignScanState *node);
static void jdbcReScanForeignScan(ForeignScanState *node);
static void jdbcEndForeignScan(ForeignScanState *node);

/*
 * Helper functions
 */
static bool jdbcIsValidOption(const char *option, Oid context);
static void jdbcGetOptions(Oid foreigntableid, char **drivername, char **url, char **username, char **password, char **query, char **table);
/*
 * Uses a String object's content to create an instance of C String
 */
static char* ConvertStringToCString(jobject);
/*
 * JVM Initialization function
 */
static void JVMInitialization();
/*
 * JVM destroy function
 */
static void DestroyJVM();

/*
 * ConvertStringToCString
 *		Uses a String object passed as a jobject to the function to 
 *		create an instance of C String.
 */
static char*
ConvertStringToCString(jobject java_cstring)
{
	jclass JavaString;
	char *StringPointer;

	JavaString=(*env)->FindClass(env, "java/lang/String");
	if(!((*env)->IsInstanceOf(env, java_cstring, JavaString)))
	{
		elog(WARNING,"Object not an instance of String class");
		exit(0);
	}

	StringPointer=(char*)(*env)->GetStringUTFChars(env, (jstring)java_cstring, 0);

	return StringPointer;
}

/*
 * DestroyJVM
 *		Shuts down the JVM.
 */
static void 
DestroyJVM()
{
	(*jvm)->DestroyJavaVM(jvm);
}

/*
 * JVMInitialization
 *		Create the JVM which will be used for calling the Java routines
 *	        that use JDBC to connect and access the foreign database.
 *
 */
static void
JVMInitialization()
{
	jint res=-100;   /* Initializing the value of res so that we can check it later to see whether JVM has been correctly created or not */
	JavaVMInitArgs vm_args;
	JavaVMOption options[1];
		#ifndef JNI_VERSION_1_2
				JDK1_1InitArgs vm_args;
				char classpath[1024];
		#endif
	static bool FunctionCallCheck=false;   /* This flag safeguards against multiple calls of JVMInitialization().*/
	
	if(FunctionCallCheck==false)
	{
		#ifdef JNI_VERSION_1_2
				options[0].optionString =
				"-Djava.class.path=" "/home/gitc/postgresql-9.1.3/contrib/jdbc_fdw/JDBCClasses:/usr/local/pgsql/share/java/postgresql-9.1-902.jdbc4.jar:.";
				vm_args.version = 0x00010002;
				vm_args.options = options;
				vm_args.nOptions = 1;
				vm_args.ignoreUnrecognized = JNI_TRUE;

				/* Create the Java VM */
				res = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
		#else
				vm_args.version = 0x00010001;
				JNI_GetDefaultJavaVMInitArgs(&vm_args);
				sprintf(classpath, "%s%c%s",
				vm_args.classpath, PATH_SEPARATOR, USER_CLASSPATH);
				vm_args.classpath = classpath;

				/* Create the Java VM */
				res = JNI_CreateJavaVM(&jvm, &env, &vm_args);
		#endif
		if (res < 0) 
		{
			ereport(ERROR,
				 (errmsg("Failed to create Java VM")
				 ));
			exit(1);
		}

		/* Register an on_proc_exit handler that shuts down the JVM.*/
		on_proc_exit(DestroyJVM,0);
		FunctionCallCheck=true;
	}
}
	
/*
 * Foreign-data wrapper handler function: return a struct with pointers
 * to my callback routines.
 */
Datum
jdbc_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);

	fdwroutine->PlanForeignScan = jdbcPlanForeignScan;
	fdwroutine->ExplainForeignScan = jdbcExplainForeignScan;
	fdwroutine->BeginForeignScan = jdbcBeginForeignScan;
	fdwroutine->IterateForeignScan = jdbcIterateForeignScan;
	fdwroutine->ReScanForeignScan = jdbcReScanForeignScan;
	fdwroutine->EndForeignScan = jdbcEndForeignScan;

	PG_RETURN_POINTER(fdwroutine);
}

/*
 * Validate the generic options given to a FOREIGN DATA WRAPPER, SERVER,
 * USER MAPPING or FOREIGN TABLE that uses file_fdw.
 *
 * Raise an ERROR if the option or its value is considered invalid.
 */
Datum
jdbc_fdw_validator(PG_FUNCTION_ARGS)
{
	List		*options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid		catalog = PG_GETARG_OID(1);
	char		*svr_drivername = NULL;
	char 		*svr_url = NULL;
	char		*svr_username = NULL;
	char		*svr_password = NULL;
	char		*svr_query = NULL;
	char		*svr_table = NULL;
	ListCell	*cell;

	/*
	 * Check that only options supported by jdbc_fdw,
	 * and allowed for the current object type, are given.
	*/ 
	foreach(cell, options_list)
	{
		DefElem	   *def = (DefElem *) lfirst(cell);

		if (!jdbcIsValidOption(def->defname, catalog))
		{
			struct jdbcFdwOption *opt;
			StringInfoData buf;

			/*
			 * Unknown option specified, complain about it. Provide a hint
			 * with list of valid options for the object.
			 */
			initStringInfo(&buf);
			for (opt = valid_options; opt->optname; opt++)
			{
				if (catalog == opt->optcontext)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "",
							 opt->optname);
			}

			ereport(ERROR, 
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME), 
				errmsg("invalid option \"%s\"", def->defname), 
				errhint("Valid options in this context are: %s", buf.len ? buf.data : "<none>")
				));
		}

		if (strcmp(def->defname, "drivername") == 0)
		{
			if (svr_drivername)
				ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), 
					errmsg("conflicting or redundant options: drivername (%s)", defGetString(def))
					));

			svr_drivername = defGetString(def);
		}
		else if (strcmp(def->defname, "url") == 0)
		{
			if (svr_url)
				ereport(ERROR, 
					(errcode(ERRCODE_SYNTAX_ERROR), 
					errmsg("conflicting or redundant options: url (%s)", defGetString(def))
					));

			svr_url = defGetString(def);
		}
		if (strcmp(def->defname, "username") == 0)
		{
			if (svr_username)
				ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting or redundant options: username (%s)", defGetString(def))
					));

			svr_username = defGetString(def);
		}
		if (strcmp(def->defname, "password") == 0)
		{
			if (svr_password)
				ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting or redundant options: password")
					));

			svr_password = defGetString(def);
		}
		else if (strcmp(def->defname, "query") == 0)
		{
			if (svr_table)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting options: query cannot be used with table")
					));

			if (svr_query)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting or redundant options: query (%s)", defGetString(def))
					));

			svr_query = defGetString(def);
		}
		else if (strcmp(def->defname, "table") == 0)
		{
			if (svr_query)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting options: table cannot be used with query")
					));

			if (svr_table)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("conflicting or redundant options: table (%s)", defGetString(def))
					));

			svr_table = defGetString(def);
		}
	}

	PG_RETURN_VOID();
}


/*
 * Check if the provided option is one of the valid options.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
jdbcIsValidOption(const char *option, Oid context)
{
	struct jdbcFdwOption *opt;

	for (opt = valid_options; opt->optname; opt++)
	{
		if (context == opt->optcontext && strcmp(opt->optname, option) == 0)
			return true;
	}
	return false;
}

/*
 * Fetch the options for a jdbc_fdw foreign table.
 */
static void
jdbcGetOptions(Oid foreigntableid, char **drivername, char **url, char **username, char **password, char **query, char **table)
{
	ForeignTable	*f_table;
	ForeignServer	*f_server;
	UserMapping	*f_mapping;
	List		*options;
	ListCell	*lc;

	/*
	 * Extract options from FDW objects.
	 */
	f_table = GetForeignTable(foreigntableid);
	f_server = GetForeignServer(f_table->serverid);
	f_mapping = GetUserMapping(GetUserId(), f_table->serverid);

	options = NIL;
	options = list_concat(options, f_table->options);
	options = list_concat(options, f_server->options);
	options = list_concat(options, f_mapping->options);

	/* Loop through the options, and get the server/port */
	foreach(lc, options)
	{
		DefElem *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "drivername") == 0)
		{
			*drivername = defGetString(def);
		}

		if (strcmp(def->defname, "username") == 0)
		{
			*username = defGetString(def);
		}

		if (strcmp(def->defname, "password") == 0)
		{
			*password = defGetString(def);
		}

		if (strcmp(def->defname, "query") == 0)
		{
			*query = defGetString(def);
		}

		if (strcmp(def->defname, "table") == 0)
		{
			*table = defGetString(def);
		}

		if (strcmp(def->defname, "url") == 0)
		{
			*url = defGetString(def);
		}
	}

	/* Check we have the options we need to proceed */
	if (!*table && !*query)
			ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("either a table or a query must be specified")
			));
	if(!*drivername)
			ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
			errmsg("Driver name must be specified")
			));
	if(!*url)
		ereport(ERROR,
		(errcode(ERRCODE_SYNTAX_ERROR),
		errmsg("URL must be specified")
		));
}

/*
 * jdbcPlanForeignScan
 *		Create a FdwPlan for a scan on the foreign table
 */
static FdwPlan*
jdbcPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel)
{
	FdwPlan *fdwplan=NULL;
	
	char		*svr_drivername = NULL;
	char		*svr_username = NULL;
	char		*svr_password = NULL;
	char 		*svr_query = NULL;
	char 		*svr_table = NULL;
	char 		*svr_url = NULL;
	char		*query;

	fdwplan = makeNode(FdwPlan);
	JVMInitialization();

	/* Fetch options */
	jdbcGetOptions(foreigntableid, &svr_drivername, &svr_url, &svr_username, &svr_password, &svr_query, &svr_table);
	
	/* Build the query */
	if (svr_query)
	{
		size_t len = strlen(svr_query) + 9;

		query = (char *) palloc(len);
		snprintf(query, len, "EXPLAIN %s", svr_query);
	}
	else
	{
		size_t len = strlen(svr_table) + 23;

		query = (char *) palloc(len);
		snprintf(query, len, "EXPLAIN SELECT * FROM %s", svr_table);
	}

	return fdwplan;
}

/*
 * jdbcExplainForeignScan
 *		Produce extra output for EXPLAIN
 */
static void
jdbcExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
	char 		    *svr_drivername = NULL;
	char 		    *svr_url = NULL;
	char		    *svr_username = NULL;
	char		    *svr_password = NULL;
	char		    *svr_query = NULL;
	char		    *svr_table = NULL;

	/* Fetch options  */
	jdbcGetOptions(RelationGetRelid(node->ss.ss_currentRelation), &svr_drivername, &svr_url, &svr_username, &svr_password, &svr_query, &svr_table);
}

/*
 * jdbcBeginForeignScan
 *		Initiate access to the database
 */
static void
jdbcBeginForeignScan(ForeignScanState *node, int eflags)
{
	char 			*svr_drivername = NULL;
	char 			*svr_url = NULL;
	char			*svr_username = NULL;
	char			*svr_password = NULL;
	char			*svr_query = NULL;
	char			*svr_table = NULL;
	jdbcFdwExecutionState   *festate;
	char			*query;
	jclass 			JDBCUtilsClass;
	jclass		 	JavaString;
	jstring 		StringArray[6];
	jmethodID		id_initialize;
	jobjectArray		arg_array;
	int 			counter = 0;
	jfieldID 		id_numberofrows;
	jfieldID 		id_numberofcolumns;

	/* Fetch options  */
	jdbcGetOptions(RelationGetRelid(node->ss.ss_currentRelation), &svr_drivername, &svr_url, &svr_username, &svr_password, &svr_query, &svr_table);

	/* Build the query */
	if (svr_query){
		query = svr_query;
	}
	else
	{
		size_t len = strlen(svr_table) + 15;

		query = (char *)palloc(len);
		snprintf(query, len, "SELECT * FROM %s", svr_table);
	}

	/* Stash away the state info we have already */
	festate = (jdbcFdwExecutionState *) palloc(sizeof(jdbcFdwExecutionState));
	festate->query = query;
	festate->NumberOfRows=0;
	festate->RowCount=0;
	festate->NumberOfColumns=0;

	/* Connect to the server and execute the query */
	JDBCUtilsClass = (*env)->FindClass(env, "JDBCUtils");
	if (JDBCUtilsClass == NULL) 
	{
		elog(WARNING,"JDBCUtilsClass is NULL");
	}

	id_initialize = (*env)->GetMethodID(env, JDBCUtilsClass, "Initialize", "([Ljava/lang/String;)I");
	if (id_initialize == NULL) 
	{
		elog(WARNING,"id_initialize is NULL");
	}

	id_numberofrows = (*env)->GetFieldID(env, JDBCUtilsClass, "NumberOfRows" , "I");
	if(id_numberofrows == NULL)
	{
		elog(WARNING,"id_numberofrows is NULL");
	}

	id_numberofcolumns = (*env)->GetFieldID(env, JDBCUtilsClass, "NumberOfColumns" , "I");
	if(id_numberofcolumns == NULL)
	{
		elog(WARNING,"id_numberofcolumns is NULL");
	}

	StringArray[0] = (*env)->NewStringUTF(env, (festate->query));
	StringArray[1] = (*env)->NewStringUTF(env, svr_drivername);
	StringArray[2] = (*env)->NewStringUTF(env, svr_url);
	StringArray[3] = (*env)->NewStringUTF(env, svr_username);
	StringArray[4] = (*env)->NewStringUTF(env, svr_password);

	JavaString= (*env)->FindClass(env, "java/lang/String");

	arg_array = (*env)->NewObjectArray(env, 5, JavaString, StringArray[0]);
	if (arg_array == NULL) 
	{
		elog(WARNING,"arg_array is NULL");
	}

	for(counter=1;counter<5;counter++)
	{		
		if (StringArray[counter] == NULL) 
		{
			elog(WARNING, "StringArray[%d] is NULL",counter);
		}
		else
		{
			(*env)->SetObjectArrayElement(env,arg_array,counter,StringArray[counter]);
		}
	}
	

	java_call=(*env)->AllocObject(env,JDBCUtilsClass);
	if(java_call==NULL)
	{
		elog(WARNING,"java_call is NULL");
	}

	(*env)->CallObjectMethod(env,java_call,id_initialize,arg_array);
	node->fdw_state = (void *) festate;
	festate->NumberOfRows=(*env)->GetIntField(env, java_call, id_numberofrows);
	festate->NumberOfColumns=(*env)->GetIntField(env, java_call, id_numberofcolumns);
}

/*
 * jdbcIterateForeignScan
 *		Read next record from the data file and store it into the
 *		ScanTupleSlot as a virtual tuple
 */
static TupleTableSlot*
jdbcIterateForeignScan(ForeignScanState *node)
{
	char 			**values;
	HeapTuple		tuple;
	jmethodID		id_returnresultset;
	jclass 			JDBCUtilsClass;
	jobjectArray 		java_rowarray; 
	int 		        i=0;
	jdbcFdwExecutionState *festate = (jdbcFdwExecutionState *) node->fdw_state;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;

	/* Cleanup */
	ExecClearTuple(slot);


	JDBCUtilsClass = (*env)->FindClass(env, "JDBCUtils");
	if(JDBCUtilsClass == NULL) 
	{
		elog(WARNING,"JDBCUtilsClass is NULL");
	}

	id_returnresultset = (*env)->GetMethodID(env, JDBCUtilsClass, "ReturnResultSet", "()[Ljava/lang/String;");
	if (id_returnresultset == NULL) 
	{
		elog(WARNING,"id_returnresultset is NULL");
	}
 
	values=(char**)palloc(sizeof(char*)*(festate->NumberOfColumns));

	if(festate->RowCount < festate->NumberOfRows)
	{
		java_rowarray=(*env)->CallObjectMethod(env, java_call,id_returnresultset);

		for(i=0;i<(festate->NumberOfColumns);i++) 
		{
        		values[i]=ConvertStringToCString((jobject)(*env)->GetObjectArrayElement(env,java_rowarray, i));
    		}

		tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att), values);
		ExecStoreTuple(tuple, slot, InvalidBuffer, false);
		++(festate->RowCount);
	 }

return (slot);
}

/*
 * jdbcEndForeignScan
 *		Finish scanning foreign table and dispose objects used for this scan
 */
static void
jdbcEndForeignScan(ForeignScanState *node)
{
	jmethodID 			id_close;
	jclass 				JDBCUtilsClass;
	jdbcFdwExecutionState *festate = (jdbcFdwExecutionState *) node->fdw_state;

	JDBCUtilsClass = (*env)->FindClass(env, "JDBCUtils");
	if (JDBCUtilsClass == NULL) 
	{
		elog(WARNING,"JDBCUtilsClass is NULL");
	}

	id_close = (*env)->GetMethodID(env, JDBCUtilsClass, "Close", "()V");
	if (id_close == NULL) 
	{
		elog(WARNING,"id_close is NULL");
	}

	(*env)->CallObjectMethod(env,java_call,id_close);
	if (festate->query)
	{
		pfree(festate->query);
		festate->query = 0;
	}
}

/*
 * jdbcReScanForeignScan
 *		Rescan table, possibly with new parameters
 */
static void
jdbcReScanForeignScan(ForeignScanState *node)
{

}
