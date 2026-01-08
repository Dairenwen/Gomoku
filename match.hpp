#pragma once
#include "log.hpp"
#include "room.hpp"
#include "online.hpp"
#include "db.hpp"
#include <queue>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>

template<class T>
class match_queue
{
public:
    match_queue()
    {
        LOG(DEB,"match_queue constructor");
    }
    ~match_queue()
    {
        LOG(DEB,"match_queue destructor");
    }
    //获取队列大小
    size_t size()
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        return _queue.size();
    }
    //判断是否为空
    bool empty()
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        return _queue.empty();
    }
    //加入队列，并唤醒所有线程，因为不知道哪一个线程等待的是这个数据
    bool push(const T& data)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        _queue.push_back(data);
        wake_up();
        return true;
    }

    void wait_for_size(size_t sz)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        while(_queue.size()<sz)
        {
            wait(lock);//等待线程被唤醒
        }
    }
    //弹出队列
    bool pop(T& data)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        while(_queue.empty())
        {
            wait(lock);//等待线程被唤醒
        }
        data = _queue.front();
        _queue.pop_front();
        return true;
    }
    //删除指定数据
    bool remove(const T& data)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        _queue.remove(data);//链表删除指定数据
        return true;
    }
    //唤醒线程,由于已经加锁了，所以不需要再加锁
    void wake_up()
    {
        cv.notify_all();
    }
    //等待线程
    void wait(std::unique_lock<std::mutex>& lock)
    {
        cv.wait(lock);//等待线程被唤醒
    }
private:
    std::list<T> _queue;//需要从中间删除数据，不能完全采用队列
    std::mutex mutex_t;//锁
    std::condition_variable cv;//条件变量
};


class match_manager
{
    using wsserver_t=websocketpp::server<websocketpp::config::asio>;
    using room_ptr=std::shared_ptr<room>;
public:
    match_manager(room_manager* rm,online_manager* onlinemgr,user_table* userdb)
        :_rm(rm),_onlinemgr(onlinemgr),_userdb(userdb)
        ,_th_weak(&match_manager::func_weak_queue,this),_th_normal(&match_manager::func_normal_queue,this),_th_strong(&match_manager::func_strong_queue,this)
    {
        LOG(DEB,"match_manager constructor");
    }
    ~match_manager()
    {
        LOG(DEB,"match_manager destructor");
    }

    bool add_user_to_queue(int userid)
    {
        //根据玩家的分数，决定添加到哪一个线程的哪一个队列
        Json::Value json_rest;
        if(_userdb->select_by_id(userid,json_rest)==false)
        {
            LOG(ERR,"userid %d not found in user table.",userid);
            return false;
        }
        int score=json_rest["score"].asInt();
        if(score<2000)
        {
            _mq_weak.push(userid);
        }
        else if(score<3000)
        {
            _mq_normal.push(userid);
        }
        else
        {
            _mq_strong.push(userid);
        }
        return true;
    }
    bool remove_user_from_queue(int userid)
    {
        Json::Value json_rest;
        if(_userdb->select_by_id(userid,json_rest)==false)
        {
            LOG(ERR,"userid %d not found in user table.",userid);
            return false;
        }
        int score=json_rest["score"].asInt();
        if(score<2000)
        {
            _mq_weak.remove(userid);
        }
        else if(score<3000)
        {
            _mq_normal.remove(userid);
        }
        else
        {
            _mq_strong.remove(userid);
        }
        return true;
    }

    void handler_match(match_queue<int>& mq)
    {
        while(1)
        {
            //1.从队列中弹出两个玩家，如果玩家数量不足，等待
            mq.wait_for_size(2);
            int userid1,userid2;
            mq.pop(userid1);
            mq.pop(userid2);
            //2.检测两个用户是否真的在线，如果存在掉线情况，重新加入队列
            wsserver_t::connection_ptr conn1=_onlinemgr->get_conn_from_hall(userid1);
            wsserver_t::connection_ptr conn2=_onlinemgr->get_conn_from_hall(userid2);
            if(conn1.get()==nullptr)
            {
                mq.push(userid1);
                continue;
            }
            if(conn2.get()==nullptr)
            {
                mq.push(userid2);
                continue;
            }
            //3.创建房间,失败重新添加用户至队列
            room_ptr rptr=_rm->create_room(userid1,userid2);
            if(rptr.get()==nullptr)
            {
                mq.push(userid1);
                mq.push(userid2);
                LOG(ERR,"create room failed,userid1 %d,userid2 %d.",userid1,userid2);
                continue;
            }
            //4.对用户进行通知
            Json::Value json_rest;
            json_rest["optype"]="match_success";
            json_rest["result"]=true;
            std::string body;
            jsontool::writer(json_rest,body);
            conn1->send(body,websocketpp::frame::opcode::text);
            conn2->send(body,websocketpp::frame::opcode::text);
        }
    }
private:
    void func_weak_queue()//处理青铜队列
    {
        handler_match(_mq_weak);
    }
    void func_normal_queue()//处理白银队列
    {
        handler_match(_mq_normal);
    }
    void func_strong_queue()//处理黄金队列
    {
        handler_match(_mq_strong);
    }

    std::thread _th_weak;//线程1-青铜队列
    match_queue<int> _mq_weak;//青铜队列
    std::thread _th_normal;//线程2-白银队列
    match_queue<int> _mq_normal;//白银队列
    std::thread _th_strong;//线程3-黄金队列
    match_queue<int> _mq_strong;//黄金队列
    room_manager* _rm;//房间管理器
    online_manager* _onlinemgr;//在线管理器
    user_table* _userdb;//用户数据库
};