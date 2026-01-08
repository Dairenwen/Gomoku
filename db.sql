create database if not exists online_gobang;
use online_gobang;
create table if not exists users(
    id int primary key auto_increment comment "玩家序号",
    username varchar(32) unique not null comment "玩家名称",
    password varchar(128) not null comment "玩家密码",
    score int comment "玩家分数",
    total_count int comment "总场次",
    win_count int comment "赢场次"
)charset ="utf8" collate ="utf8_general_ci" engine ="InnoDB";

desc users;

