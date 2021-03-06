#include "wrap.h"
#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <mysql/mysql.h>
#include <sys/epoll.h>
#include <error.h>
#include <unistd.h>
#include <sys/stat.h>


#define SERV_PORT 8000
#define MAX_EVENTS 1000

#define OFFLINE 0
#define ONLINE 1
#define ONE_CHAT 2
#define MANY_CHAT 3

#define FRIEND 1
#define FRI_BLK 2
#define GRP 3
#define GRP_OWN 4
#define GRP_ADM 5

#define EXIT -1
#define REGISTE 1
#define LOGIN 2
#define CHECK_FRI 3
#define GET_FRI_STA 4
#define ADD_FRI 5
#define DEL_FRI 6
#define SHI_FRI 7
#define CRE_GRP 8
#define ADD_GRP 9
#define OUT_GRP 10
#define DEL_GRP 11
#define SET_GRP_ADM 12
#define KICK_GRP 13
#define CHECK_GRP 14
#define CHECK_MEM_GRP 15
#define CHAT_ONE 16
#define CHAT_MANY 17
#define CHECK_MES_FRI 18
#define CHECK_MES_GRP 19
#define RECV_FILE 20
#define SEND_FILE 21


User *U_read();                     //读取用户信息表
Relation *R_read();                 //读取关系表
Recordinfo *RC_read();              //读取消息记录
void Insert(User *pNew);            //注册
void Insert_R(Relation *pNew);      
void Insert_RC(Recordinfo *pNew);   
void Delete_R(Relation *pNew);      
void DeleteLink();                  	
void DeleteLink_R();                	
void DeleteLink_RC();               	
void *Menu(void *recv_pack_t);      
void Exit(PACK *recv_pack);        
void registe(PACK *recv_pack);      //注册
void login(PACK *recv_pack);        //登陆
void check_fri(PACK *recv_pack);    //查看好友列表
void get_fri_sta(PACK *recv_pack);  //获取好友状态
void add_fri(PACK *recv_pack);      //添加好友
void del_fri(PACK *recv_pack);      //删除好友
void shi_fri(PACK *recv_pack);      //屏蔽好友
void cre_grp(PACK *recv_pack);      
void add_grp(PACK *recv_pack);      
void out_grp(PACK *recv_pack);      
void del_grp(PACK *recv_pack);      
void set_grp_adm(PACK *recv_pack);  //设置管理员
void kick_grp(PACK *recv_pack);     //踢人
void check_grp(PACK *recv_pack);    //查看群列表       
void check_mem_grp(PACK *recv_pack);//查看群中成员
void chat_one(PACK *recv_pack);     //私聊
void chat_many(PACK *recv_pack);    //群聊
void recv_file(PACK *recv_pack);    //接收文件
void send_file(PACK *recv_pack);    //发送文件
void check_mes_fri(PACK *recv_pack);//查看与好友聊天记录
void check_mes_grp(PACK *recv_pack);//查看群组聊天记录
void send_more(int fd, int flag, PACK *recv_pack, char *mes);
void send_pack(int fd, PACK *recv_pack, char *ch);

MYSQL mysql;
User *pHead = NULL;
Relation *pStart = NULL;
Recordinfo *pRec = NULL;

PACK Mex_Box[100];
int sign;
int count;


int main(void) {

    struct sockaddr_in serveraddr,clientaddr;
    int sockfd,addrlen,confd;
    char ipstr[128];
    int epfd;
    struct epoll_event ev,events[MAX_EVENTS];
    int nready;
    int i=0;
    int ret;
    PACK recv_t;
    PACK *recv_pack;
    pthread_t pid;

    addrlen = sizeof(clientaddr);

    if(mysql_init(&mysql)==NULL){
        my_err("mysql_init",__LINE__);
        return -1;
    }

     if(mysql_real_connect(&mysql,"127.0.0.1","root","123456","chatroom",0,NULL,0)==NULL){
        my_err("mysql_real_connect",__LINE__);
        return -1;
    }
    printf("Loading...\n");

    sockfd = Socket(AF_INET,SOCK_STREAM,0);

    bzero(&serveraddr,sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERV_PORT);
    Bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));

    Listen(sockfd,128);

    epfd=epoll_create(MAX_EVENTS);
    ev.data.fd=sockfd;
    ev.events=EPOLLIN ;
    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);
    sleep(1);
    printf("服务器启动成功！\n");

    //读数据
    pHead=U_read();
    pStart=R_read();
    pRec=RC_read();
    User *t=pHead;


    while(1){
        nready = epoll_wait(epfd, events, MAX_EVENTS, 1000);    //等待事件到来
        for(i = 0; i < nready; i++){
            if(events[i].data.fd == sockfd){
                confd = Accept(sockfd, (struct sockaddr *)&clientaddr, &addrlen);
                printf("Connected client IP: %s, fd: %d\n",inet_ntoa(clientaddr.sin_addr), confd);
                ev.data.fd = confd;               //设置与要处理事件相关的文件描述符
                ev.events = EPOLLIN ;                //设置要处理的事件类型
                epoll_ctl(epfd, EPOLL_CTL_ADD, confd, &ev);   //注册epoll事件
                continue;
            }
            else if(events[i].events & EPOLLIN){      
                memset(&recv_t, 0, sizeof(PACK));
                ret=recv(events[i].data.fd, &recv_t, sizeof(PACK), MSG_WAITALL);
                recv_t.data.send_fd=events[i].data.fd;
 			    if (ret < 0) {
 				    Close(events[i].data.fd);
 				    perror("recv");
 				    continue;
 			    }
                else if(ret==0) {
                    ev.data.fd = events[i].data.fd;
                    while(t){
                        if(t->fd == ev.data.fd){
                            t->statu_s = OFFLINE;
                            break;
                        }
                        t = t->next;
                    }
                    printf("OFFLINE(fd): %d\n",ev.data.fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                    Close(events[i].data.fd);
                    continue;
                }
                printf("------------------\n");
                printf("type      : %d\n", recv_t.type);
                printf("send_name : %s\n", recv_t.data.send_name);
                printf("recv_name : %s\n",recv_t.data.recv_name);
                printf("mes       : %s\n", recv_t.data.mes);
                printf("------------------\n\n");

                recv_t.data.recv_fd = events[i].data.fd;
 			    recv_pack = (PACK*)malloc(sizeof(PACK));
 			    memcpy(recv_pack, &recv_t, sizeof(PACK));
                Menu((void*)recv_pack);
            }
        }
    }

    Close(epfd);
    Close(sockfd);
    free(recv_pack);
    DeleteLink();
    DeleteLink_R();
    DeleteLink_RC();

    return 0;
}


