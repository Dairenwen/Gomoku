#pragma once
#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include "log.hpp"

const std::string host = "47.122.127.6";
const std::string user = "drw";
const std::string password = "dairenwen1092";
const std::string database = "tmp_db";
const int port = 3306;

class mysqltool;
class mysqltool
{
public:
    mysqltool()=default;
    ~mysqltool()=default;

    static MYSQL* mysql_create(
        const std::string& host, 
        const std::string& user, 
        const std::string& password, 
        const std::string& database, 
        unsigned int port=3306)
    {
        //初始化mysql
        MYSQL *mysql = mysql_init(NULL);
        if(mysql == NULL)
        {
            LOG(ERR,"mysql init failed");
            return nullptr;
        }
        LOG(INF,"mysql init success");

        //连接mysql，参数：1.mysql指针，2.主机名，3.用户名，4.密码，5.数据库名，6.端口，7.套接字，8.标志
        if(mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, NULL, 0) == NULL)
        {
            LOG(ERR,"mysql connect failed");
            mysql_close(mysql);
            return nullptr;
        }//为了防止输入命令的编码问题，先来设置一下
        mysql_set_character_set(mysql, "utf8");
        return mysql;
    }

    static void mysql_destroy(MYSQL* mysql)
    {
        if(mysql!=nullptr)
        {
            mysql_close(mysql);
        }
    }
    
    static bool mysql_exec(MYSQL* mysql,std::string& str)
    {
        if(mysql_query(mysql, str.c_str()) != 0)
        {
            LOG(ERR,"mysql query failed");
            return false;
        }
        return true;
    }
};


int testmysql()
{
    //初始化mysql
    MYSQL *mysql = mysql_init(NULL);
    if(mysql == NULL)
    {
        std::cout<<"mysql init failed"<<std::endl;
        return -1;
    }
    std::cout<<"mysql init success"<<std::endl;
    std::cout<<"mysql version: "<<mysql_get_client_info()<<std::endl;

    //连接mysql，参数：1.mysql指针，2.主机名，3.用户名，4.密码，5.数据库名，6.端口，7.套接字，8.标志
    if(mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, NULL, 0) == NULL)
    {
        std::cout<<"mysql connect failed"<<std::endl;
        return -1; 
    }
    std::cout<<"mysql connect success"<<std::endl;
    //为了防止输入命令的编码问题，先来设置一下
    mysql_set_character_set(mysql, "utf8");
    std::string input;
    while(1)
    {
        std::cout<<"please input your command: "<<std::endl;
        getline(std::cin, input);
        if(input == "exit"||input == "quit")
        {
            break;
        }
        if(mysql_query(mysql, input.c_str()) != 0)//使用mysql_query执行sql语句，成功返回0，失败返回非0
        {
            std::cout<<"mysql query failed"<<std::endl;
            continue;
        }
    
        MYSQL_RES *result = mysql_store_result(mysql);//使用mysql_store_result将查询结果存储在result中
        //这里直接理解为MYSQL_RES是一个指向结果集的指针，每个结果集包含多个char*，每个char*指向一个字符串，每个字符串代表一行数据
        if(result == NULL)
        {
            std::cout<<"mysql store result failed"<<std::endl;
            continue;
        }
        int rows=mysql_num_rows(result);
        int cols=mysql_num_fields(result);
        std::cout<<"行数："<<rows<<std::endl;
        std::cout<<"列数："<<cols<<std::endl;
        //输出表头
        MYSQL_FIELD *field=mysql_fetch_field(result);//还有一个mysql_fetch_fields函数，返回一个MYSQL_FIELD*数组，每个MYSQL_FIELD*指向一个字段
        for(int i=0;i<cols;i++)
        {
            std::cout<<field[i].name<<"\t\t\t";
        }
        //又或者
        // MYSQL_FIELD *fields=mysql_fetch_fields(result);
        // for(int i=0;i<cols;i++)
        // {
        //     cout<<fields[i].name<<"\t\t\t";
        // }
        // cout<<endl;
        //输出内容
        for(int i=0;i<rows;i++)
        {
            MYSQL_ROW row=mysql_fetch_row(result);//使用mysql_fetch_row获取一行数据,返回一个char*数组，每次调用完自动指向下一个数据
            for(int j=0;j<cols;j++)
            {
                std::cout<<row[j]<<"\t\t\t";
            }
            std::cout<<std::endl;
        }
        //获取完毕，释放结果集，释放后不能再使用result指针
        mysql_free_result(result);
    }
    //最后需要关闭mysql
    mysql_close(mysql);
    std::cout<<"mysql close success"<<std::endl;
    return 0;
}