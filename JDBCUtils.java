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
	private ResultSet rs;
	private Connection conn;
	private int NumberOfColumns;
	private int NumberOfRows;
	private Statement sql;
	private String[] Iterate;
	private JDBCDriverLoader JDBC_Driver_Loader;
public void
Initialize(String[] ar1) throws IOException
{       
	DatabaseMetaData dbmd;
	ResultSetMetaData r1;
	Properties JDBCProperties;
	Class JDBCDriverClass;
	Driver JDBCDriver;
	String url = ar1[2];
  	String userName = ar1[3]; 
  	String password = ar1[4];

  	NumberOfColumns=0;
  	conn=null;

  	try 
	{
		JDBC_Driver_Loader = new JDBCDriverLoader(new URL[]{new URL("jar:file://"+ar1[6]+"!/")});
		JDBCDriverClass = JDBC_Driver_Loader.loadClass(ar1[1]);
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
			if(ar1[5] == null)
			{
				sql.setQueryTimeout(5);
			}
			else
			{
				sql.setQueryTimeout(Integer.parseInt(ar1[5]));
			}

		}catch(Exception a)
		 {
		 	System.out.println("Query timeout shall not work");
		 }

  		rs = sql.executeQuery(ar1[0]);

  		r1=rs.getMetaData();
  		NumberOfColumns=r1.getColumnCount();

  		Iterate=new String[NumberOfColumns];
	} catch (Exception e)
	  {
  	  	e.printStackTrace();
	  }
}
public String[] 
ReturnResultSet()
{
	int i = 0;

	try
	{
		if(rs.next())
		{
			for(i=1;i<=NumberOfColumns;i++)
			{
    				Iterate[(i-1)]=rs.getString(i);
			}

			++NumberOfRows;				

			return Iterate;
		}

	}catch (Exception e)
	 {
		e.printStackTrace();
	 }

return null;
}
public void 
Close()
{
	try
	{
		rs.close();
		conn.close();
	}catch (Exception e) 
	 {
	 	e.printStackTrace();
	 }
}
public void 
Cancel()
{
	try
	{
			sql.cancel();
			rs.close();
			conn.close();
	}catch(Exception a)
	 {
		a.printStackTrace();
  	 }
}
}