//读取用户信息表
User *U_read()
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query[1000];

    User *pEnd, *pNew;

    sprintf(query, "select * from user_data");
    mysql_real_query(&mysql, query, strlen(query));
    res = mysql_store_result(&mysql);

    while(row = mysql_fetch_row(res)){
        pNew = (User *)malloc(sizeof(User));
        strcpy(pNew->name, row[0]);
        strcpy(pNew->passwd, row[1]);
        pNew->statu_s = OFFLINE;
        pNew->next = NULL;
        if(pHead == NULL)
            pHead = pNew;
        else
            pEnd->next = pNew;
        pEnd = pNew;
    }
    return pHead;
}

//读取关系表
Relation *R_read()
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query[1000];

    Relation *pEnd, *pNew;

    sprintf(query, "select * from friends");
    mysql_real_query(&mysql, query, strlen(query));
    res = mysql_store_result(&mysql);

    while(row = mysql_fetch_row(res)){
        pNew = (Relation *)malloc(sizeof(Relation));
        strcpy(pNew->name1, row[0]);
        strcpy(pNew->name2, row[1]);
        pNew->statu_s = row[2][0] - '0';
        pNew->next = NULL;
        if(pStart == NULL)
            pStart = pNew;
        else
            pEnd->next = pNew;
        pEnd = pNew;
    }
    return pStart;
}

//读取消息记录
Recordinfo *RC_read()
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query[1000];
    Recordinfo *pEnd, *pNew;

    sprintf(query, "select * from records");
    mysql_real_query(&mysql, query, strlen(query));
    res = mysql_store_result(&mysql);

    while(row = mysql_fetch_row(res)){
        pNew = (Recordinfo *)malloc(sizeof(Recordinfo));
        strcpy(pNew->name1, row[0]);
        strcpy(pNew->name2, row[1]);
        strcpy(pNew->message, row[2]);
        pNew->next = NULL;
        if(pRec == NULL)
            pRec = pNew;
        else
            pEnd->next = pNew;
        pEnd = pNew;
    }
    return pRec;
}

//处理函数
void *Menu(void *recv_pack_t)
{
    PACK *recv_pack;
    recv_pack = (PACK *)recv_pack_t;
    switch(recv_pack->type)
    {
    case EXIT:
        Exit(recv_pack);
        break;
    case REGISTE:
        registe(recv_pack);            
        break;
    case LOGIN:
        login(recv_pack);
        break;
    case CHECK_FRI:
        check_fri(recv_pack);
        break;
    case GET_FRI_STA:
        get_fri_sta(recv_pack);
        break;
    case ADD_FRI:
        add_fri(recv_pack);
        break;
    case DEL_FRI:
        del_fri(recv_pack);
        break;
    case SHI_FRI:
        shi_fri(recv_pack);
        break;
    case CRE_GRP:
        cre_grp(recv_pack);
        break;
    case ADD_GRP:
        add_grp(recv_pack);
        break;
    case OUT_GRP:
        out_grp(recv_pack);
        break;
    case DEL_GRP:
        del_grp(recv_pack);
        break;
    case SET_GRP_ADM:
        set_grp_adm(recv_pack);
        break;
    case KICK_GRP:
        kick_grp(recv_pack);
        break;
    case CHECK_GRP:
        check_grp(recv_pack);
        break;
    case CHECK_MEM_GRP:
        check_mem_grp(recv_pack);
        break;
    case CHAT_ONE:
        chat_one(recv_pack);
        break;
    case CHAT_MANY:
        chat_many(recv_pack);
        break;
    case CHECK_MES_FRI:
        check_mes_fri(recv_pack);
        break;
    case CHECK_MES_GRP:
        check_mes_grp(recv_pack);
        break;
    case RECV_FILE:
        recv_file(recv_pack);
        break;
    case SEND_FILE:
        send_file(recv_pack);
        break;
    default:
        break;
    }
}

//退出
void Exit(PACK *recv_pack)
{
    User *t = pHead;
    while(t){
        if(strcmp(t->name, recv_pack->data.send_name) == 0){
            t->statu_s = OFFLINE;
            break;
        }
        t = t->next;
    }
    Close(recv_pack->data.send_fd);
}

//注册
void registe(PACK *recv_pack)
{
    char query[1000];
    int a;
    char ch[5];
    int fd;
    fd = recv_pack->data.send_fd;

    User *t = pHead;
    int flag = 0;
    User *pNew = (User *)malloc(sizeof(User));
    while(t){
        if(strcmp(t->name, recv_pack->data.send_name) == 0){
            flag = 1;
            break;
        }
        t = t->next;
    }

    //添加到数据库中并发送信息给客户端
    if(flag == 0){
        strcpy(pNew->name, recv_pack->data.send_name);
        strcpy(pNew->passwd, recv_pack->data.mes);
        pNew->statu_s = OFFLINE;
        Insert(pNew);
        memset(query, 0, strlen(query));
        sprintf(query, "insert into user_data values('%s', '%s')", recv_pack->data.send_name, recv_pack->data.mes);
        mysql_real_query(&mysql, query, strlen(query));
        ch[0] = '1';
    }
    else//重名
        ch[0] = '0';
    
    ch[1] = '\0';
    send_pack(fd, recv_pack, ch);
}

//注册——加入链表
void Insert(User *pNew)
{
    User *t = pHead;
    if(t==NULL){
        t=pNew;
        pNew->next = NULL;
    }
    else{
        while(t && t->next != NULL)
            t = t->next;
        t->next = pNew;
        pNew->next = NULL;
    }
}

