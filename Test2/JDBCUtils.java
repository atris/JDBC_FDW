import java.sql.*;
import java.text.*;
import java.io.*;
public class JDBCUtils {
	private ResultSet rs;
	private Connection conn;
	private int NumberOfColumns;
	private int NumberOfRows;
	private String[] Iterate;
public int 
Initialize(String[] ar1) throws IOException
{
	Statement        sql;       
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

  		sql = conn.createStatement(ResultSet.TYPE_SCROLL_INSENSITIVE, ResultSet.CONCUR_READ_ONLY);
  		rs = sql.executeQuery(ar1[0]);

  		r1=rs.getMetaData();
  		NumberOfColumns=r1.getColumnCount();

  		rs.last();
  		NumberOfRows=rs.getRow();
  		rs.beforeFirst();

  		Iterate=new String[NumberOfColumns];
	} catch (Exception e) 
	  {
  	  	e.printStackTrace();
	  }
return NumberOfColumns;
}
public String[] 
ReturnResultSet()
{
	int i = 0;

	try
	{
		if(!rs.wasNull())
		{
			if(rs.next())
			{
				for(i=1;i<=NumberOfColumns;i++)
				{
    					Iterate[(i-1)]=rs.getString(i);
				}

			}

		}

	}catch (Exception e) 
	{
		e.printStackTrace();
	}

	return Iterate;
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
}
