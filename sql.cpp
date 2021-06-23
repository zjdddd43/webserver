#include "sql.h"

SQL::SQL()
{
    
}

SQL::~SQL()
{
    // mysql_close(&m_mysql);
}

void SQL::init(std::string url, std::string user, std::string password, std::string database, int port)
{
    std::cout << "init sql..." << std::endl;
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = password;
    m_database = database;

    mysql_init(&m_mysql);
    if(mysql_real_connect(&m_mysql, m_url.c_str(), m_user.c_str(), m_password.c_str(), m_database.c_str(), m_port, NULL, 0))
    {
        std::cout << "connect to mysql success..." << std::endl;
    }

}

MYSQL_RES* SQL::sql_query(std::string query)
{
    MYSQL_RES* result;

    int res = mysql_query(&m_mysql, query.c_str());
    
    if(!res)
    {
        std::cout << "mysql query success..." << std::endl;
        result = mysql_store_result(&m_mysql);

    }
    else
    {
        std::cout << "mysql query failed..." << std::endl;
    }
    return result;
}

bool SQL::sql_insert(std::string query)
{
    int res = mysql_query(&m_mysql, query.c_str());
    if(!res)
    {
        std::cout << "insert success..." << std::endl;
        return true;
    }
    else
    {
        std::cout << "insert failed..." << std::endl;
        return false;
    }
}