//登陆
void login(PACK *recv_pack)
{
    char ch[5];
    int fd = recv_pack->data.send_fd;
    int i;

    User *t = pHead;
    int flag = 0;
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.send_name) == 0 && strcmp(t->passwd, recv_pack->data.mes) == 0){
            flag = 1;
            break;
        }
        t = t->next;
    }

    if(flag == 0)//不存在
        ch[0] = '0';
    else{
        if(t->statu_s == OFFLINE){
            ch[0] = '1';
            t->statu_s = ONLINE;
            t->fd = recv_pack->data.send_fd;
        }
        else //在线
            ch[0] = '2';
    }
    ch[1] = '\0';
    send_pack(fd, recv_pack, ch);
    
    for(i = 0; i < sign; i++)
    {
        //私聊
        if((ch[0] == '1') && strcmp(recv_pack->data.send_name, Mex_Box[i].data.recv_name) == 0 && (Mex_Box[i].type == CHAT_ONE)){
            send_more(fd, CHAT_ONE, &Mex_Box[i], "1");
            count++;
        }
        //群聊
        if((ch[0] == '1') && strcmp(recv_pack->data.send_name, Mex_Box[i].data.send_name) == 0 && (Mex_Box[i].type == CHAT_MANY)){
            send_more(fd, CHAT_MANY, &Mex_Box[i], "2");
            count++;
        }
        //加好友
        if((ch[0] == '1') && strcmp(recv_pack->data.send_name, Mex_Box[i].data.recv_name) == 0 && (Mex_Box[i].type == ADD_FRI)){
            Menu((void *)&Mex_Box[i]);
            count++;
        }
        //加群
        if((ch[0] == '1') && strcmp(recv_pack->data.send_name, Mex_Box[i].data.recv_name) == 0 && (Mex_Box[i].type == ADD_GRP)){
            Menu((void *)&Mex_Box[i]);  
            count++;
        }
        //设置管理员/踢人
        if((ch[0] == '1') && strcmp(recv_pack->data.send_name, Mex_Box[i].data.mes) == 0){
            send_more(fd, Mex_Box[i].type, &Mex_Box[i], "6");
            count++;
        }
        //发文件
        if((ch[0] == '1') && strcmp(recv_pack->data.send_name, Mex_Box[i].data.recv_name) == 0 && strcmp(Mex_Box[i].data.mes, "success") == 0)
        {
            send_file(&Mex_Box[i]);
            count++;
        }
    }
    if(count == sign)
        sign = count = 0;
}

//查看好友列表
void check_fri(PACK *recv_pack)
{
    int flag = CHECK_FRI;
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query[700];
    int rows;
    int i;

    int fd = recv_pack->data.send_fd;
    int statu_s;

    memset(query, 0, strlen(query));
    sprintf(query, "select * from friends where name1='%s' or name2='%s'", recv_pack->data.send_name, recv_pack->data.send_name);
    mysql_real_query(&mysql, query, strlen(query));
    
    res = mysql_store_result(&mysql);
    
    rows = mysql_num_rows(res); //行数

    if(rows == 0)
        recv_pack->fri_info.friends_num = 0;
    else{
        i = 0;
        while(row = mysql_fetch_row(res)){
            if(strcmp(row[0], recv_pack->data.send_name) == 0){
                strcpy(recv_pack->fri_info.friends[i], row[1]);
                statu_s = row[2][0] - '0';
                recv_pack->fri_info.friends_status[i] = statu_s;
                i++;
            }
            else if(strcmp(row[1], recv_pack->data.send_name) == 0){
                strcpy(recv_pack->fri_info.friends[i], row[0]);
                statu_s = row[2][0] - '0';
                recv_pack->fri_info.friends_status[i] = statu_s;
                i++;
            }   
        }
        recv_pack->fri_info.friends_num = i;
    }
    send_more(fd, flag, recv_pack, "");
}

//获取好友状态
void get_fri_sta(PACK *recv_pack)
{
    int flag = GET_FRI_STA;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    User *t = pHead;
    int flag2 = 0;
    while(t){
        if(strcmp(t->name, recv_pack->data.send_name) == 0){
            flag2 = 1;
            break;
        }
        t = t->next;
    }

    if(t->statu_s == OFFLINE)
        ch[0] = '0';
    else 
        ch[0] = '1';
    ch[1] = '\0';

    send_more(fd, flag, recv_pack, ch);
}

//添加好友
void add_fri(PACK *recv_pack)
{
    char query[1700];
    int flag = ADD_FRI;
    int fd = recv_pack->data.send_fd;
    char ch[5];
    char temp[MAX_CHAR];

    User *t = pHead;
    int flag2 = 0;
    Relation *q = pStart;
    int flag3 = 0;
    Relation *pNew = (Relation *)malloc(sizeof(Relation));
    while(q){
        if((strcmp(q->name1, recv_pack->data.recv_name) == 0 && strcmp(q->name2, recv_pack->data.send_name) == 0) || (strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0)){
            flag3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag3 == 1){//已经是好友
        ch[0] = '4';
        send_more(fd, flag, recv_pack, ch);
        free(pNew);
        pNew = NULL;
        return;
    }
    else{
        while(t){
            if(strcmp(t->name, recv_pack->data.recv_name) == 0){
                flag2 = 1;
                break;
            }
            t = t->next;
        }
        
        //该用户不存在
        if(flag2 == 0){
            ch[0] = '3';
            send_more(fd, flag, recv_pack, ch);
            free(pNew);
            pNew = NULL;
            return;
        }
        else{
            if(t->statu_s != OFFLINE){
                fd = t->fd;
                if(recv_pack->data.mes[0] == '0')
                    ch[0] = '0';
                else if(recv_pack->data.mes[0] == 'y'){
                    ch[0] = '1';
                    strcpy(pNew->name1, recv_pack->data.recv_name);
                    strcpy(pNew->name2, recv_pack->data.send_name);
                    pNew->statu_s = FRIEND;
                    printf("add----\n");
                    Insert_R(pNew);

                    memset(query, 0, strlen(query));
                    sprintf(query, "insert into friends values('%s', '%s', %d)", recv_pack->data.recv_name, recv_pack->data.send_name, FRIEND);
                    mysql_real_query(&mysql, query, strlen(query));
                }
                else if(recv_pack->data.mes[0] == 'n')
                    ch[0] = '2';
                
                strcpy(temp,recv_pack->data.recv_name);
                strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                strcpy(recv_pack->data.send_name, temp);
                send_more(fd, flag, recv_pack, ch);
            }
            else if(t->statu_s == OFFLINE)
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));       //登陆

        }
    }
}

