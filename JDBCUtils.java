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
public class JDBCUtils{
	private ResultSet result_set;
	private Connection conn;
	private int NumberOfColumns;
	private int NumberOfRows;
	private Statement sql;
	private String[] Iterate;
	private static JDBCDriverLoader JDBC_Driver_Loader;

	public String
	Initialize(String[] options_array) throws IOException
	{       
		DatabaseMetaData dbmd;
		ResultSetMetaData result_set_metadata;
		Properties JDBCProperties;
		Class JDBCDriverClass = null;
		File JarFile = new File(options_array[6]);
		Driver JDBCDriver = null;
		String DriverClassName = options_array[1];
		String url = options_array[2];
  		String userName = options_array[3];
  		String password = options_array[4];
		String query = options_array[0];
		String jarfile_path = options_array[6];
		int querytimeoutvalue = Integer.parseInt(options_array[5]);
		StringWriter exception_stack_trace_string_writer = new StringWriter();
		PrintWriter exception_stack_trace_print_writer = new PrintWriter(exception_stack_trace_string_writer);

  		NumberOfColumns = 0;
  		conn = null;

  		try 
		{
			if(JDBC_Driver_Loader == null)
			{
				JDBC_Driver_Loader = new JDBCDriverLoader(new URL[]{JarFile.toURI().toURL()});
			}
			else if(JDBC_Driver_Loader.CheckIfClassIsLoaded(DriverClassName) == null){
				JDBC_Driver_Loader.addPath("jar:file://"+jarfile_path+"!/");
			}	

			JDBCDriverClass = JDBC_Driver_Loader.loadClass(DriverClassName);

			JDBCDriver = (Driver)JDBCDriverClass.newInstance();
			JDBCProperties = new Properties();

			JDBCProperties.put("user", userName);
			JDBCProperties.put("password", password);

			conn = JDBCDriver.connect(url, JDBCProperties);
  		
  			dbmd = conn.getMetaData();
  			System.out.println("Connection to "+dbmd.getDatabaseProductName()+" "+dbmd.getDatabaseProductVersion()+" successful.\n");

  			sql = conn.createStatement(ResultSet.TYPE_FORWARD_ONLY, ResultSet.CONCUR_READ_ONLY);
			try
			{
				if(querytimeoutvalue!=0)
				{
					sql.setQueryTimeout(querytimeoutvalue);
				}

			}
			catch(Exception setquerytimeout_exception)
			{
		 		setquerytimeout_exception.printStackTrace(exception_stack_trace_print_writer);
				return new String(exception_stack_trace_string_writer.toString());
			}

  			result_set = sql.executeQuery(query);

  			result_set_metadata = result_set.getMetaData();
  			NumberOfColumns = result_set_metadata.getColumnCount();
  			Iterate = new String[NumberOfColumns];
		}
		catch (Exception initialize_exception)
	  	{
  	  		initialize_exception.printStackTrace(exception_stack_trace_print_writer);
			return new String(exception_stack_trace_string_writer.toString());
	  	}

		return null;
	}

	public String[] 
	ReturnResultSet()
	{
		int i = 0;

		try
		{
			if(result_set.next())
			{
				for(i=0;i<NumberOfColumns;i++)
				{
    					Iterate[i] = result_set.getString(i+1);
				}

				++NumberOfRows;				

				return Iterate;
			}

		}
		catch (Exception returnresultset_exception)
	 	{
			returnresultset_exception.printStackTrace();
	 	}

		return null;
	}

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
