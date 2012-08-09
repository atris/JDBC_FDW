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
 *		  jdbc_fdw/JDBCUtils.java
 *
 *-------------------------------------------------------------------------
 */

import java.sql.*;
import java.text.*;
import java.io.*;
import java.net.URL;
import java.net.URLClassLoader;
import java.net.MalformedURLException;
import java.util.*;
public class JDBCUtils
{
	private ResultSet 		result_set;
	private Connection 		conn;
	private int 			NumberOfColumns;
	private int 			NumberOfRows;
	private Statement 		sql;
	private String[] 		Iterate;
	private static JDBCDriverLoader JDBC_Driver_Loader;

/*
 * Initialize
 *		Initiates the connection to the foreign database after setting 
 *		up initial configuration and executes the query.
 */
	public String
	Initialize(String[] options_array) throws IOException
	{       
		DatabaseMetaData 	db_metadata;
		ResultSetMetaData 	result_set_metadata;
		Properties 		JDBCProperties;
		Class 			JDBCDriverClass = null;
		Driver 			JDBCDriver = null;
		String 			query = options_array[0];
		String 			DriverClassName = options_array[1];
		String 			url = options_array[2];
  		String 			userName = options_array[3];
  		String 			password = options_array[4];
		int 			querytimeoutvalue = Integer.parseInt(options_array[5]);
		File 			JarFile = new File(options_array[6]);
		String 			jarfile_path = JarFile.toURI().toURL().toString();
		StringWriter 		exception_stack_trace_string_writer = new StringWriter();
		PrintWriter 		exception_stack_trace_print_writer = new PrintWriter(exception_stack_trace_string_writer);

  		NumberOfColumns = 0;
  		conn = null;

  		try 
		{
			if (JDBC_Driver_Loader == null)
			{
				JDBC_Driver_Loader = new JDBCDriverLoader(new URL[]{JarFile.toURI().toURL()});
			}
			else if (JDBC_Driver_Loader.CheckIfClassIsLoaded(DriverClassName) == null)
			{
				JDBC_Driver_Loader.addPath("jar:file://"+jarfile_path+"!/");
			}	

			JDBCDriverClass = JDBC_Driver_Loader.loadClass(DriverClassName);

			JDBCDriver = (Driver)JDBCDriverClass.newInstance();
			JDBCProperties = new Properties();

			JDBCProperties.put("user", userName);
			JDBCProperties.put("password", password);

			conn = JDBCDriver.connect(url, JDBCProperties);
  		
  			db_metadata = conn.getMetaData();
  			System.out.println("Connection to "+db_metadata.getDatabaseProductName()+" "+db_metadata.getDatabaseProductVersion()
				+" successful.\n");

  			sql = conn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY);
			try
			{
				if (querytimeoutvalue != 0)
				{
					sql.setQueryTimeout(querytimeoutvalue);
				}

			}
			catch(Exception setquerytimeout_exception)
			{
		 		setquerytimeout_exception.printStackTrace(exception_stack_trace_print_writer);
				return (new String(exception_stack_trace_string_writer.toString()));
			}

  			result_set = sql.executeQuery(query);

  			result_set_metadata = result_set.getMetaData();
  			NumberOfColumns = result_set_metadata.getColumnCount();
  			Iterate = new String[NumberOfColumns];
		}
		catch (Exception initialize_exception)
	  	{
  	  		initialize_exception.printStackTrace(exception_stack_trace_print_writer);
			return (new String(exception_stack_trace_string_writer.toString()));
	  	}

		return null;
	}

/*
 * ReturnResultSet
 *		Returns the result set that is returned from the foreign database
 *		after execution of the query to C code.
 */
	public String[] 
	ReturnResultSet()
	{
		int 	i = 0;

		try
		{
			if (result_set.next())
			{
				for (i = 0; i < NumberOfColumns; i++)
				{
    					Iterate[i] = result_set.getString(i+1);
				}

				++NumberOfRows;				

				return (Iterate);
			}

		}
		catch (Exception returnresultset_exception)
	 	{
			returnresultset_exception.printStackTrace();
	 	}

		return null;
	}

/*
 * Close
 *		Releases the resources used.
 */
	public void 
	Close()
	{

		try
		{
			result_set.close();
			conn.close();
			result_set = null;
			conn = null;
			Iterate = null;
		}
		catch (Exception close_exception) 
	 	{
	 		close_exception.printStackTrace();
	 	}
	}

/*
 * Cancel
 *		Cancels the query and releases the resources in case query
 *		cancellation is requested by the user.
 */
	public void 
	Cancel()
	{

		try
		{
			result_set.close();
			conn.close();
		}
		catch(Exception cancel_exception)
	 	{
			cancel_exception.printStackTrace();
  	 	}
	}
}
