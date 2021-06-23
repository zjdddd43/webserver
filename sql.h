#ifndef SQL_H
#define SQL_H

#include <mysql/mysql.h>
#include <iostream>
#include <string.h>


class SQL
{
public:
    void init(std::string url, std::string user, std::string password, std::string database, int port);
    
    // 连接mysql
    MYSQL sql_connect();
    // 查询
    MYSQL_RES* sql_query(std::string query);
    // 插入
    bool sql_insert(std::string query);

    // 单例模式
    static SQL* get_instance()
    {
        static SQL instance;
        return &instance;
    }

private:
    SQL();
    ~SQL();

private:
    std::string m_url;          // 主机地址
    int m_port;                 // 数据库端口号
    std::string m_user;         // 登录数据库用户名
    std::string m_password;     // 登录数据库密码
    std::string m_database;     // 使用数据库名

private:
    MYSQL m_mysql;

};

#endif