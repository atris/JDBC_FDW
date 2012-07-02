import java.sql.*;
import java.text.*;
import java.io.*;
public class JDBCUtils {
	private ResultSet rs;
	private Connection conn;
	private int NumberOfColumns;
	private int NumberOfRows;
	private Statement sql;
	private String[] Iterate;
public void 
Initialize(String[] ar1) throws IOException
{       
	DatabaseMetaData dbmd;
	ResultSetMetaData r1;
	String url = ar1[2];
  	String userName = ar1[3]; 
  	String password = ar1[4];

  	NumberOfColumns=0;
  	conn=null;

  	try 
	{
  		Class.forName(ar1[1]);
  		conn = DriverManager.getConnection(url, userName, password);

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
System.out.println("\n In cancel");
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
