#pragma once
#include "log.hpp"
#include <errno.h>
#include <cstring>
#include <iostream>
#include <fstream>
class filetool;
class filetool
{
public:
    static bool read(const std::string& filename,std::string& body,websocketpp::http::status_code::value& code)
    {
        code=websocketpp::http::status_code::ok;
        std::ifstream ifs(filename,std::ios::binary|std::ios::in);
        if(!ifs.is_open())
        {
            code=websocketpp::http::status_code::not_found;
            std::ifstream errorifs("../wwwroot/404.html",std::ios::binary|std::ios::in);
            if(!errorifs.is_open())
            {
                LOG(ERR,"file opened error: %s",strerror(errno));
                return false;
            }
            errorifs.seekg(0,std::ios::end);
            size_t fsize=errorifs.tellg();
            errorifs.seekg(0,std::ios::beg);

            body.resize(fsize);//预留空间
            errorifs.read(&body[0],fsize);//取首地址写入
            if(errorifs.good()==false)
            {
                LOG(WAR,"file read failed");
                errorifs.close();
                return false;
            }
            errorifs.close();
            return true;
        }
        else
        {
            ifs.seekg(0,std::ios::end);
            size_t fsize=ifs.tellg();
            ifs.seekg(0,std::ios::beg);

            body.resize(fsize);//预留空间
            ifs.read(&body[0],fsize);//取首地址写入

            if(ifs.good()==false)
            {
                LOG(WAR,"file read failed");
                ifs.close();
                return false;
            }
            ifs.close();
            return true;
        }
    }

    static bool write(const std::string& filename,const std::string& body)
    {
        std::ofstream ofs(filename,std::ios::binary|std::ios::out);
        if(!ofs.is_open())
        {
            LOG(ERR,"file opened error");
            return false;
        }
        size_t ssize=body.size();
        ofs.write(body.c_str(),ssize);

        if(ofs.good()==false)
        {
            LOG(WAR,"file write failed");
            ofs.close();
            return false;
        }
        return true;
    }
};