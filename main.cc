#include "log.hpp"
#include "db.hpp"
#include "room.hpp"
#include "wsserver.hpp"
#include "jsontool.hpp"
#include "filetool.hpp"
#include "mysqltool.hpp"
#include "stringtool.hpp"
#include "session.hpp"
#include "match.hpp"
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
using namespace std;
const string rootpath="/home/drw/gomoku/";

using wsserver_t=websocketpp::server<websocketpp::config::asio>;
using room_ptr=std::shared_ptr<room>;

void test_room_manager()
{
    online_manager* onlinemgr=new online_manager();
    wsserver_t::connection_ptr conn=nullptr;
    user_table* userdb=new user_table("124.222.53.34","drw","dairenwen1092","online_gobang");
    room_manager rm(userdb,onlinemgr);
    onlinemgr->enter_game_hall(1,conn);
    onlinemgr->enter_game_hall(2,conn);
    room_ptr rptr=rm.create_room(1,2);
    if(rptr!=nullptr) cout<<"create room success"<<endl;
    if(rptr->Get_room_id()==1) cout<<"room id is 1"<<endl;
    if(rptr->Get_Player_Count()==2) cout<<"player count is 2"<<endl;
    if(rptr->Get_White_ID()==1) cout<<"white id is 1"<<endl;
    if(rptr->Get_Black_ID()==2) cout<<"black id is 2"<<endl;
}

void test_user_table()
{
    user_table user("124.222.53.34","drw","dairenwen1092","online_gobang");
    Json::Value userjson;
    userjson["username"]="testuser";
    userjson["password"]="123456";
    user.select_by_name("testuser",userjson);
    string body;
    jsontool::writer(userjson,body);
    cout<<"userjson: "<<body<<endl;
    user.select_by_id(2,userjson); 
    Test_WSServer wssr;
    wssr.Init();
}

void test_session_manager()
{
    wsserver_t ws;
    session_manager sm(&ws);
    //sm.create_session(1,);
    sm.set_timer(1,1000);
}

void test_match_queue()
{
    match_queue<int> mq;
    mq.push(1);
    mq.push(2);
    mq.push(3);
    int data;
    mq.pop(data);
    cout<<"data: "<<data<<endl;
    cout<<"mq size: "<<mq.size()<<endl;
    mq.pop(data);
    cout<<"data: "<<data<<endl;
    cout<<"mq size: "<<mq.size()<<endl;
    mq.pop(data);
    cout<<"data: "<<data<<endl;
    cout<<"mq size: "<<mq.size()<<endl;
}

void test_match_manager()
{
    online_manager* onlinemgr=new online_manager();
    wsserver_t::connection_ptr conn=nullptr;
    user_table* userdb=new user_table("124.222.53.34","drw","dairenwen1092","online_gobang");
    room_manager* rm=new room_manager(userdb,onlinemgr);
    match_manager mm(rm,onlinemgr,userdb);
}

void test_gobang_server()
{
    gobang_server wssr("124.222.53.34","drw","dairenwen1092","online_gobang");
    wssr.run(8080);
}
int main()
{
    // WSServer wssr;
    // wssr.Init();
    test_gobang_server();
    return 0;
}