//加入关系表
void Insert_R(Relation *pNew)
{
    Relation *t = pStart;
    if(t==NULL){
        t=pNew;
        pNew->next = NULL;
    }
    else{
        while(t && t->next != NULL)
            t = t->next;
        t->next = pNew;
        pNew->next = NULL;
    }
}

//删除好友
void del_fri(PACK *recv_pack)
{
    char query[1700];
    int flag = DEL_FRI;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag3 = 0;
    while(q){
        if((strcmp(q->name1, recv_pack->data.mes) == 0 && strcmp(q->name2, recv_pack->data.send_name) == 0) || (strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0)){
            flag3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag3 == 0)//不是好友
        ch[0] = '0';
    else{
        Delete_R(q);

        memset(query, 0, strlen(query));
        sprintf(query, "delete from friends where (name1='%s' and name2='%s') or (name1='%s' and name2='%s')", recv_pack->data.send_name, recv_pack->data.mes, recv_pack->data.mes, recv_pack->data.send_name);
        mysql_real_query(&mysql, query, strlen(query));
        ch[0] = '1';
    }
    send_more(fd, flag, recv_pack, ch);
}

//删除出关系表
void Delete_R(Relation *pNew)
{
    Relation *t = pStart;
    Relation *ptr;
    while(t){
        if((strcmp(t->name1, pNew->name1) == 0 && strcmp(t->name2, pNew->name2) == 0) || (strcmp(t->name1, pNew->name2) == 0 && strcmp(t->name2, pNew->name2) == 0)){
            if(pStart == t){
                pStart = t->next;
                free(t);
                return;
            }
            ptr->next = t->next;
            free(t);
            return;
        }
        ptr = t;
        t = t->next;
    }
}

//屏蔽好友
void shi_fri(PACK *recv_pack)
{
    char query[1700];
    int flag = SHI_FRI;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag3 = 0;
    while(q){
        if((strcmp(q->name1, recv_pack->data.mes) == 0 && strcmp(q->name2, recv_pack->data.send_name) == 0) || (strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0)){
            flag3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag3 == 0)
        ch[0] = '0';
    else{
        q->statu_s = FRI_BLK;
        memset(query, 0, strlen(query));
        sprintf(query, "update friends set status=%d where (name1='%s' and name2='%s') or (name1='%s' and name2='%s')", FRI_BLK, recv_pack->data.send_name, recv_pack->data.mes, recv_pack->data.mes, recv_pack->data.send_name);
        mysql_real_query(&mysql, query, strlen(query));
        ch[0] = '1';
    }
    send_more(fd, flag, recv_pack, ch);

}

//创建群
void cre_grp(PACK *recv_pack)
{
    char query[1000];

    int flag = CRE_GRP;
    int fd = recv_pack->data.send_fd;
    char ch[5];

    Relation *q = pStart;
    int flag3 = 0;
    Relation *pNew = (Relation *)malloc(sizeof(Relation));
    while(q){
        if(strcmp(q->name2, recv_pack->data.mes) == 0){
            flag3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag3 == 1)
        ch[0] = '0';
    else{
        ch[0] = '1';
        strcpy(pNew->name1, recv_pack->data.send_name);
        strcpy(pNew->name2, recv_pack->data.mes);
        pNew->statu_s = GRP_OWN;
        Insert_R(pNew);
        
        memset(query, 0, strlen(query));
        sprintf(query, "insert into friends values('%s', '%s', %d)", recv_pack->data.send_name, recv_pack->data.mes, GRP_OWN);
        mysql_real_query(&mysql, query, strlen(query));
    }
    send_more(fd, flag, recv_pack, ch);
}

//加群
void add_grp(PACK *recv_pack)
{
    char query[15000];

    int flag = ADD_GRP;
    int fd = recv_pack->data.send_fd;
    char ch[5];
    User *t = pHead;
    Relation *q = pStart;
    int flag2 = 0;
    Relation *pNew = (Relation *)malloc(sizeof(Relation));
    if(strcmp(recv_pack->data.mes, "y") == 0){   
        while(t){
            if(strcmp(t->name, recv_pack->data.recv_name) == 0){
                fd = t->fd;
                break;
            }
            t = t->next;
        }
        ch[0] = '2';
        printf("%s\n", recv_pack->file.mes);
        strcpy(pNew->name1, recv_pack->data.recv_name);
        strcpy(pNew->name2, recv_pack->data.send_name);
        pNew->statu_s = GRP;
        Insert_R(pNew);

        memset(query, 0, strlen(query));
        sprintf(query, "insert into friends values('%s', '%s', %d)", recv_pack->data.recv_name, recv_pack->data.send_name, GRP);
        mysql_real_query(&mysql, query, strlen(query));
        send_more(fd, flag, recv_pack, ch);
        return;
    }
    else if(strcmp(recv_pack->data.mes, "n") == 0){
        while(t){
            if(strcmp(t->name, recv_pack->data.recv_name) == 0){
                fd = t->fd;
                break;
            }
            t = t->next;
        }
        ch[0] = '3';
        send_more(fd, flag, recv_pack, ch);
        return;
    }
    while(q){
        if(strcmp(q->name2, recv_pack->data.mes) == 0 && (q->statu_s == GRP_OWN)){
            flag2 = 1;
            strcpy(recv_pack->data.recv_name, q->name1);
            break;
        }
        q = q->next;
    }

    if(flag2 == 0){//该群不存在
        ch[0] = '0';
        send_more(fd, flag, recv_pack, ch);
        return;
    }
    else if(flag2 == 1){
        t = pHead;
        while(t){
            if(strcmp(recv_pack->data.recv_name, t->name) == 0 && (t->statu_s != OFFLINE)){
                ch[0] = '1';
                fd = t->fd;
                strcpy(recv_pack->file.mes, recv_pack->data.mes);
                send_more(fd, flag, recv_pack, ch);
                return;
            }
            else if(strcmp(recv_pack->data.recv_name, t->name) == 0 && (t->statu_s == OFFLINE)){
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));
                break;
            } 
            t = t->next;
        }
    }
}

//退群
void out_grp(PACK *recv_pack)
{
    char query[1000];
    int flag = OUT_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    while(q){
        if(strcmp(q->name2, recv_pack->data.mes) == 0){
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 0)
        ch[0] = '0';
    else{
        ch[0] = '1';
        Delete_R(q);

        memset(query, 0, strlen(query));
        sprintf(query, "delete from friends where name1='%s' and name2='%s'", recv_pack->data.send_name, recv_pack->data.mes);
        mysql_real_query(&mysql, query, strlen(query));
    }
    send_more(fd, flag, recv_pack, ch);
}

//解散群
void del_grp(PACK *recv_pack)
{
    char query[1000];
    int flag = DEL_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag3 = 0;
    int flag2 = 0;
    while(q){
        if(strcmp(q->name2, recv_pack->data.mes) == 0){
            flag2 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q){
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0 && (q->statu_s == GRP_OWN)){
            flag3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag2 == 0)
        ch[0] = '0';
    else if(flag3 == 1 && flag2 == 1){
        ch[0] = '1';
        q = pStart;
        while(q){
            if(strcmp(q->name2, recv_pack->data.mes) == 0)
                Delete_R(q);
            q = q->next;
        }
        memset(query, 0, strlen(query));
        sprintf(query, "delete from friends where name2='%s'", recv_pack->data.mes);
        mysql_real_query(&mysql, query, strlen(query));
    }
    else if(flag3 == 0 && flag2 == 1)
        ch[0] = '2';
    send_more(fd, flag, recv_pack, ch);
}

//设置管理员
void set_grp_adm(PACK *recv_pack)
{
    char query[1000];

    int flag = SET_GRP_ADM;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    int fd2;
    User *t = pHead;
    Relation *q = pStart;
    int flag3 = 0;
    int flag2 = 0;
    int flag1 = 0;
    while(q){
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0){
            flag2 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q){
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && strcmp(q->name1, recv_pack->data.mes) == 0){
            flag1 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q){
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0 && q->statu_s == GRP_OWN){
            flag3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag3 == 1 && flag2 == 1 && flag1 == 1){
        ch[0] = '1';
        q = pStart;
        while(q){
            if(strcmp(q->name1, recv_pack->data.mes) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0){
                q->statu_s = GRP_ADM;
                break;
            }
            q = q->next;
        }
        while(t){
            if(strcmp(t->name, recv_pack->data.mes) == 0 && (t->statu_s != OFFLINE)){
                fd2 = t->fd;
                send_more(fd2, flag, recv_pack, "6");
            }
            else if(strcmp(t->name, recv_pack->data.mes) == 0 && (t->statu_s == OFFLINE))
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));
            t = t->next;
        }
        memset(query, 0, strlen(query));
        sprintf(query, "update friends set status=%d where name1='%s' and name2='%s'", GRP_ADM, recv_pack->data.mes, recv_pack->data.recv_name);
        mysql_real_query(&mysql, query, strlen(query));
    }
    else if(flag3 == 0 && flag2 == 1 && flag1 == 1)
        ch[0] = '2';
    else if(flag1 == 0)
        ch[0] = '3';
    else if(flag2 == 0)
        ch[0] = '0';
    send_more(fd, flag, recv_pack, ch);
}

//踢人
void kick_grp(PACK *recv_pack)
{
    char query[1000];

    int flag = KICK_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    int fd2;
    User *t = pHead;
    Relation *q = pStart;
    int flag3 = 0;
    int flag1 = 0;
    int flag2 = 0;
    int flag4 = 0;
    while(q){
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0){
            flag1 = 1;
            break;
        }
        q = q->next;
    }
    q = pStart;
    while(q){
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && strcmp(q->name1, recv_pack->data.mes) == 0){
            flag2 = 1;
            break;
        }
        q = q->next;
    }
    q = pStart;
    while(q){
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s == GRP_OWN || q->statu_s == GRP_ADM)){
            flag3 = 1;
            break;
        }
        q = q->next;
    }
    q = pStart;
    while(q){
        if(strcmp(q->name1, recv_pack->data.mes) == 0 && (q->statu_s == (GRP))){
            flag4 = 1;
            break;
        }
        q = q->next;
    }
    if(flag3 == 1 && flag1 == 1 && flag2 == 1 && flag4 == 1){
        ch[0] = '1';
        Delete_R(q);
        while(t){
            if(strcmp(t->name, recv_pack->data.mes) == 0 && (t->statu_s != OFFLINE)){
                fd2 = t->fd;
                send_more(fd2, flag, recv_pack, "6");
            }
            else if(strcmp(t->name, recv_pack->data.mes) == 0 && (t->statu_s == OFFLINE))
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));
            t = t->next;
        }
        memset(query, 0, strlen(query));
        sprintf(query, "delete from friends where name1='%s' and name2='%s'", recv_pack->data.mes, recv_pack->data.recv_name);
        mysql_real_query(&mysql, query, strlen(query));
    }
    else if(flag3 == 0 && flag1 == 1 && flag2 == 1 && flag4 == 1)
        ch[0] = '2';
    else if(flag4 == 0)
        ch[0] = '4';
    else if(flag2 == 0)
        ch[0] = '3';
    else if(flag1 == 0)
        ch[0] = '0';
    send_more(fd, flag, recv_pack, ch);
}

