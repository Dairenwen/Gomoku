#pragma once
#include <mutex>              // 互斥锁相关
#include <condition_variable> // 条件变量
#include "mysqltool.hpp"
#include <cassert>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>

class user_table
{
public:
    user_table(
        const std::string& host, 
        const std::string& user, 
        const std::string& password, 
        const std::string& database)
    {
        mysql=mysqltool::mysql_create(host.c_str(), user.c_str(), password.c_str(), database.c_str());
        assert(mysql!=nullptr);
    }
    ~user_table(){
        if(mysql!=nullptr)
        {
            mysqltool::mysql_destroy(mysql);
        }
    }

    bool create(Json::Value& user)//用户创建
    {
        #define INSERT_USER "insert into users(username,password,score,total_count,win_count)  values('%s', SHA2(CONCAT('%s', 'salt_abc123'), 256),1000,0,0);"//初始分数为1000,采用加密算法
        Json::Value tmp;
        if(select_by_name(user["username"].asCString(),tmp)==true)
        {
            LOG(WAR,"user has been registered,please create again.");
            return false;
        }

        char sql[4096]={0};
        sprintf(sql,INSERT_USER,user["username"].asCString(),user["password"].asCString());
        std::string str(sql);
        if(mysqltool::mysql_exec(mysql,str)==false)
        {
            LOG(ERR,"user create failed.");
            return false;
        }
        return true;
    }

    bool login(Json::Value& user)//用户登录
    {
        //以用户名和密码用于筛选，核对用户身份，并返回全部用户相关信息
        #define SELECT_USER "select * from users where username='%s'and password=SHA2(CONCAT('%s', 'salt_abc123'), 256);"
        char sql[4096]={0};
        sprintf(sql,SELECT_USER,user["username"].asCString(),user["password"].asCString());
        std::string str(sql);
        MYSQL_RES *result;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            if(mysqltool::mysql_exec(mysql,str)==false)
            {
                LOG(ERR,"user identification mysql-search wrong.");
                return false;
            }
            result = mysql_store_result(mysql);
        }
        int result_row=mysql_num_rows(result);
        if(result_row==0)
        {
            LOG(WAR,"user identification can't be confirmed.");
            mysql_free_result(result);
            return false;
        }
        else if(result_row>1)
        {
            LOG(ERR,"user has been exsisted.");
            mysql_free_result(result);
            return false;
        }
        else
        {
            MYSQL_ROW row=mysql_fetch_row(result);
            user["id"]=std::stoi(row[0]);            user["username"]=row[1];
            user["password"]=row[2];                 user["score"]=std::stoi(row[3]);
            user["total_count"]=std::stoi(row[4]);   user["win_count"]=std::stoi(row[5]);
            mysql_free_result(result);
            return true;
        }

    }

    bool select_by_name(const std::string& name,Json::Value& user)//名字查询
    {
        #define SELECT_BY_NAME "select * from users where username= '%s' ;"
        char sql[4096]={0};
        sprintf(sql,SELECT_BY_NAME,name.c_str());
        std::string str(sql);
        MYSQL_RES *result;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            if(mysqltool::mysql_exec(mysql,str)==false)
            {
                LOG(ERR,"user identification mysql-search wrong.");
                return false;
            }
            result = mysql_store_result(mysql);
        }
        int result_row=mysql_num_rows(result);
        if(result_row==0)
        {
            LOG(WAR,"user identification can't be confirmed,maybe the user has not been registered.");
            mysql_free_result(result);
            return false;
        }
        else if(result_row>1)
        {
            LOG(ERR,"user has been exsisted.");
            mysql_free_result(result);
            return false;
        }
        else
        {
            MYSQL_ROW row=mysql_fetch_row(result);
            user["id"]=std::stoi(row[0]);            user["username"]=row[1];
            user["password"]=row[2];                 user["score"]=std::stoi(row[3]);
            user["total_count"]=std::stoi(row[4]);   user["win_count"]=std::stoi(row[5]);
            mysql_free_result(result);
            return true;
        }
    }

    bool select_by_id(int id,Json::Value& user)//id查询
    {
        #define SELECT_BY_ID "select * from users where id=%d;"
        char sql[4096]={0};
        sprintf(sql,SELECT_BY_ID,id);
        std::string str(sql);
        MYSQL_RES *result;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            if(mysqltool::mysql_exec(mysql,str)==false)
            {
                LOG(ERR,"user identification mysql-search wrong.");
                return false;
            }
            result = mysql_store_result(mysql);
        }
        int result_row=mysql_num_rows(result);
        if(result_row==0)
        {
            LOG(WAR,"user identification can't be confirmed,maybe the user has not been registered.");
            mysql_free_result(result);
            return false;
        }
        else if(result_row>1)
        {
            LOG(ERR,"user has been exsisted.");
            mysql_free_result(result);
            return false;
        }
        else
        {
            MYSQL_ROW row=mysql_fetch_row(result);
            user["id"]=std::stoi(row[0]);            user["username"]=row[1];
            user["password"]=row[2];                 user["score"]=std::stoi(row[3]);
            user["total_count"]=std::stoi(row[4]);   user["win_count"]=std::stoi(row[5]);
            mysql_free_result(result);
            return true;
        }
    }
    
    //规定：输掉场次分数-50，赢得场次分数+50
    bool win(int id)//胜利场次增加，总场次增加，分数增加
    {
        std::cout<<"user_table win function called."<<std::endl;
        Json::Value tmp;
        if(select_by_id(id,tmp)==false)
        {
            LOG(ERR,"win function select id error.");
            return false;
        }

        #define UPDATE_WIN "update users set score=score+50,total_count=total_count+1,win_count=win_count+1 where id=%d;"
        char sql[4096]={0};
        sprintf(sql,UPDATE_WIN,id);
        std::string str(sql);
        if(mysqltool::mysql_exec(mysql,str)==false)
        {
            LOG(ERR,"user data update wrong.");
            return false;
        }
        return true;
    }

    bool lose(int id)//总场次增加，分数减少，其他不变
    {
        Json::Value tmp;
        if(select_by_id(id,tmp)==false)
        {
            LOG(ERR,"win function select id error.");
            return false;
        }

        #define UPDATE_LOSE "update users set score=score-50,total_count=total_count+1 where id=%d;"
        char sql[4096]={0};
        sprintf(sql,UPDATE_LOSE,id);
        std::string str(sql);
        if(mysqltool::mysql_exec(mysql,str)==false)
        {
            LOG(ERR,"user data update wrong.");
            return false;
        }
        return true;
    }
private:
    MYSQL* mysql;//句柄
    std::mutex mutex_t;
    //单独的函数可以保证原子性，但是多个函数组合就不行了，所以需要加锁
};