#pragma once
#include "online.hpp"
#include "room.hpp"
#include "filetool.hpp"
#include "session.hpp"
#include "match.hpp"
#include "db.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include <websocketpp/server.hpp>   
#include <websocketpp/config/asio_no_tls.hpp>

// 1. 实例化server对象
// 2. 设置日志输出等级
// 3. 初始化asio框架种的调度器
// 4. 设置业务处理回调函数（具体业务处理的函数由我们自己实现）
// 5. 设置服务器监听端口
// 6. 开始获取新建连接
// 7. 启动服务器

class gobang_server
{
    using wsserver_t=websocketpp::server<websocketpp::config::asio>;
    using session_ptr=std::shared_ptr<session>;
    using room_ptr=std::shared_ptr<room>;
public:
    gobang_server(const std::string& host, 
        const std::string& user, 
        const std::string& password, 
        const std::string& database)
        :_online_mgr(),
        _session_mgr(&_wssr),
        _user_table(host,user,password,database),
        _room_mgr(&_user_table,&_online_mgr),
        _match_mgr(&_room_mgr,&_online_mgr,&_user_table)
    {
        //设置日志等级
        _wssr.set_access_channels(websocketpp::log::alevel::none);
        //初始化调度器
        _wssr.init_asio();
        _wssr.set_reuse_addr(true);//允许地址重用,消除time_wait状态
        //设置回调函数
        _wssr.set_open_handler(bind(&gobang_server::open_handler,this,std::placeholders::_1));
        _wssr.set_close_handler(bind(&gobang_server::close_handler,this,std::placeholders::_1));
        _wssr.set_message_handler(bind(&gobang_server::message_handler,this,std::placeholders::_1,std::placeholders::_2));  
        _wssr.set_http_handler(bind(&gobang_server::http_handler,this,std::placeholders::_1));
    }

    void run(uint16_t port=8080)
    {
        //设置监听端口
        _wssr.listen(port);//默认为8080端口
        //启动监听
        _wssr.start_accept();
        //启动服务器
        _wssr.run();
    }
    //静态资源请求的处理
    void file_handler(wsserver_t::connection_ptr cptr,std::string uri)
    {
        websocketpp::http::status_code::value code;
        std::string body;
        filetool::read(_webroot + uri, body,code);
        cptr->set_status(code);
        cptr->set_body(body);
    }
    //处理临时错误的请求
    void process_handler_tmp(bool result,wsserver_t::connection_ptr cptr,std::string reason,websocketpp::http::status_code::value code)
    {
        Json::Value err_ret;
        err_ret["result"]=result;
        err_ret["reason"]=reason;
        std::string body_err;
        jsontool::writer(err_ret,body_err);
        cptr->set_status(code);
        cptr->set_body(body_err);
    }
    //处理长连接的错误请求
    void process_handler_long_str(bool result,wsserver_t::connection_ptr cptr,std::string reason,websocketpp::http::status_code::value code)
    {
        Json::Value err_ret;
        err_ret["result"]=result;
        err_ret["reason"]=reason;
        std::string body_err;
        jsontool::writer(err_ret,body_err);
        cptr->send(body_err);
    }

