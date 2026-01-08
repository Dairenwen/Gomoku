#pragma once
#include "log.hpp"
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#define SESSION_TIMEOUT 600000 //设置超时时间为十分钟
#define SESSION_FOREVER -1 //设置session永久存在
typedef enum{ UNLOGIN=0,LOGIN=1 } login_status;

class session
{
    using wsserver_t=websocketpp::server<websocketpp::config::asio>;
    public:
    session(int session_id)
        :_session_id(session_id)
        ,_status(UNLOGIN)
    {
        LOG(DEB,"session create success,id:%ld",_session_id);
    }
    ~session()
    {
        LOG(DEB,"session destroy success,id:%ld",_session_id);
    }

    bool Set_User(int userid)
    {
        _userid=userid;
        return true;
    }

    int Get_User_ID()
    {
        return _userid;
    }

    bool Is_Login()
    {
        return _status==LOGIN;
    }
    void Set_Timer(wsserver_t::timer_ptr timer)
    {
        _timer=timer;
    }

    wsserver_t::timer_ptr Get_Timer()
    {
        return _timer;
    }

    bool Set_Status(login_status status)
    {
        _status=status;
        return true;
    }

    uint64_t Get_Session_ID()
    {
        return _session_id;
    }

    login_status Get_Status()
    {
        return _status;
    }
private:
    uint64_t _session_id;//session id，用于标识session唯一
    int _userid;//session对应用户id
    login_status _status;//session登录状态
    wsserver_t::timer_ptr _timer;//session超时定时器
};

//session管理类：
//1.创建session 
//2.通过session id获取session指针
//3.为session设置定时器
//4.销毁session，过期自动销毁
//需要锁、总session id、session映射表
class session_manager
{
    using wsserver_t=websocketpp::server<websocketpp::config::asio>;
    using session_ptr=std::shared_ptr<session>;
public:
    session_manager(wsserver_t* ws)
        :_ws(ws)
        ,_total_session(0)
    {
        LOG(DEB,"session manager create success");
    }
    ~session_manager()
    {
        LOG(DEB,"session manager destroy success");
    }

    session_ptr create_session(int userid,login_status status)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        session_ptr sptr=std::make_shared<session>(++_total_session);
        sptr->Set_User(userid);
        sptr->Set_Status(status);
        _sessions[_total_session]=sptr;
        return sptr;
    }
    session_ptr get_sptr_by_sid(uint64_t session_id)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        auto it=_sessions.find(session_id);
        if(it==_sessions.end())
        {
            LOG(ERR,"get_sptr_by_sid failed,session not exist");
            return nullptr;
        }
        return it->second;
    }

    void append_session(uint64_t session_id,session_ptr sptr)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        _sessions.insert(std::make_pair(session_id,sptr));
    }

    bool set_timer(uint64_t session_id,int timeout)//设置session超时时间，单位毫秒
    {
        //依赖于websocketpp中的定时器来管理session的生命周期
        //在http登陆注册的时候session应该具备生命周期，
        //但是在websocket中建立起长连接后，session应该永久存在，
        //打个比方，建立起websocket后相当于就在下棋了，如果不是永久存在，那么一段时间后就会销毁session
        //这时候用户还在下着棋呢，你给我销毁了干什么！必须等到用户离开大厅后将session重新设为生命周期
        session_ptr sptr=get_sptr_by_sid(session_id);
        if(sptr==nullptr)
        {
            LOG(ERR,"set_timer failed,session not exist");
            return false;
        }
        wsserver_t::timer_ptr tptr=sptr->Get_Timer();
        //1.在session永久存在的情况下，将session设置为永久存在；
        if(tptr.get()==nullptr&&timeout==SESSION_FOREVER)
        {
            return true;
        }
        else if(tptr.get()==nullptr&&timeout!=SESSION_FOREVER)
        {
            //2.在session永久存在的情况下，将session设置指定时间后删除；
            wsserver_t::timer_ptr tmp_ptr=_ws->set_timer(timeout,std::bind(&session_manager::destroy_session,this,session_id));
            sptr->Set_Timer(tmp_ptr);
            return true;
        }
        else if(tptr.get()!=nullptr&&timeout==SESSION_FOREVER)
        {
            //3.在session设置了定时删除的情况下，将session设置为永久存在；
            tptr->cancel();//直接取消定时器，导致删除任务立即执行，session从映射表中移除
            sptr->Set_Timer(nullptr);
            //还需要将session重新添加到映射表中，但是cancel并不是立即删除的，需要加锁保护，确保cancel执行完成后再添加
            _ws->set_timer(0,std::bind(&session_manager::append_session,this,session_id,sptr));
            return true;
        }
        else if(tptr.get()!=nullptr&&timeout!=SESSION_FOREVER)
        {
            //4.在session设置了定时删除的情况下，将session设置指定时间后删除；
            tptr->cancel();//仍需取消定时器，避免引发重复删除
            sptr->Set_Timer(nullptr);
            _ws->set_timer(0,std::bind(&session_manager::append_session,this,session_id,sptr));
            wsserver_t::timer_ptr tmp_ptr=_ws->set_timer(timeout,std::bind(&session_manager::destroy_session,this,session_id));
            sptr->Set_Timer(tmp_ptr);
            return true;
        }
        else
        {
            //状态有误
            LOG(ERR,"set_timer failed,illegal status");
            return false;
        }
    }

    bool destroy_session(uint64_t session_id)//超时销毁session
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        auto it=_sessions.find(session_id);
        if(it==_sessions.end())
        {
            LOG(ERR,"destroy_session failed,session not exist");
            return false;
        }
        _sessions.erase(it);
        LOG(DEB,"destroy_session success(may be called for flashing),session id:%ld",session_id);
        return true;
    }
private:
    int _total_session;//当前已分配的session id
    std::mutex mutex_t;//锁
    wsserver_t* _ws;//websocket服务器指针
    std::unordered_map<uint64_t,session_ptr> _sessions;//session id到session的映射
};