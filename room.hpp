#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "db.hpp"
#include "log.hpp"
#include "online.hpp"
#include "stringtool.hpp"
#include "jsontool.hpp"
#include <unordered_set>
#include <unordered_map>
//游戏房间的设计：需要房间的ID，房间状态，在线玩家的数量，白棋玩家ID，黑棋玩家ID
//用户mysql的句柄以及通信连接句柄，棋盘状态等

#define DEF_RAW 15
#define DEF_COL 15
#define WHITE 1
#define BLACK 2
typedef enum{GAME_START,GAME_OVER} room_status;

class room
{
    using wsserver_t=websocketpp::server<websocketpp::config::asio>;
public:
    room(int room_id,user_table* userdb,online_manager* onlinemgr)
        :_room_id(room_id),_status(GAME_START),_player_count(0),
        _userdb(userdb),_onlinemgr(onlinemgr),_board(DEF_RAW,std::vector<int>(DEF_COL,0))
    {
        LOG(DEB,"room %d created.",room_id);
    }
    ~room()
    {
        LOG(DEB,"room %d destroyed.",_room_id);
    }


    bool Set_White_Player(int userid)
    {
        _white_id=userid;
        _player_count++;
        return true;
    }

    bool Set_Black_Player(int userid)
    {
        _black_id=userid;
        _player_count++;
        return true;
    }

    int Get_room_id() {return _room_id;}

    room_status Get_room_status() { return _status;}

    int& Get_Player_Count() {return _player_count;}

    int Get_White_ID() {return _white_id;}

    int Get_Black_ID() {return _black_id;}

    bool check(std::vector<std::vector<int>>& board,int row,int col)
    {
        //传入已经合法
        int color=board[row][col];
        int count=1;
        //横向检查
        for(int i=col-1;i>=0;i--)
        {
            if(board[row][i]!=color)
            {
                break;
            }
            count++;
        }
        for(int i=col+1;i<DEF_COL;i++)
        {
            if(board[row][i]!=color)
            {
                break;
            }
            count++;
        }
        if(count>=5)
        {
            return true;
        }
        //纵向检查
        count=1;
        for(int i=row-1;i>=0;i--)
        {
            if(board[i][col]!=color)
            {
                break;
            }
            count++;
        }
        for(int i=row+1;i<DEF_RAW;i++)
        {
            if(board[i][col]!=color)
            {
                break;
            }
            count++;
        }
        if(count>=5)
        {
            return true;
        }
        //斜向检查
        count=1;
        for(int i=row-1,j=col-1;i>=0&&j>=0;i--,j--)
        {
            if(board[i][j]!=color)
            {
                break;
            }
            count++;
        }
        for(int i=row+1,j=col+1;i<DEF_RAW&&j<DEF_COL;i++,j++)
        {
            if(board[i][j]!=color)
            {
                break;
            }
            count++;
        }
        if(count>=5)
        {
            return true;
        }
        //反斜向检查
        count=1;
        for(int i=row-1,j=col+1;i>=0&&j<DEF_COL;i--,j++)
        {
            if(board[i][j]!=color)
            {
                break;
            }
            count++;
        }
        for(int i=row+1,j=col-1;i<DEF_RAW&&j>=0;i++,j--)
        {
            if(board[i][j]!=color)
            {
                break;
            }
            count++;
        }
        if(count>=5)
        {
            return true;
        }
        return false;
    }
    