//查看所加群
void check_grp(PACK *recv_pack)
{
    int flag = CHECK_GRP;
    int fd = recv_pack->data.send_fd;
    Relation *q = pStart;
    int i = 0;

    while(q){
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && (q->statu_s == GRP || q->statu_s == GRP_OWN || q->statu_s == GRP_ADM)){
            strcpy(recv_pack->grp_info.groups[i], q->name2);
            i++;
        }
        q = q->next;
    }
    recv_pack->grp_info.grp_num = i;

    send_more(fd, flag, recv_pack, "");
}

//查看群中成员
void check_mem_grp(PACK *recv_pack)
{
    int flag = CHECK_MEM_GRP;
    int fd = recv_pack->data.send_fd;
    Relation *q = pStart;
    int i = 0;

    while(q){
        if(strcmp(q->name2, recv_pack->data.mes) == 0 && (q->statu_s == GRP || q->statu_s == GRP_OWN || q->statu_s == GRP_ADM)){
            strcpy(recv_pack->fri_info.friends[i], q->name1);
            i++;
        }
        q = q->next;
    }
    recv_pack->fri_info.friends_num = i;

    send_more(fd, flag, recv_pack, "");
}

//私聊
void chat_one(PACK *recv_pack)
{
    int flag = CHAT_ONE;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    char temp[MAX_CHAR];
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query[1500];
    int rows;
    int i = 0;
    User *t = pHead;
    Relation *q = pStart;
    int flag2 = 0;
    int flag3 = 0;
    Recordinfo *pNew = (Recordinfo *)malloc(sizeof(Recordinfo));

    if(strcmp(recv_pack->data.mes, "q") == 0){
        while(t){
            if(strcmp(t->name, recv_pack->data.send_name) == 0){
                t->statu_s = ONLINE;
                t->chat[0] = '\0';
                free(pNew);
                pNew = NULL;
                return;
            }
            t = t->next;
        }
    }

    while(q){//不是好友
        if(((strcmp(q->name1,recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0) || (strcmp(q->name2,recv_pack->data.send_name) == 0 && strcmp(q->name1, recv_pack->data.recv_name) == 0)) && (q->statu_s == FRI_BLK)){
            ch[0] = '3';
            send_more(fd, flag, recv_pack, ch);
            free(pNew);
            pNew = NULL;
            return;
        }
        q = q->next;
    }
    t = pHead;
    while(t){
        if(strcmp(t->name, recv_pack->data.recv_name) == 0){
            flag2 = 1;
            break;
        }
        t = t->next;
    }
    if(flag2 == 0){//用户不存在
        ch[0] = '0';
        send_more(fd, flag, recv_pack, ch);
        free(pNew);
        pNew = NULL;
        return;
    }
    else
    {
        if(recv_pack->data.mes[0] == '1'){
            memset(query, 0, strlen(query));
            sprintf(query, "select * from off_records where name1='%s' and name2='%s'", recv_pack->data.recv_name, recv_pack->data.send_name);
            mysql_real_query(&mysql, query, strlen(query));
            res = mysql_store_result(&mysql);
            rows = mysql_num_rows(res);
            while(row = mysql_fetch_row(res)){
                strcpy(pNew->name1, row[0]);
                strcpy(pNew->name2, row[1]);
                strcpy(pNew->message, row[2]);
                Insert_RC(pNew);
                memset(query, 0, strlen(query));
                sprintf(query, "insert into records values('%s', '%s', '%s')", row[0], row[1], row[2]);
                mysql_real_query(&mysql, query, strlen(query));
                
                strcpy(recv_pack->rec_info[i].name1, row[0]);
                strcpy(recv_pack->rec_info[i].name2, row[1]);
                strcpy(recv_pack->rec_info[i].message, row[2]);
                i++;
                if(i > 50)
                    break;                          
            }
            recv_pack->rec_info[i].message[0] = '0';
            send_more(fd, flag, recv_pack, "6");

            memset(query, 0, strlen(query));
            sprintf(query, "delete from off_records where name1='%s' and name2='%s'", recv_pack->data.recv_name, recv_pack->data.send_name);
            mysql_real_query(&mysql, query, strlen(query));
            
            t = pHead;
            while(t){
                if(strcmp(t->name, recv_pack->data.send_name) == 0){
                    t->statu_s = ONE_CHAT;
                    strcpy(t->chat, recv_pack->data.recv_name);
                    break;
                }
                t = t->next;
            }
            t = pHead;
            while(t){
                if(strcmp(t->name, recv_pack->data.recv_name) == 0 && (t->statu_s != OFFLINE)){
                    flag3 = 1;
                    break;
                }
                t = t->next;
            }
            if(flag3 == 1){
                ch[0] = '1';
                fd = t->fd;
                strcpy(temp,recv_pack->data.recv_name);
                strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                strcpy(recv_pack->data.send_name, temp);
                send_more(fd, flag, recv_pack, ch);
            }
            else{
                ch[0] = '2';
                send_more(fd, flag, recv_pack, ch);
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));
            }
        }
        else{
            t = pHead;
            while(t){
                if(strcmp(t->name, recv_pack->data.recv_name) == 0 && strcmp(t->chat, recv_pack->data.send_name) == 0 && (t->statu_s == ONE_CHAT)){
                    fd = t->fd;
                    strcpy(pNew->name1, recv_pack->data.send_name);
                    strcpy(pNew->name2, recv_pack->data.recv_name);
                    strcpy(pNew->message, recv_pack->data.mes);
                    Insert_RC(pNew);

                    memset(query, 0, strlen(query));
                    sprintf(query, "insert into records values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
                    mysql_real_query(&mysql, query, strlen(query));
                    
                    memset(temp, 0, MAX_CHAR);
                    strcpy(temp,recv_pack->data.recv_name);
                    strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                    send_more(fd, flag, recv_pack, recv_pack->data.mes);
                    return;
                }
                else if(strcmp(t->name, recv_pack->data.recv_name) == 0 && strcmp(t->chat, recv_pack->data.send_name) != 0){
                    memset(query, 0, strlen(query));
                    sprintf(query, "insert into off_records values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
                    mysql_real_query(&mysql, query, strlen(query));
                    free(pNew);
                    pNew = NULL;
                    return;
                }
                t = t->next;
            }
        }
    }
}

//加入聊天记录
void Insert_RC(Recordinfo *pNew)
{
    Recordinfo *p = pRec;
    if(p==NULL){
        p=pNew;
        pNew->next = NULL;
    }
    else{
        while(p && p->next != NULL)
            p = p->next;
        p->next = pNew;
        pNew->next = NULL;
    }
}

//群聊
void chat_many(PACK *recv_pack)
{
    int flag = CHAT_MANY;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    char temp[MAX_CHAR];
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query[1500];
    int rows;
    PACK recv_t;
    recv_t.type = flag;
    int i = 0,j = 0;
    User *t = pHead;
    Relation *q = pStart;
    int flag2 = 0;

    Recordinfo *pNew = (Recordinfo *)malloc(sizeof(Recordinfo));
    if(strcmp(recv_pack->data.mes, "q") == 0){
        while(t){
            if(strcmp(t->name, recv_pack->data.send_name) == 0){
                t->statu_s = ONLINE;
                free(pNew);
                pNew = NULL;
                return;
            }
            t = t->next;
        }
    }
    while(q){
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s >= GRP)){
            flag2 = 1;
            break;
        }
        q = q->next;
    }
    if(flag2 == 0){//不是群成员
        ch[0] = '0';
        send_more(fd, flag, recv_pack, ch);
        free(pNew);
        pNew = NULL;
        return;
    }
    else{
        if(strcmp(recv_pack->data.mes, "1") == 0){//加入群聊
            memset(query, 0, strlen(query));
            sprintf(query, "select * from records where name2='%s'", recv_pack->data.recv_name);
            mysql_real_query(&mysql, query, strlen(query));
            res = mysql_store_result(&mysql);
            rows = mysql_num_rows(res);
            if(rows != 0){
                while(row = mysql_fetch_row(res)){
                    if(rows <= 30){
                        strcpy(recv_pack->rec_info[i].name1, row[0]);
                        strcpy(recv_pack->rec_info[i].name2, row[1]);
                        strcpy(recv_pack->rec_info[i].message, row[2]);
                    }
                    else{
                        if(rows - i <= 30){
                            strcpy(recv_pack->rec_info[j].name1, row[0]);
                            strcpy(recv_pack->rec_info[j].name2, row[1]);
                            strcpy(recv_pack->rec_info[j].message, row[2]);
                            j++;
                        }
                    }
                    i++;
                }
            }
            if(rows <= 30)
                recv_pack->rec_info[i].message[0] = '0';
            else
                recv_pack->rec_info[j].message[0] = '0';

            send_more(fd, flag, recv_pack, "6");
            free(pNew);
            pNew = NULL;

            t = pHead;
            while(t){
                if(strcmp(t->name, recv_pack->data.send_name) == 0){
                    t->statu_s = MANY_CHAT;
                    strcpy(t->chat, recv_pack->data.recv_name);
                    break;
                }
                t = t->next;
            }
            q = pStart;
            while(q){
                if(strcmp(q->name1, recv_pack->data.send_name) != 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s >= GRP)){
                    t = pHead;
                    while(t){
                        if(strcmp(q->name1, t->name) == 0 && (t->statu_s != OFFLINE)){
                            ch[0] = '1';
                            fd = t->fd;
                            send_more(fd, flag, recv_pack, ch);
                            break;
                        }
                        else if(strcmp(q->name1, t->name) == 0 && (t->statu_s == OFFLINE)){
                            strcpy(recv_t.data.send_name, t->name);
                            strcpy(recv_t.data.recv_name, recv_pack->data.recv_name);
                            memcpy(&Mex_Box[sign++], &recv_t, sizeof(PACK));      
                            break;
                        }
                        t = t->next;
                    }
                }
                q = q->next;
            }
        }
        else{
            strcpy(pNew->name1, recv_pack->data.send_name);
            strcpy(pNew->name2, recv_pack->data.recv_name);
            strcpy(pNew->message, recv_pack->data.mes);
            Insert_RC(pNew);
            memset(query, 0, strlen(query));
            sprintf(query, "insert into records values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
            mysql_real_query(&mysql, query, strlen(query));

            q = pStart;
            while(q){
                if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s >= GRP)){
                    t = pHead;
                    while(t){
                        if(strcmp(q->name1, t->name) == 0 && strcmp(t->chat, recv_pack->data.recv_name) == 0 && (t->statu_s == MANY_CHAT)){
                            fd = t->fd;
                            bzero(temp, MAX_CHAR);
                            strcpy(temp,recv_pack->data.recv_name);
                            strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                            send_more(fd, flag, recv_pack, recv_pack->data.mes);
                            strcpy(recv_pack->data.send_name, temp);
                            bzero(temp, MAX_CHAR);
                            strcpy(temp,recv_pack->data.recv_name);
                            strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                            strcpy(recv_pack->data.send_name, temp);
                            break;
                        }
                        t = t->next;
                    }
                }
                q = q->next;
            }
        }
    }
}

