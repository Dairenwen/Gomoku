#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include "log.hpp"
#include <jsoncpp/json/json.h>
class jsontool;
class jsontool
{
public:
    jsontool()
    {}

    static bool writer(Json::Value& root,std::string& str)
    {
        std::stringstream ss;
        Json::StreamWriterBuilder swb;
        std::unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());
        if(writer->write(root,&ss)!=0)
        {
            LOG(ERR,"json write wrong");
            return false;
        }
        str=ss.str();
        return true;
    }

    static bool reader(Json::Value& root,const std::string& str)
    {
        std::string err;
        Json::CharReaderBuilder crb;
        std::unique_ptr<Json::CharReader> reader(crb.newCharReader());
        //parse(字符串起始地址，结束地址，要写入的对象地址，错误信息存放)
        if(reader->parse(str.c_str(),str.c_str()+str.size(),&root,&err)==false)
        {
            LOG(ERR,"json parse wrong : %s",err.c_str());
            return false;
        }
        return true;
    }

    ~jsontool()
    {}
};


void testjsoncpp()
{
    //序列化
    //将要存储的数据放在root中
    Json::Value root;
    std::stringstream ss;
    root["name"]="Bob";
    root["age"]=18;
    root["is_student"]=true;
    root["score"].append(66.66);
    root["score"].append(77.77);
    root["score"].append(88.88);

    //实例化工厂类对象，将root数据写入
    Json::StreamWriterBuilder swb;
    std::unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());
    if(writer->write(root,&ss)!=0)
    {
        std::cerr<<"json wrong"<<std::endl;
        return;
    }
    std::cout<<ss.str()<<std::endl;

    //反序列化
    //将要得到的数据放在value中
    Json::Value value;
    std::string str=ss.str(),err;
    //实例化工厂类对象，将str的信息导入到value中
    Json::CharReaderBuilder crb;
    std::unique_ptr<Json::CharReader> reader(crb.newCharReader());
    //parse(字符串起始地址，结束地址，要写入的对象地址，错误信息存放)
    if(reader->parse(str.c_str(),str.c_str()+str.size(),&value,&err)==false)
    {
        std::cerr<<"parse wrong"<<std::endl;
        return;
    }
    std::cout<<value["name"].asString()<<std::endl;
    std::cout<<value["age"].asInt()<<std::endl;
    std::cout<<value["is_student"].asBool()<<std::endl;
    for(int i=0;i<value["score"].size();i++) std::cout<<value["score"][i].asDouble()<<" ";
}