    void process_handler_long_json(bool result,wsserver_t::connection_ptr cptr,Json::Value err_ret,websocketpp::http::status_code::value code)
    {
        std::string body_err;
        jsontool::writer(err_ret,body_err);
        cptr->send(body_err);
    }
    //用户注册请求功能的处理
    void register_handler(wsserver_t::connection_ptr cptr)
    {
        //获取正文
        Json::Value register_req;
        std::string req_body=cptr->get_request_body();
        //进行序列化解析出Json对象
        if(jsontool::reader(register_req,req_body)==false)
        {
            //注册请求格式错误
            process_handler_tmp(false,cptr,"register request format error",websocketpp::http::status_code::bad_request);
            return;
        }
        //对注册请求进行校验
        std::string username=register_req["username"].asString();
        std::string password=register_req["password"].asString();
        if(username.empty() || password.empty())
        {
            //用户名或密码为空
            process_handler_tmp(false,cptr,"username or password is empty",websocketpp::http::status_code::bad_request);
            return;
        }
        //校验通过，创建用户
        if(_user_table.create(register_req)==false)
        {
            //用户创建失败
            process_handler_tmp(false,cptr,"user create failed",websocketpp::http::status_code::bad_request);
            return;
        }
        //用户创建成功
        process_handler_tmp(true,cptr,"user create success",websocketpp::http::status_code::ok);
        return;

    }
    //用户登录请求功能的处理
    void login_handler(wsserver_t::connection_ptr cptr)
    {
        //获取正文
        Json::Value login_req;
        std::string req_body=cptr->get_request_body();
        //进行序列化解析出Json对象
        if(jsontool::reader(login_req,req_body)==false)
        {
            //登录请求格式错误
            process_handler_tmp(false,cptr,"login request format error",websocketpp::http::status_code::bad_request);
            return;
        }
        //对注册请求进行校验
        std::string username=login_req["username"].asString();
        std::string password=login_req["password"].asString();
        if(username.empty() || password.empty())
        {
            //用户名或密码为空
            process_handler_tmp(false,cptr,"username or password is empty",websocketpp::http::status_code::bad_request);
            return;
        }
        //校验通过
        if(_user_table.login(login_req)==false)
        {
            //用户登录失败
            process_handler_tmp(false,cptr,"The username does not exist or the password is incorrect.",websocketpp::http::status_code::bad_request);
            return;
        }
        //需要创建session，并返回唯一的session_id，并记得设置定时器
        int userid=login_req["id"].asInt();
        session_ptr sptr=_session_mgr.create_session(userid,LOGIN);
        if(_session_mgr.set_timer(sptr->Get_Session_ID(),SESSION_TIMEOUT)==false)
        {
            //设置定时器失败
            process_handler_tmp(false,cptr,"session timer set failed",websocketpp::http::status_code::bad_request);
            return;
        }
        cptr->append_header("Set-Cookie","SSID="+std::to_string(sptr->Get_Session_ID())+";path=/");
        //登录成功
        process_handler_tmp(true,cptr,"user login success",websocketpp::http::status_code::ok);
        return;
    }

    uint64_t get_session_id(wsserver_t::connection_ptr cptr)
    {
        //cookie中的格式为：SSID=XXX;path=XXX;必须以;进行分割
        //获取请求中的Cookie，得到sessionid
        std::string cookie=cptr->get_request_header("Cookie");
        std::cout<<"cookie:"<<cookie<<std::endl;
        if(cookie.empty())
        {
            //没有sessionid
            return -1;
        }
        else
        {
            std::vector<std::string> cookie_parts;
            stringtool::split(cookie,";",cookie_parts);
            for(const auto&e:cookie_parts)
            {
                if(e.substr(0,4)=="SSID") return std::stoull(e.substr(5));
            }
            return -1;
        }
    }