//查看与好友聊天记录
void check_mes_fri(PACK *recv_pack)
{
    int i = 0;
    int flag = CHECK_MES_FRI;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    Relation *q = pStart;
    Recordinfo *p = pRec;
    int flag2 = 0;
    while(q){
        if(((strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0) || (strcmp(q->name2, recv_pack->data.send_name) == 0 && strcmp(q->name1, recv_pack->data.mes) == 0)) && (q->statu_s == FRIEND)) {
            flag2 = 1;
            break;
        }
        q = q->next;
    }
    if(flag2 == 0)
        ch[0] = '0';
    else{
        ch[0] = '1';
        while(p){
            if((strcmp(p->name1, recv_pack->data.send_name) == 0 && strcmp(p->name2, recv_pack->data.mes) == 0) || (strcmp(p->name2, recv_pack->data.send_name) == 0 && strcmp(p->name1, recv_pack->data.mes) == 0)){
                strcpy(recv_pack->rec_info[i].name1, p->name1);
                strcpy(recv_pack->rec_info[i].name2, p->name2);
                strcpy(recv_pack->rec_info[i].message, p->message);
                i++;
                if(i > 50)
                    break;
            }
            p = p->next;
        }
    }
    recv_pack->rec_info[i].message[0] = '0';
                            
    send_more(fd, flag, recv_pack, ch);
}