    //处理下棋任务
    Json::Value handler_chess(Json::Value& chess_val)
    {
        Json::Value json_rest;
        int room_id=chess_val["room_id"].asInt();
        int cur_user_id=chess_val["userid"].asInt();
        int cur_row=chess_val["row"].asInt();
        int cur_col=chess_val["col"].asInt();
        //1.判断两个用户是否同时在线
        if(_onlinemgr->is_in_game_room(_white_id)==false)
        {
            json_rest["optype"]="put_chess";
            json_rest["result"]=true;
            json_rest["reason"]="user not in room";
            json_rest["userid"]=cur_user_id;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=cur_row;
            json_rest["col"]=cur_col;
            json_rest["winner"]=_black_id;
            return json_rest;
        }
        if(_onlinemgr->is_in_game_room(_black_id)==false)
        {
            json_rest["optype"]="put_chess";
            json_rest["result"]=true;
            json_rest["reason"]="user not in room";
            json_rest["userid"]=cur_user_id;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=cur_row;
            json_rest["col"]=cur_col;
            json_rest["winner"]=_white_id;
            return json_rest;
        }

        //2.判断落子是否合法
        if(cur_row<0||cur_row>=DEF_RAW||cur_col<0||cur_col>=DEF_COL)
        {
            json_rest["optype"]="put_chess";
            json_rest["result"]=false;
            json_rest["reason"]="invalid position";
            json_rest["userid"]=cur_user_id;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=cur_row;
            json_rest["col"]=cur_col;
            return json_rest;
        }
        else if(_board[cur_row][cur_col]!=0)
        {
            json_rest["optype"]="put_chess";
            json_rest["result"]=false;
            json_rest["reason"]="position has been planed";
            json_rest["userid"]=cur_user_id;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=cur_row;
            json_rest["col"]=cur_col;
            return json_rest;
        }
        else
        {
            _board[cur_row][cur_col] = (cur_user_id == _white_id ? WHITE : BLACK);
        }
        //3.判断是否有赢家产生
        bool result=check(_board,cur_row,cur_col);
        if(result)
        {
            //需要更新数据库
            int win_id=cur_user_id,lose_id=(cur_user_id==_white_id?_black_id:_white_id);
            _userdb->win(win_id);
            _userdb->lose(lose_id);
            // 标记房间已结束，避免后续的退出处理重复写入数据库
            _status = GAME_OVER;
            json_rest["optype"]="put_chess";
            json_rest["result"]=true;
            json_rest["reason"]="The winner has been determined";
            json_rest["userid"]=cur_user_id;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=cur_row;
            json_rest["col"]=cur_col;
            json_rest["winner"]=win_id;
            return json_rest;
        }
        else
        {
            json_rest["optype"]="put_chess";
            json_rest["result"]=true;
            json_rest["reason"]="the chess has been planed successfully";
            json_rest["userid"]=cur_user_id;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=cur_row;
            json_rest["col"]=cur_col;
            json_rest["winner"]=0;
            return json_rest;
        }
    }
    //处理聊天任务
    Json::Value handler_chat(Json::Value& chat_val)
    {
        Json::Value json_rest;
        int room_id=chat_val["room_id"].asInt();
        int cur_user_id=chat_val["userid"].asInt();
        std::string msg=chat_val["message"].asString();
        //1.检测并替换敏感词
        std::string filtered_msg=stringtool::filterSensitiveWords(msg);
        //2.将过滤后的消息广播给所有在线用户
        json_rest["optype"]="chat";
        json_rest["result"]=true;
        json_rest["reason"]="chat message has been sent successfully";
        json_rest["userid"]=cur_user_id;
        json_rest["roomid"]=_room_id;
        json_rest["message"]=filtered_msg;
        return json_rest;
    }
    //处理退出任务
    bool handler_exit(int userid)
    {
        Json::Value json_rest;
        if(_status==GAME_START)
        {
            int win_id=(userid==_white_id?_black_id:_white_id);
            int lose_id=(userid==_white_id?_white_id:_black_id);
            _userdb->win(win_id);
            _userdb->lose(lose_id);
            json_rest["optype"]="exit";
            json_rest["result"]=true;
            json_rest["reason"]="user not in room";
            json_rest["userid"]=userid;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=-1;
            json_rest["col"]=-1;
            json_rest["winner"]=win_id;
        }
        else
        {
            json_rest["optype"]="exit";
            json_rest["result"]=true;
            json_rest["reason"]="user has been exit successfully";
            json_rest["userid"]=userid;
            json_rest["roomid"]=_room_id;
            json_rest["row"]=-1;
            json_rest["col"]=-1;
            json_rest["winner"]=0;
        }
        _player_count--;
        handler_broadcast(json_rest);//将消息广播给所有在线用户
        return true;
    }
    //处理下棋超时任务
    Json::Value handler_time_out(Json::Value& time_out_val)
    {
        Json::Value json_rest;
        int room_id=time_out_val["room_id"].asInt();
        int cur_user_id=time_out_val["userid"].asInt();
        std::cout<<"handler_time_out called win"<<std::endl;
        // 标记房间已结束并更新胜负记录
        _status = GAME_OVER;
        _userdb->win((cur_user_id==_white_id?_black_id:_white_id));
        _userdb->lose(cur_user_id);
        json_rest["optype"]="time_out";
        json_rest["result"]=true;
        json_rest["reason"]="user has been time out,winner determined";
        json_rest["userid"]=cur_user_id;
        json_rest["roomid"]=_room_id;
        json_rest["winner"]=(cur_user_id==_white_id?_black_id:_white_id);
        return json_rest;
    }
    //处理总的请求任务
    bool handler_request(Json::Value& req_val)
    {
        //先来检查房间号是否正确
        Json::Value json_rest;
        int room_id=req_val["room_id"].asInt();
        if(_room_id!=room_id)
        {
            json_rest["optype"]=req_val["optype"].asString();
            json_rest["result"]=false;
            json_rest["reason"]="room id match failed";
            handler_broadcast(json_rest);//将消息广播给所有在线用户
            return true;
        }

        //2.检查操作类型
        std::string optype=req_val["optype"].asString();
        if(optype=="put_chess")
        {
            //处理下棋任务
            json_rest=handler_chess(req_val);
        }
        else if(optype=="chat")
        {
            //处理聊天任务
            json_rest=handler_chat(req_val);
        }
        else if(optype=="time_out")
        {
            json_rest=handler_time_out(req_val);
        }
        else
        {
            //其他操作类型
            json_rest["optype"]=req_val["optype"].asString();
            json_rest["result"]=false;
            json_rest["reason"]="optype not support";
        }
        //退出动作单独处理，不放在此函数中
        handler_broadcast(json_rest);//将消息广播给所有在线用户
        return true;
    }
    //将内容广播给在线用户
    bool handler_broadcast(Json::Value& broad_val)
    {
        //将消息广播给所有在线用户
        //1.先将信息转为字符串
        std::string str;
        if(jsontool::writer(broad_val,str)==false)
        {
            LOG(ERR,"handler_boradcast failed");
            return false;
        }
        //2.将字符串发送给两玩家
        wsserver_t::connection_ptr white_conn=_onlinemgr->get_conn_from_room(_white_id);
        wsserver_t::connection_ptr black_conn=_onlinemgr->get_conn_from_room(_black_id);
        if(white_conn==nullptr&&black_conn==nullptr)
        {
            LOG(ERR,"handler_boradcast failed,all of users are not in room");
            return false;
        }
        if(white_conn) white_conn->send(str);
        if(black_conn) black_conn->send(str);
        return true;
    }
private:
    int _room_id;//房间ID
    room_status _status;//房间状态  
    int _player_count;//在线玩家数量
    int _white_id;//白棋玩家ID
    int _black_id;//黑棋玩家ID
    user_table* _userdb;//用户mysql句柄
    online_manager* _onlinemgr;//用户连接句柄
    std::vector<std::vector<int>> _board;//棋盘状态，0为空，1
};

