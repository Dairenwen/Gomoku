#pragma once
#include <unordered_map>
#include <mutex>
#include <websocketpp/server.hpp>   
#include <websocketpp/config/asio_no_tls.hpp>

class online_manager
{
    typedef websocketpp::server<websocketpp::config::asio> wsserver_t;
public:
    online_manager()
    {
    }
    ~online_manager()
    {
    }
    //建立连接，加入用户管理
    bool enter_game_hall(int userid,wsserver_t::connection_ptr& conn)
    {
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            _Hall_User[userid]=conn;
        }
        return true;
    }
    bool enter_game_room(int userid,wsserver_t::connection_ptr& conn)
    {
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            _Room_User[userid]=conn;
        }
        return true;
    }
    //断开连接，退出用户管理
    bool exit_game_hall(int userid)
    {
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            _Hall_User.erase(userid);
        }
        return true;
    }
    bool exit_game_room(int userid)
    {
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            _Room_User.erase(userid);
        }
        return true;
    }
    //判断是否在线
    bool is_in_game_hall(int userid)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        return _Hall_User.find(userid)!=_Hall_User.end();
    }
    bool is_in_game_room(int userid)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        return _Room_User.find(userid)!=_Room_User.end();
    }

    wsserver_t::connection_ptr get_conn_from_hall(int userid)
    {
        wsserver_t::connection_ptr conn=nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            auto it=_Hall_User.find(userid);
            if(it!=_Hall_User.end())
            {
                conn=it->second;
            }
        }
        return conn;
    }
    wsserver_t::connection_ptr get_conn_from_room(int userid)
    {
        wsserver_t::connection_ptr conn=nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            auto it=_Room_User.find(userid);
            if(it!=_Room_User.end())
            {
                conn=it->second;
            }   
        }
        return conn;
    }
private:
    //通过用户id找到对应的连接
    std::mutex mutex_t;
    std::unordered_map<int,wsserver_t::connection_ptr> _Hall_User;
    std::unordered_map<int,wsserver_t::connection_ptr> _Room_User;
};