//查看群组聊天记录
void check_mes_grp(PACK *recv_pack)
{
    int i = 0;
    int flag = CHECK_MES_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    Relation *q = pStart;
    Recordinfo *p = pRec;
    int flag2 = 0;
    while(q){
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0 && (q->statu_s >= GRP)){
            flag2 = 1;
            break;
        }
        q = q->next;
    }
    if(flag2 == 0)
        ch[0] = '0';
    else{
        ch[0] = '1';
        while(p){
            if((strcmp(p->name2, recv_pack->data.mes) == 0)) {
                strcpy(recv_pack->rec_info[i].name1, p->name1);
                strcpy(recv_pack->rec_info[i].name2, p->name2);
                strcpy(recv_pack->rec_info[i].message, p->message);
                i++;
                if(i > 50)
                    break;
            }
            p = p->next;
        }
    }
    recv_pack->rec_info[i].message[0] = '0';
    
    send_more(fd, flag, recv_pack, ch);
}


//接收文件
void recv_file(PACK *recv_pack)
{
    int flag = RECV_FILE;
    int fd = recv_pack->data.send_fd;
    int length = 0;
    int i = 0;
    char mes[MAX_CHAR * 3 + 1];
    char *name;
    bzero(mes, MAX_CHAR * 3 + 1);
    int fp;
    User *t = pHead;
    int flag2 = 0;
    if(strcmp(recv_pack->data.mes,"send") == 0){
        while(t){
            if(strcmp(t->name, recv_pack->data.recv_name) == 0){
                flag2 = 1;
                break;
            }
            t = t->next;
        }
        if(flag2 == 1){
            file.file_name[file.sign_file][0] = '_';
            for(i = 0; i < strlen(recv_pack->data.send_name); i++){
                if(recv_pack->data.send_name[i] == '/'){
                    name = strrchr(recv_pack->data.send_name, '/');
                    name++;
                    strcat(file.file_name[file.sign_file],name);
                    break;
                }
            }
            if(i == strlen(recv_pack->data.send_name))
                strcat(file.file_name[file.sign_file],recv_pack->data.send_name);

            strcpy(file.file_send_name[file.sign_file], recv_pack->data.recv_name);
            fp = creat(file.file_name[file.sign_file], S_IRWXU);
            file.sign_file++;
            close(fp);
            send_more(fd, flag, recv_pack, "1");
        }
        else 
            send_more(fd, flag, recv_pack, "0");
    }
    else if(strcmp(recv_pack->data.mes, "success") == 0){
        while(t){
            if(strcmp(t->name, recv_pack->data.recv_name) == 0 && (t->statu_s != OFFLINE)){
                flag2 = 1;
                break;
            }
            t = t->next;
        }
        if(flag2 == 1)
            send_file(recv_pack);
        else if(flag2 == 0)
            memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));    
    }
    else{
        for(i = 0; i < file.sign_file; i++){
            if(strcmp(recv_pack->data.recv_name, file.file_send_name[i]) == 0){
                fp = open(file.file_name[i], O_WRONLY | O_APPEND);
                break;
            }
        }
        if(write(fp, recv_pack->file.mes, recv_pack->file.size) < 0)
            my_err("write", __LINE__);
        close(fp);
        //send_more(fd, flag, recv_pack, "");
    }
}