// 房间管理类，房间管理：
// 1.创建房间，（两个玩家都在线，匹配完成，创建一个新房间，传入玩家ID）
// 2.查找房间，（根据用户id可以查找到房间，根据房间id可以查找到房间）
// 3.销毁房间，（采用智能指针进行资源释放，当所有用户都退出了，就可以进行房间的销毁）
// 需要管理的数据有：
// 1.数据管理模块句柄；2.在线用户管理模块句柄；3.房间id分配计数器；4.互斥锁
// 5.两个哈希表，第一个对应用户与房间id的关系，第二个对应房间id与房间指针的关系
class room_manager
{
    using room_ptr=std::shared_ptr<room>;
    //typedef std::shared_ptr<room> room_ptr;
public:
    room_manager(user_table* userdb,online_manager* onlinemgr)
        :_userdb(userdb),_onlinemgr(onlinemgr),_total_room(0)
    {
        LOG(DEB,"room_manager init");
    }
    ~room_manager()
    {
        LOG(DEB,"room_manager destroy");
    }
    //创建房间
    room_ptr create_room(int white_id,int black_id)
    {
        //1.检查用户是否在游戏大厅中，不在则返回错误
        if(_onlinemgr->is_in_game_hall(white_id)==false ||
           _onlinemgr->is_in_game_hall(black_id)==false )
        {
            LOG(ERR,"create_room failed,user not in hall");
            return room_ptr();
        }
        //2.创建房间，将房间id与用户id进行关联
        room_ptr rptr=nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_t);//加锁，防止多线程同时创建房间
            int roomid=(++_total_room);
            rptr=std::make_shared<room>(roomid,_userdb,_onlinemgr);
            rptr->Set_Black_Player(black_id);
            rptr->Set_White_Player(white_id);
            _userid_to_roomid[white_id]=roomid;
            _userid_to_roomid[black_id]=roomid;
            _roomid_to_room[roomid]=rptr;
            LOG(DEB,"create_room success,roomid=%d",roomid);
        }
        return rptr;
    }
    //通过用户id获取房间指针
    room_ptr get_rptr_by_uid(int userid)
    {
        room_ptr rptr=nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            auto it=_userid_to_roomid.find(userid);
            if(it==_userid_to_roomid.end())
            {
                LOG(ERR,"get_rptr_by_uid failed,user not in room");
                return rptr;
            }
            rptr=_roomid_to_room[it->second];
        }
        return rptr;
    }
    //通过房间id获取房间指针
    room_ptr get_rptr_by_rid(int roomid)
    {
        room_ptr rptr=nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_t);
            auto it=_roomid_to_room.find(roomid);
            if(it==_roomid_to_room.end())
            {
                LOG(ERR,"get_rptr_by_rid failed,room not exist");
                return rptr;
            }
            rptr=it->second;
        }
        return rptr;
    }
    //销毁房间
    bool destroy_room(int roomid)//该函数无锁，慎重调用！
    {
        auto it=_roomid_to_room.find(roomid);
        if(it==_roomid_to_room.end())
        {
            LOG(ERR,"destroy_room failed,room not exist");
            return false;
        }
        _roomid_to_room.erase(it);
        LOG(DEB,"destroy_room success,roomid=%d",roomid);
        return true;
    }

    //删除房间用户，如果在线用户为0，销毁房间
    bool del_user_in_room(int userid)
    {
        std::unique_lock<std::mutex> lock(mutex_t);
        auto it=_userid_to_roomid.find(userid);
        if(it==_userid_to_roomid.end())
        {
            LOG(ERR,"del_user_in_room failed,user not in room");
            return false;
        }
        int roomid=it->second;

        room_ptr rptr=nullptr;
        auto fit=_roomid_to_room.find(roomid);
        if(fit==_roomid_to_room.end())
        {
            LOG(ERR,"get_rptr_by_rid failed,room not exist");
            return false;
        }
        rptr=fit->second;
        if(rptr==nullptr)
        {
            LOG(ERR,"del_user_in_room failed,room not exist");
            return false;
        }
        //处理用户退出房间,更新房间人数，并判断赢家
        if((rptr->Get_Player_Count()==2))
        {
            rptr->handler_exit(userid);
            return true;
        }
        if((rptr->Get_Player_Count())==1)
        {
            _userid_to_roomid.erase(rptr->Get_White_ID());
            _userid_to_roomid.erase(rptr->Get_Black_ID());
            destroy_room(roomid);
        }
        LOG(DEB,"del_user_in_room success,userid=%d",userid);
        return true;
    }
private:
    int _total_room;//房间id计数器
    std::mutex mutex_t;//互斥锁
    user_table* _userdb;//用户mysql句柄
    online_manager* _onlinemgr;//用户连接句柄
    std::unordered_map<int,int> _userid_to_roomid;//<用户id,房间id>
    std::unordered_map<int,room_ptr> _roomid_to_room;//<房间id,房间指针>
};