    //用户信息获取请求的处理
    void info_handler(wsserver_t::connection_ptr cptr)
    {
        //获取用户信息，展示到hall.html
        //1.获取请求中的Cookie，得到sessionid，如果没有返回错误
        uint64_t session_id=get_session_id(cptr);
        if(session_id==-1)
        {
            //没有sessionid
            process_handler_tmp(false,cptr,"no session id",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.在session中查找是否还未过期，没有找到相应的session_id，说明已经过期，让用户重新登录
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(sptr==nullptr)
        {
            //没有找到session，说明登录过期，需要重新登录
            process_handler_tmp(false,cptr,"session expired",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.从user_table取出用户信息
        Json::Value user_info;
        if(_user_table.select_by_id(sptr->Get_User_ID(),user_info)==false)
        {
            //用户信息获取失败
            process_handler_tmp(false,cptr,"user info get failed",websocketpp::http::status_code::bad_request);
            return;
        }
        //4.将用户信息返回给客户端
        std::string body;
        jsontool::writer(user_info,body);
        cptr->set_body(body);
        cptr->set_status(websocketpp::http::status_code::ok);
        cptr->append_header("Content-Type","application/json");
        //5.刷新session定时器
        _session_mgr.set_timer(session_id,SESSION_TIMEOUT);//设定时间有待商榷
        return;
    }

    void hall_open_handler(wsserver_t::connection_ptr cptr)
    {
        //1.先判断是否能获取到cookie和session
        uint64_t session_id=get_session_id(cptr);
        if(session_id==-1)
        {
            //没有sessionid
            process_handler_long_str(false,cptr,"no session id",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.在session中查找是否还未过期，没有找到相应的session_id，说明已经过期，让用户重新登录
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(sptr==nullptr)
        {
            //没有找到session，说明登录过期，需要重新登录
            process_handler_long_str(false,cptr,"session expired, please login again",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.判断是否已经进入过大厅或者已经在游戏房间内了
        if(_online_mgr.is_in_game_hall(sptr->Get_User_ID())||_online_mgr.is_in_game_room(sptr->Get_User_ID()))
        {
            //已经进入过大厅或者已经在游戏房间内了
            process_handler_long_str(false,cptr,"you have already entered the game hall or game room",websocketpp::http::status_code::bad_request);
            return;
        }
        //4.将用户加入到游戏大厅
        _online_mgr.enter_game_hall(sptr->Get_User_ID(),cptr);
        //5.将session设置为永久存在
        _session_mgr.set_timer(session_id,SESSION_FOREVER);
        process_handler_long_str(true,cptr,"user enter game hall success",websocketpp::http::status_code::ok);
        return;
    }

    void game_open_handler(wsserver_t::connection_ptr cptr)
    {
        //1.先判断是否能获取到cookie和session
        uint64_t session_id=get_session_id(cptr);
        if(session_id==-1)
        {
            //没有sessionid
            process_handler_long_str(false,cptr,"no session id",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.在session中查找是否还未过期，没有找到相应的session_id，说明已经过期，让用户重新登录
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(sptr==nullptr)
        {
            //没有找到session，说明登录过期，需要重新登录
            process_handler_long_str(false,cptr,"session expired, please login again",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.判断是否在大厅或者在游戏房间内了
        if(_online_mgr.is_in_game_hall(sptr->Get_User_ID())||_online_mgr.is_in_game_room(sptr->Get_User_ID()))
        {
            process_handler_long_str(false,cptr,"user is in game hall or game room",websocketpp::http::status_code::bad_request);
            return;
        }
        //4.判断房间是否存在
        room_ptr rptr=_room_mgr.get_rptr_by_uid(sptr->Get_User_ID());
        if(rptr==nullptr)
        {
            process_handler_long_str(false,cptr,"room not found",websocketpp::http::status_code::bad_request);
            return;
        }
        //5.将用户加入到游戏房间
        _online_mgr.enter_game_room(sptr->Get_User_ID(),cptr);
        //6.将session设置为永久存在
        _session_mgr.set_timer(session_id,SESSION_FOREVER);
        //7.将房间信息和用户信息返回给客户端
        Json::Value room_info,own_info,rival_info;
        std::string own_info_body,rival_info_body;
        int white_id=rptr->Get_White_ID(),black_id=rptr->Get_Black_ID();
        int own_id=sptr->Get_User_ID(),rival_id=(own_id==white_id)?black_id:white_id;
        room_info["optype"]="room_ready";
        room_info["roomid"]=rptr->Get_room_id();
        room_info["own_id"]=own_id;
        room_info["white_id"]=white_id;
        room_info["black_id"]=black_id;
        room_info["rival_id"]= rival_id;
        if(_user_table.select_by_id(own_id,own_info)==false)
        {
            //用户信息获取失败
            process_handler_long_str(false,cptr,"white user info get failed",websocketpp::http::status_code::bad_request);
            return;
        }
        jsontool::writer(own_info,own_info_body);
        room_info["own_info"]=own_info_body;
        if(_user_table.select_by_id(rival_id,rival_info)==false)
        {
            //用户信息获取失败
            process_handler_long_str(false,cptr,"black user info get failed",websocketpp::http::status_code::bad_request);
            return;
        }
        jsontool::writer(rival_info,rival_info_body);
        room_info["rival_info"]=rival_info_body;
        process_handler_long_json(true,cptr,room_info,websocketpp::http::status_code::ok);
        return;
    }
    void open_handler(websocketpp::connection_hdl hdl)
    {
        //这里进行建立起websocket长连接后的处理
        wsserver_t::connection_ptr cptr= _wssr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req=cptr->get_request();
        std::string method=req.get_method();
        std::string uri = req.get_uri();

        std::cout<<"method:"<<req.get_method()<<std::endl;
        std::cout<<"uri:"<<req.get_uri()<<std::endl;

        if(uri=="/hall")
        {
            hall_open_handler(cptr);
        }
        else if(uri=="/game")
        {
            game_open_handler(cptr);
        }
        
        std::cout<<"websocket握手成功!"<<std::endl;
    }

    void hall_close_handler(wsserver_t::connection_ptr cptr)
    {
        //1.先判断是否能获取到cookie和session
        uint64_t session_id=get_session_id(cptr);
        if(session_id==-1)
        {
            //没有sessionid
            process_handler_long_str(false,cptr,"no session id",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.在session中查找是否还未过期，没有找到相应的session_id，说明已经过期，让用户重新登录
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(sptr==nullptr)
        {
            //没有找到session，说明登录过期，需要重新登录
            process_handler_long_str(false,cptr,"session expired, please login again",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.从大厅中移除用户
        _online_mgr.exit_game_hall(sptr->Get_User_ID());
        //4.将session设置定时删除
        _session_mgr.set_timer(session_id,SESSION_TIMEOUT);
        process_handler_long_str(true,cptr,"user leave game hall success",websocketpp::http::status_code::ok);
        return;
    }
    void game_close_handler(wsserver_t::connection_ptr cptr)
    {
        //1.先判断是否能获取到cookie和session
        uint64_t session_id=get_session_id(cptr);
        if(session_id==-1)
        {
            //没有sessionid
            process_handler_long_str(false,cptr,"no session id",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.在session中查找是否还未过期，没有找到相应的session_id，说明已经过期，让用户重新登录
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(sptr==nullptr)
        {
            //没有找到session，说明登录过期，需要重新登录
            process_handler_long_str(false,cptr,"session expired, please login again",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.从游戏房间中移除用户，同时从房间中移除用户
        _online_mgr.exit_game_room(sptr->Get_User_ID());
        _room_mgr.del_user_in_room(sptr->Get_User_ID());
        //4.将session设置定时删除
        _session_mgr.set_timer(session_id,SESSION_TIMEOUT);
        process_handler_long_str(true,cptr,"user leave game room success",websocketpp::http::status_code::ok);
        return;
    }
    void close_handler(websocketpp::connection_hdl hdl)
    {
        wsserver_t::connection_ptr cptr= _wssr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req=cptr->get_request();
        std::string method=req.get_method();
        std::string uri = req.get_uri();

        std::cout<<"method:"<<req.get_method()<<std::endl;
        std::cout<<"uri:"<<req.get_uri()<<std::endl;

        if(uri=="/hall")
        {
            hall_close_handler(cptr);
        }
        else if(uri=="/game")
        {
            game_close_handler(cptr);
        }

        std::cout<<"websocket连接断开"<<std::endl;
    }

    void hall_message_handler(wsserver_t::connection_ptr cptr,wsserver_t::message_ptr ptr)
    {
        //获取游戏大厅的处理操作
        //1.获取用户信息
        uint64_t session_id=get_session_id(cptr);
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(session_id==-1||sptr==nullptr)
        {
            //没有sessionid或者session过期
            process_handler_long_str(false,cptr,"no session id or session expired",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.获取请求信息
        std::string body=ptr->get_payload();
        Json::Value json_req,json_ret;
        if(jsontool::reader(json_req,body)==false)
        {
            //json解析失败
            process_handler_long_str(false,cptr,"json parse failed",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.处理匹配请求并向用户返回匹配结果
        if(json_req["optype"].asString()=="match_start")
        {
            //匹配请求，将用户加入匹配队列
            _match_mgr.add_user_to_queue(sptr->Get_User_ID());
            json_ret["optype"]="match_start";
            json_ret["result"]=true;
            json_ret["reason"]="match start success,wait for match";
            process_handler_long_json(true,cptr,json_ret,websocketpp::http::status_code::ok);
        }
        else if(json_req["optype"].asString()=="match_stop")
        {
            //取消匹配请求,将用户从匹配队列中移除
            _match_mgr.remove_user_from_queue(sptr->Get_User_ID());
            json_ret["optype"]="match_stop";
            json_ret["result"]=true;
            json_ret["reason"]="match stop success,wait for next match";
            std::string body_str;
            jsontool::writer(json_ret,body_str);
            process_handler_long_json(true,cptr,json_ret,websocketpp::http::status_code::ok);
        }
        else
        {
            json_ret["optype"]="unknown";
            json_ret["result"]=false;
            json_ret["reason"]="unknown optype";
            std::string body_str;
            jsontool::writer(json_ret,body_str);
            process_handler_long_json(true,cptr,json_ret,websocketpp::http::status_code::bad_request);
        }
        return;
    }
    void game_message_handler(wsserver_t::connection_ptr cptr,wsserver_t::message_ptr ptr)
    {
        //获取游戏房间的处理操作
        //1.获取用户信息,获取房间信息
        uint64_t session_id=get_session_id(cptr);
        session_ptr sptr=_session_mgr.get_sptr_by_sid(session_id);
        if(session_id==-1||sptr==nullptr)
        {
            //没有sessionid或者session过期
            process_handler_long_str(false,cptr,"no session id or session expired",websocketpp::http::status_code::bad_request);
            return;
        }
        //2.获取房间id并派发任务
        room_ptr rptr=_room_mgr.get_rptr_by_uid(sptr->Get_User_ID());
        if(rptr==nullptr)
        {
            //没有找到房间，说明用户不在游戏房间中
            process_handler_long_str(false,cptr,"user not in game room",websocketpp::http::status_code::bad_request);
            return;
        }
        //3.获取请求信息
        std::string body=ptr->get_payload();
        Json::Value json_req;
        if(jsontool::reader(json_req,body)==false)
        {
            //json解析失败
            process_handler_long_str(false,cptr,"json parse failed",websocketpp::http::status_code::bad_request);
            return;
        }
        //处理匹配请求
        rptr->handler_request(json_req);
        return;
    }
    void message_handler(websocketpp::connection_hdl hdl,wsserver_t::message_ptr ptr)
    {
        //获取游戏大厅的处理操作
        wsserver_t::connection_ptr cptr= _wssr.get_con_from_hdl(hdl);
        websocketpp::http::parser::request req=cptr->get_request();
        std::string method=req.get_method();
        std::string uri = req.get_uri();

        std::cout<<"method:"<<req.get_method()<<std::endl;
        std::cout<<"uri:"<<req.get_uri()<<std::endl;

        if(uri=="/hall")
        {
            hall_message_handler(cptr,ptr);
        }
        else if(uri=="/game")
        {
            game_message_handler(cptr,ptr);
        }
    }


    void http_handler(websocketpp::connection_hdl hdl)
    {
        //给用户返回页面,参照connection类
        wsserver_t::connection_ptr cptr= _wssr.get_con_from_hdl(hdl);//connection类

        //获取客户请求对象
        websocketpp::http::parser::request req=cptr->get_request();
        std::string method=req.get_method();
        std::string uri = req.get_uri();

        std::cout<<"method:"<<req.get_method()<<std::endl;
        std::cout<<"uri:"<<req.get_uri()<<std::endl;
        //因为可能返回图标，让浏览器自己处理
        // 当访问根路径时，自动指向index.html
        if (uri == "/" || uri == "") 
        {
            uri = "/index.html";
        }

        if(method=="GET")
        {
            if(uri=="/info") info_handler(cptr);
            else file_handler(cptr,uri);
        }
        else if(method=="POST")
        {
            if(uri=="/register")//这里对应的是ajax的url
            {
                register_handler(cptr);
            }
            else if(uri=="/login")
            {
                login_handler(cptr);
            }
            else
            {
                process_handler_tmp(false,cptr,"uri not found",websocketpp::http::status_code::not_found);
            }
        }
        else
        {
            process_handler_tmp(false,cptr,"method not found",websocketpp::http::status_code::bad_request);
        }
    }
    ~gobang_server()
    {
    }

private:
    wsserver_t _wssr;
    online_manager _online_mgr;
    session_manager _session_mgr;
    user_table _user_table;
    room_manager _room_mgr;
    match_manager _match_mgr;
    std::string _webroot="../wwwroot";//web根目录
};



class Test_WSServer
{
    using wsserver_t=websocketpp::server<websocketpp::config::asio>;
public:
    Test_WSServer()
    {
    }

    void Init()
    {
        //设置日志等级
        wssr.set_access_channels(websocketpp::log::alevel::none);
        //初始化调度器
        wssr.init_asio();
        wssr.set_reuse_addr(true);//允许地址重用,消除time_wait状态
        //设置回调函数
        wssr.set_open_handler(bind(&Test_WSServer::open_handler,this,std::placeholders::_1));
        wssr.set_close_handler(bind(&Test_WSServer::close_handler,this,std::placeholders::_1));
        wssr.set_message_handler(bind(&Test_WSServer::message_handler,this,std::placeholders::_1,std::placeholders::_2));
        wssr.set_http_handler(bind(&Test_WSServer::http_handler,this,std::placeholders::_1));
        //设置监听端口
        wssr.listen(8080);//设置8080端口
        //启动监听
        wssr.start_accept();
        //启动服务器
        wssr.run();
    }

    void open_handler(websocketpp::connection_hdl hdl)
    {
        std::cout<<"websocket握手成功!"<<std::endl;
    }
    void close_handler(websocketpp::connection_hdl hdl)
    {
        std::cout<<"websocket连接断开"<<std::endl;
    }
    void message_handler(websocketpp::connection_hdl hdl,wsserver_t::message_ptr ptr)
    {
        //获取消息并发送
        wsserver_t::connection_ptr cptr= wssr.get_con_from_hdl(hdl);//connection类
        std::cout<<"wmsg:"<<ptr->get_payload()<<std::endl;
        std::string ret="client says:"+ptr->get_payload();
        cptr->send(ret,websocketpp::frame::opcode::text);//文本内容
    }
    void http_handler(websocketpp::connection_hdl hdl)
    {
        //给用户返回页面,参照connection类
        wsserver_t::connection_ptr cptr= wssr.get_con_from_hdl(hdl);//connection类
        std::cout<<"http 正文：";
        std::cout<<cptr->get_request_body()<<std::endl;

        //获取客户请求对象
        websocketpp::http::parser::request req=cptr->get_request();
        std::cout<<"method:"<<req.get_method()<<std::endl;
        std::cout<<"uri:"<<req.get_uri()<<std::endl;

        //回应
        cptr->set_status(websocketpp::http::status_code::ok);
        cptr->append_header("Content-Type", "text/html; charset=utf-8");
        cptr->set_body(cptr->get_request_body());
        cptr->set_timer(50000,bind(&Test_WSServer::timeout_handler,this,hdl));

    }
    
    void timeout_handler(websocketpp::connection_hdl hdl)
    {
        std::cout<<"timeout_handler"<<std::endl;  
    }
    ~Test_WSServer()
    {
        
    }

    
private:
    wsserver_t wssr;
};