//发送文件
void send_file(PACK *recv_pack)
{
    int flag = SEND_FILE;
    int fd = recv_pack->data.send_fd;
    int fd2;
    int fp;
    int length = 0;
    PACK send_file;
    send_file.type = flag;

    char temp[MAX_CHAR];
    User *t = pHead;
    int flag_2 = 0;
    int i = 0;
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.recv_name) == 0)
        {
            fd2 = t->fd;
            break;
        }
        t = t->next;
    }

    if(strcmp(recv_pack->data.mes, "success") == 0)
    {
        strcpy(temp,recv_pack->data.recv_name);
        strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
        strcpy(recv_pack->data.send_name, temp);
        send_more(fd2, flag, recv_pack, "5");
    }
    else if(recv_pack->data.mes[0] == 'y')
    {
        for(i = 0; i < file.sign_file; i++)
            if(strcmp(file.file_send_name[i], recv_pack->data.send_name) == 0)
                break;
        send_more(fd2, flag, recv_pack, "1");
        strcpy(recv_pack->data.recv_name, file.file_name[i]);
        send_more(fd, flag, recv_pack, "4");

        strcpy(send_file.data.send_name, recv_pack->data.recv_name);
        strcpy(send_file.data.recv_name, recv_pack->data.send_name);
        fp = open(file.file_name[i], O_RDONLY);
        if(fp == -1)
            printf("未找到文件%s！\n", file.file_name[i]);
        while((length = read(fp, send_file.file.mes, MAX_FILE - 1)) > 0)
        {
            send_file.file.size = length;
            if(send(fd, &send_file, sizeof(PACK), 0) < 0)
                my_err("send",__LINE__);
            bzero(send_file.file.mes, MAX_FILE);
        }
        printf("发送成功!\n");
        send_more(fd, flag, recv_pack, "3");
        send_more(fd2, flag, recv_pack, "2");
        remove(file.file_name[i]);
        file.file_send_name[i][0] = '\0';
        close(fp);
    }
    else if(recv_pack->data.mes[0] == 'n')
    {
        send_more(fd2, flag, recv_pack, "0");
        for(i = 0; i < file.sign_file; i++)
            if(strcmp(file.file_send_name[i], recv_pack->data.send_name) == 0)
                break;
        remove(file.file_name[i]);
        file.file_send_name[i][0] = '\0';
    }
}


void send_more(int fd, int type, PACK *recv_pack, char *mes)
{
    PACK pack_send;
    char temp[MAX_CHAR];
    memcpy(&pack_send, recv_pack, sizeof(PACK));
    strcpy(temp,pack_send.data.recv_name);
    pack_send.type = type;
    strcpy(pack_send.data.recv_name, pack_send.data.send_name);
    strcpy(pack_send.data.send_name, temp);
    strcpy(pack_send.data.mes, mes);
    pack_send.data.recv_fd = pack_send.data.send_fd;
    pack_send.data.send_fd = fd;

    if(send(fd, &pack_send, sizeof(PACK), 0) < 0)
        my_err("send", __LINE__);
}

//发送信息
void send_pack(int fd, PACK *recv_pack, char *ch)
{
    PACK pack_send;
    memcpy(&pack_send, recv_pack, sizeof(PACK));
    strcpy(pack_send.data.recv_name, pack_send.data.send_name);
    strcpy(pack_send.data.send_name, "server");
    strcpy(pack_send.data.mes, ch);
    pack_send.data.recv_fd = pack_send.data.send_fd;
    pack_send.data.send_fd = fd;

    if(send(fd, &pack_send, sizeof(PACK), 0) < 0)
        my_err("send", __LINE__);
}

//销毁链表
void DeleteLink()		
{
    User *q = pHead;
    if(pHead == NULL)
        return;
    while(pHead){
        q = pHead->next;
        free(pHead);
        pHead = q;
    }
	pHead = NULL;
}

void DeleteLink_R()		
{
    Relation *q = pStart;
    if(pStart == NULL)
        return;
    while(pStart){
        q = pStart->next;
        free(pStart);
        pStart = q;
    }
	pStart = NULL;
}

void DeleteLink_RC()
{
    Recordinfo *q = pRec;
    if(pRec == NULL)
        return;
    while(pRec){
        q = pRec->next;
        free(pRec);
        pRec = q;
    }
	pRec = NULL;
}
