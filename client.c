#include "wrap.h"
#include "chat.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
    

#define SERV_PORT 8000
#define MAX_LINE 20

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
// #define RECV_FILE 20
// #define SEND_FILE 21

#define PASSIVE 0
#define ACTIVE 1


void *get_back(); //服务器返回结果
void Menu();            //主菜单
void Menu_friends();    //好友管理
void Menu_groups();     //群管理
void Menu_message();    //聊天记录
void Menu_mes_box();    //消息盒子
int login_menu();       //登陆菜单
int login();            //登陆
void registe();         //注册
void check_fri();       //查看好友列表
void add_fri();         //添加好友
void del_fri();         //删除好友
void shi_fri();         //屏蔽好友
void cre_grp();         //创建群
void add_grp();         //加群
void out_grp();         //退群
void power_grp_menu();  //群管理权限
void del_grp();         //解散群
void set_grp_adm();     //设置管理员
void kick_grp();        //踢人
void check_grp_menu();  //查看群
void check_grp();       //查看所加群
void check_mem_grp();   //查看群中成员
void Menu_chat();       //聊天+聊天记录
void chat_one();        //私聊
void chat_many();       //群聊
void check_mes_fri();   //查看与好友聊天记录
void check_mes_grp();   //查看群组聊天记录
void send_pack(int type, char *send_name, char *recv_name, char *mes);
// void send_file();       //发送文件
// void recv_file(PACK *recv_pack);       //接收文件
// int get_file_size(char *send_file_name); //得到文件大小




int confd;
char user[MAX_CHAR];    //当前登陆的账号名称
char grp_name[MAX_CHAR];
FRI_INFO fri_info;      //好友列表信息
GROUP_INFO grp_info;    //群列表信息
RECORD_INFO rec_info[55];  //聊天记录

int signal;

//来自外部的请求——消息盒子
char name[100][MAX_CHAR];    //发给的人
char mes_box[100][MAX_CHAR];//消息记录
int mes_box_inc[100];//加群/好友
int sign;
int sign_ive[100];//消息状态

pthread_mutex_t mutex;
pthread_cond_t cond;


int main(int argc, char *argv[]) {

    struct sockaddr_in serveraddr;
    char ipstr[ ]="127.0.0.1";
    pthread_t tid;

    confd = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, ipstr, &serveraddr.sin_addr.s_addr);
    serveraddr.sin_port = htons(SERV_PORT);

    Connect(confd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    
    if(login_menu() == 0){   //判断是否登陆成功
        Close(confd);
        return 0;
    }

    pthread_create(&tid, NULL, get_back, NULL);
    Menu();
    Close(confd);
    return 0;
}

//服务器返回结果
void *get_back()
{
    pthread_mutex_t mutex_g;
    pthread_mutex_init(&mutex_g, NULL);
    while(1)
    {
        int flag;
        int i = 0;
        PACK recv_pack;
        int ret = recv(confd, &recv_pack, sizeof(PACK), 0);
        if(ret < 0)
            my_err("recv", __LINE__);
        switch(recv_pack.type)
        {
            case CHECK_FRI:
                memcpy(&fri_info, &recv_pack.fri_info, sizeof(FRI_INFO));
                pthread_cond_signal(&cond);           
                break;
            case GET_FRI_STA:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("%s---离线\n",recv_pack.data.recv_name);
                else if(flag == 1)
                    printf("%s---在线\n",recv_pack.data.recv_name);            
                pthread_cond_signal(&cond);
                break;
            case ADD_FRI:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0){
                    sign_ive[sign] = PASSIVE;
                    sprintf(name[sign], "%s", recv_pack.data.send_name);
                    mes_box_inc[sign] = ADD_FRI;
                    sprintf(mes_box[sign], "%s请求加你为好友(y/n): ", recv_pack.data.send_name);
                    sign++;
                }
                else if(flag == 1){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "%s已同意请求", recv_pack.data.send_name);
                    sign++;
                }
                else if(flag == 2){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "%s拒绝了你的请求", recv_pack.data.send_name);
                    sign++;
                }
                else if(flag == 3){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "%s账号不存在", recv_pack.data.send_name);
                    sign++;
                }
                else if(flag == 4){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "%s已是你的好友", recv_pack.data.send_name);
                    sign++;
                }
                break;
            case DEL_FRI:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n他不是你的好友!\n");
                else if(flag == 1)
                    printf("\n删除成功!\n");                
                pthread_cond_signal(&cond);
                break;
            case SHI_FRI:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n他不是你的好友!\n");
                else if(flag == 1)
                    printf("\n屏蔽成功!\n");               
                pthread_cond_signal(&cond);
                break;
            case CRE_GRP:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n该群名已被注册!\n");
                else if(flag == 1)
                    printf("\n创建成功!\n");
                pthread_cond_signal(&cond);
                break;           
            case ADD_GRP:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n该群不存在!\n");
                else if(flag == 1){
                    memset(grp_name, 0, MAX_CHAR);
                    strcpy(grp_name, recv_pack.file.mes);
                    sign_ive[sign] = PASSIVE;
                    sprintf(name[sign], "%s", recv_pack.data.recv_name);
                    mes_box_inc[sign] = ADD_GRP;
                    sprintf(mes_box[sign], "%s请求加入群聊%s(y/n): ", recv_pack.data.recv_name, recv_pack.file.mes);
                    sign++;
                }
                else if(flag == 2){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "你已加入群聊%s", recv_pack.data.recv_name);
                    sign++;
                }
                else if(flag == 3){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "加入群聊%s请求被拒绝", recv_pack.data.recv_name);
                    sign++;
                }
                break;
            case OUT_GRP:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n该群不存在!\n");
                else if(flag == 1)
                    printf("\n退群成功!\n");
                pthread_cond_signal(&cond);
                break;
            case DEL_GRP:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n该群不存在!\n");
                else if(flag == 1)
                    printf("\n解散群成功!\n");
                else if(flag == 2)
                    printf("\n仅群主有权解散群!\n");
                pthread_cond_signal(&cond);
                break;
            case SET_GRP_ADM:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n该群不存在!\n");
                else if(flag == 1)
                    printf("\n设置管理员成功!\n");
                else if(flag == 2)
                    printf("\n只有群主可以设置管理员!\n");
                else if(flag == 3)
                    printf("\n此用户不在群中!\n");
                else if(flag == 6){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "你被设置为群%s的管理员！", recv_pack.data.send_name);
                    sign++;
                    break;
                }
                if(flag != 6)
                    pthread_cond_signal(&cond);
                break;
            case KICK_GRP:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("\n该群不存在!\n");
                else if(flag == 1)
                    printf("\n踢人成功!\n");
                else if(flag == 2)
                    printf("\n只有群主/管理员可以踢人!\n");
                else if(flag == 3)
                    printf("\n此用户不在群中!\n");
                else if(flag == 4)
                    printf("\n踢人失败!\n");
                else if(flag == 6){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "你被踢出群聊%s", recv_pack.data.send_name);
                    sign++;
                    break;
                }
                if(flag != 6)
                    pthread_cond_signal(&cond);
                break;
            case CHECK_GRP:
                memcpy(&grp_info, &recv_pack.grp_info, sizeof(GROUP_INFO));
                pthread_cond_signal(&cond);           
                break;
            case CHECK_MEM_GRP:
                memcpy(&fri_info, &recv_pack.fri_info, sizeof(FRI_INFO));
                pthread_cond_signal(&cond);           
                break;
            case CHAT_ONE:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0){
                    printf("\n该用户不存在!\n");
                    signal = 1;
                    pthread_cond_signal(&cond);           
                }
                else if(flag == 1){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "%s邀你私聊", recv_pack.data.send_name);
                    sign++;
                }
                else if(flag == 2){
                    printf("\n该用户不在线!\n");
                    pthread_cond_signal(&cond);           
                }
                else if(flag == 3){
                    printf("\n该好友已被屏蔽!\n");
                    signal = 1;
                    pthread_cond_signal(&cond);           
                }
                else if(flag == 6){
                    memcpy(&rec_info, &recv_pack.rec_info, sizeof(rec_info));
                    pthread_cond_signal(&cond);           
                }
                else
                    printf("\n%s\n\t\t%s: %s\n", recv_pack.data.recv_name, recv_pack.data.send_name, recv_pack.data.mes);
                break;
            case CHAT_MANY:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0){
                    printf("\n该群不存在!\n");
                    signal = 1;
                    pthread_cond_signal(&cond);           
                }
                else if(flag == 1){
                    sign_ive[sign] = ACTIVE;
                    sprintf(mes_box[sign], "群%s有人进入群聊", recv_pack.data.send_name);
                    sign++;
                }
                else if(flag == 2)
                    printf("\n群%s有新消息\n",recv_pack.data.send_name);
                else if(flag == 6){
                    memcpy(&rec_info, &recv_pack.rec_info, sizeof(rec_info));
                    pthread_cond_signal(&cond);           
                }
                else
                    printf("\n%s\n\t\t%s: %s\n", recv_pack.data.recv_name, recv_pack.data.send_name, recv_pack.data.mes);
                break;
            case CHECK_MES_FRI:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("该用户不是你的好友！\n");
                else if(flag == 1){
                    memcpy(&rec_info, &recv_pack.rec_info, sizeof(rec_info));
                    printf("\n-----------聊天记录-----------\n");
                    if(rec_info[0].message[0] == '0')
                        printf("暂无历史记录\n");
                    else{
                        while(rec_info[i].message[0] != '0'){
                            printf("%s-->%s: %s\n",rec_info[i].name1, rec_info[i].name2, rec_info[i].message);
                            i++;
                        }
                    }
                }
                pthread_cond_signal(&cond);
                break;
            case CHECK_MES_GRP:
                flag = recv_pack.data.mes[0] - '0';
                if(flag == 0)
                    printf("你不是该群成员！\n");
                else if(flag == 1){
                    memcpy(&rec_info, &recv_pack.rec_info, sizeof(rec_info));
                    printf("-----------聊天记录-----------\n");
                    if(rec_info[0].message[0] == '0')
                    printf("暂无历史记录\n");
                    else{
                        while(rec_info[i].message[0] != '0'){
                            printf("%s: %s\n",rec_info[i].name1, rec_info[i].message);
                            i++;
                        }
                    }
                }
                pthread_cond_signal(&cond);
                break;
            // case SEND_FILE:
            //     break;

            default:
                break;
        }
        if(ret == 0){
            printf("\n无法连接服务器!\n");
            exit(1);
        }
    }
}

//登陆菜单
int login_menu()
{
    int flag;
    int choice;
    do
    {
        printf("------------------------\n");
        printf("|\t1.登陆\t\t|\n");
        printf("------------------------\n");
        printf("|\t2.注册\t\t|\n");
        printf("------------------------\n");
        printf("|\t0.退出\t\t|\n");
        printf("------------------------\n");
        printf("请选择: ");
        scanf("%d",&choice);
        
        switch(choice)
        {
        case 1:
            if(login() == 1)
                return 1;
            break;
        case 2:
            registe();
            break;
        default:
            break;
        }
    }while(choice != 0);
    flag = EXIT;
    send_pack(flag, user, "server", " ");
    return 0;
}


//注册
void registe()
{
    int flag = REGISTE;
    char registe_name[MAX_CHAR];
    char registe_passwd[MAX_CHAR];
    PACK recv_registe;
    int registe_flag;

    printf("请输入用户名：");
    scanf("%s",registe_name);
    printf("请输入用户密码：");
    scanf("%s",registe_passwd);

    
    send_pack(flag, registe_name, "server", registe_passwd);
    printf("请稍等...\n");
    sleep(3);
    if(recv(confd, &recv_registe, sizeof(PACK), 0) < 0)
        my_err("recv", __LINE__);
    registe_flag = recv_registe.data.mes[0] - '0';

    if(registe_flag == 1)
        printf("注册成功!\n");
    else if(registe_flag == 0)
        printf("该用户名已存在，请重新选择!\n");
}

//登陆
int login()
{
    int flag = LOGIN;
    char login_name[MAX_CHAR];
    char login_passwd[MAX_CHAR];
    PACK recv_login;
    int login_flag;
    int i = 0;

    printf("请输入用户名称：");
    scanf("%s",login_name);
    getchar();                      
    printf("请输入用户密码：");
    do{
        login_passwd[i]=getch();
        if(login_passwd[i]=='\r'){
            printf("\n");
            break;
        }
        if(login_passwd[i]=='\b'){//为什么不行？？？？
            if(i==0){
                printf("\a");
                continue;
            }
            i--;
            printf("'\b' '\b'");
        }
        else{
            i++;
            printf("*");
        }
    }while(login_passwd[i]!='\n' && i<18);
    login_passwd[i]='\0';
	
    system("clear");
    send_pack(flag, login_name, "server", login_passwd);
    if(recv(confd, &recv_login, sizeof(PACK), 0) < 0)
        my_err("recv", __LINE__);
    
    login_flag = recv_login.data.mes[0] - '0';

    if(login_flag == 1)
    {
        printf("登陆成功!\n");
        strncpy(user, login_name, strlen(login_name));
        return 1;
    }
    else if(login_flag == 0)
        printf("登陆失败!\n");
    else if(login_flag == 2)
        printf("该用户已在线!\n");
    return 0;
}

//主菜单
void Menu()
{
    int choice;
    int flag;
    do
    {
        printf("---------------------------------\n");
        printf("|\t   1.好友管理   \t|\n");
        printf("---------------------------------\n");
        printf("|\t   2.群管理      \t|\n");
        printf("---------------------------------\n");
        printf("|\t   3.发送文件   \t|\n");
        printf("---------------------------------\n");
        printf("|\t   4.聊天通讯   \t|\n");
        printf("---------------------------------\n");
        printf("|\t   5.通知中心   \t|\n");
        printf("---------------------------------\n");
        printf("|\t   0.退出         \t|\n");
        printf("---------------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);

        switch(choice)
        {
        case 1:
            Menu_friends();
            break;
        case 2:
            Menu_groups();
            break;          
        // case 3:
        //     send_file();
        //     break;
        case 4:
            Menu_chat();
            break;
        case 5:
            Menu_mes_box();
            break;        
        default:
            break;
        }
    }while(choice != 0);
    flag = EXIT;
    send_pack(flag, user, "server", " ");
}

//好友管理
void Menu_friends()
{
    int choice;
    do
    {
        printf("---------------------------\n");
        printf("|\t1.查看好友列表    |\n");
        printf("---------------------------\n");
        printf("|\t2.添加好友\t  |\n");
        printf("---------------------------\n");
        printf("|\t3.删除好友\t  |\n");
        printf("---------------------------\n");
        printf("|\t4.屏蔽好友\t  |\n");
        printf("---------------------------\n");
        printf("|\t0.返回     \t  |\n");
        printf("---------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);

        switch(choice)
        {
        case 1:
            check_fri();
            break;
        case 2:
            add_fri();
            break;           
        case 3:
            del_fri();
            break;
        case 4:
            shi_fri();
            break;
        default:
            break;
        }
    }while(choice != 0);
}

//查看好友列表
void check_fri()
{
    int flag = CHECK_FRI;
    char mes[MAX_CHAR];
    bzero(mes, MAX_CHAR);
    memset(&fri_info, 0, sizeof(fri_info));
    int i;

    pthread_mutex_lock(&mutex);
    send_pack(flag, user, "server", "1");
    pthread_cond_wait(&cond, &mutex);
    printf("\n-----------好友列表-----------\n");
    if(fri_info.friends_num == 0)
        printf("暂无好友!\n");
    else{
        for(i = 0; i < fri_info.friends_num; i++){
            if(fri_info.friends_status[i] == 1){
                flag = GET_FRI_STA;
                send_pack(flag, fri_info.friends[i], "server", mes);
                pthread_cond_wait(&cond, &mutex);
            }
            else if(fri_info.friends_status[i] == 2)
                printf("%s---黑名单\n",fri_info.friends[i]);
        }
    }
    pthread_mutex_unlock(&mutex);
}

//添加好友
void add_fri()
{
    int i;
    int flag = ADD_FRI;
    pthread_mutex_lock(&mutex);
    char friend_add[MAX_CHAR];
    printf("你想要添加的用户名：");
    scanf("%s",friend_add);
    send_pack(flag, user, friend_add, "0");
    pthread_mutex_unlock(&mutex);
}

//删除好友
void del_fri()
{
    int flag = DEL_FRI;
    char friend_del[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要删除的用户名：");
    scanf("%s",friend_del);
    send_pack(flag, user, "server", friend_del);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//屏蔽好友
void shi_fri()
{
    int flag = SHI_FRI;
    char friend_shi[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要屏蔽的用户名：");
    scanf("%s",friend_shi);
    send_pack(flag, user, "server", friend_shi);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}


//群管理
void Menu_groups()
{
    int choice;
    do
    {
        printf("-----------------------------\n");
        printf("|         1.查看群          |\n");
        printf("-----------------------------\n");
        printf("|         2.创建群          |\n");
        printf("-----------------------------\n");
        printf("|         3.加入群          |\n");
        printf("-----------------------------\n");
        printf("|         4.退出群          |\n");
        printf("-----------------------------\n");
        printf("|         5.管理群          |\n");
        printf("-----------------------------\n");
        printf("|         0.返回            |\n");
        printf("-----------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);

        switch(choice)
        {
        case 1:
            check_grp_menu();           
            break;
        case 2:
            cre_grp();
            break;          
        case 3:
            add_grp();
            break;
        case 4:
            out_grp();
            break;
        case 5:
            power_grp_menu();
            break;
        default:
            break;
        }
    }while(choice != 0);
}

//查看群
void check_grp_menu()
{
    int choice;
    do
    {
        printf("-----------------------------\n");
        printf("|       1.查看所加群        |\n");
        printf("-----------------------------\n");
        printf("|       2.查看群中成员      |\n");
        printf("-----------------------------\n");
        printf("|       0.返回              |\n");
        printf("-----------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);

        switch(choice)
        {
        case 1:
            check_grp();
            break;
        case 2:
            check_mem_grp();
            break;
        default:
            break;
        }
    }while(choice != 0);
}

//查看所加群
void check_grp()
{
    int flag = CHECK_GRP;
    char mes[MAX_CHAR];
    memset(mes, 0, sizeof(mes));
    memset(&grp_info, 0, sizeof(grp_info));
    int i;

    pthread_mutex_lock(&mutex);
    send_pack(flag, user, "server", mes);
    pthread_cond_wait(&cond, &mutex);
    printf("\n-----------群聊列表-----------\n");
    if(grp_info.grp_num == 0)
        printf("暂无加入群聊!\n");
    else{
        for(i = 0; i < grp_info.grp_num; i++){
            printf("%s\n",grp_info.groups[i]);
        }
    }
    pthread_mutex_unlock(&mutex);
}

//查看群中成员
void check_mem_grp()
{
    int flag = CHECK_MEM_GRP;
    char mes[MAX_CHAR];
    int i;

    pthread_mutex_lock(&mutex);
    printf("\n你想要查看那个群中的成员信息：");
    scanf("%s",mes);
    for(i = 0; i < grp_info.grp_num; i++){
        if(strcmp(grp_info.groups[i], mes) == 0)
            break;
    }
    if(i > grp_info.grp_num)
        printf("你没有加入此群!\n");
    else{
        memset(&fri_info, 0, sizeof(fri_info));
        send_pack(flag, user, "server", mes);
        pthread_cond_wait(&cond, &mutex);
        printf("\n-----------%s-----------\n",mes);
        if(fri_info.friends_num == 0)
            printf("该群中暂无成员!\n");
        else
        {
            for(i = 0; i < fri_info.friends_num; i++)
                printf("%s\n", fri_info.friends[i]);
        }
    }
    pthread_mutex_unlock(&mutex);
}

//创建群
void cre_grp()
{
    int flag = CRE_GRP;
    char grp_cre[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要创建的群名称：");
    scanf("%s",grp_cre);
    send_pack(flag, user, "server", grp_cre);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//加群
void add_grp()
{
    int flag = ADD_GRP;
    char grp_add[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要加入的群名称：");
    scanf("%s",grp_add);
    send_pack(flag, user, "server", grp_add);
    pthread_mutex_unlock(&mutex);
}

//退群
void out_grp()
{
    int flag = OUT_GRP;
    char grp_out[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要退出的群名称：");
    scanf("%s",grp_out);
    send_pack(flag, user, "server", grp_out);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//群管理权限
void power_grp_menu()
{
    int choice;
    do
    {
        printf("-----------------------------\n");
        printf("|         1.解散群          |\n");
        printf("-----------------------------\n");
        printf("|         2.设置管理员      |\n");
        printf("-----------------------------\n");
        printf("|         3.踢人(管)        |\n");
        printf("-----------------------------\n");
        printf("|         0.返回            |\n");
        printf("-----------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);
        
        switch(choice)
        {
        case 1:
            del_grp();
            break;
        case 2:
            set_grp_adm();
            break;           
        case 3:
            kick_grp();
            break;
        default:
            break;
        }
    }while(choice != 0);
}

//解散群
void del_grp()
{
    int flag = DEL_GRP;
    char grp_del[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要解散的群名称：");
    scanf("%s",grp_del);
    send_pack(flag, user, "server", grp_del);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//设置管理员
void set_grp_adm()
{
    int flag = SET_GRP_ADM;
    char grp_set_1[MAX_CHAR];
    char grp_set_2[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要在那个群中设置谁为管理员：");
    scanf("%s",grp_set_1);
    scanf("%s",grp_set_2);
    send_pack(flag, user, grp_set_1, grp_set_2);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//踢人
void kick_grp()
{
    int flag = KICK_GRP;
    char grp_set_1[MAX_CHAR];
    char grp_set_2[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("你想要在那个群将谁踢出：");
    scanf("%s",grp_set_1);
    scanf("%s",grp_set_2);
    send_pack(flag, user, grp_set_1, grp_set_2);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}


void Menu_chat()
{

    int choice;
    do
    {
        printf("----------------------------\n");
        printf("|         1.私聊            |\n");
        printf("----------------------------\n");
        printf("|         2.群聊            |\n");
        printf("----------------------------\n");
        printf("|         3.聊天记录        |\n");
        printf("----------------------------\n");
        printf("|         0.返回            |\n");
        printf("----------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);

        switch(choice)
        {
        case 1: 
            chat_one(); 
            break;
        case 2:
            chat_many();
            break;
        case 3:
            Menu_message();
            break;
        default:
            break;
        }
    }while(choice != 0);
}

//私聊
void chat_one()
{
    int flag = CHAT_ONE;
    char chat_name[MAX_CHAR];
    char mes[MAX_CHAR];
    int i = 0;
    memset(mes, 0, sizeof(mes));
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n你想要和谁聊天呢? ");
    scanf("%s",chat_name);
    mes[0] = '1';
    send_pack(flag, user, chat_name, mes);
    
    pthread_cond_wait(&cond, &mutex);
    if(signal == 1){
        signal = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }
    printf("\n-----------%s-----------\n",rec_info[i].name2);
    while(rec_info[i].message[0] != '0'){
        printf("%s: %s\n",rec_info[i].name1,rec_info[i].message);
        i++;
    }
    
    printf("\n按q退出聊天\n");
    getchar();
    do
    {
        memset(mes, 0, sizeof(mes));
        printf("%s: ", user);
        //scanf("%s", mes);
        scanf("%[^\n]", mes);
        getchar();
        send_pack(flag, user, chat_name, mes);
    }while(strcmp(mes, "q") != 0);

    pthread_mutex_unlock(&mutex);
}


//群聊
void chat_many()
{
    int flag = CHAT_MANY;
    char chat_name[MAX_CHAR];
    char mes[MAX_CHAR];
    int i = 0;
    memset(mes, 0, sizeof(mes));
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n你想要在那个群中聊天呢? ");
    scanf("%s",chat_name);
    mes[0] = '1';
    send_pack(flag, user, chat_name, mes);
    
    pthread_cond_wait(&cond, &mutex);
    if(signal == 1)
    {
        signal = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }
    printf("\n-----------%s-----------\n",rec_info[i].name2);
    while(rec_info[i].message[0] != '0')
    {
        printf("%s: %s\n",rec_info[i].name1, rec_info[i].message);
        i++;
    }
    printf("\n按q退出群聊\n");
    getchar();
    do
    {
        memset(mes, 0, sizeof(mes));
        //scanf("%s", mes);
        scanf("%[^\n]", mes);
        getchar();
        send_pack(flag, user, chat_name, mes);
    }while(strcmp(mes, "q") != 0);

    pthread_mutex_unlock(&mutex);
}


//聊天记录
void Menu_message()
{
    int choice;
    do
    {
        printf("-----------------------------\n");
        printf("|       1.好友聊天记录      |\n");
        printf("-----------------------------\n");
        printf("|       2.群聊记录          |\n");
        printf("-----------------------------\n");
        printf("|       0.返回              |\n");
        printf("-----------------------------\n");
        printf("请选择：");
        scanf("%d",&choice);

        switch(choice)
        {
        case 1:
            check_mes_fri();
            break;
        case 2:
            check_mes_grp();
            break;
        default:
            break;
        }
    }while(choice != 0);
}

//与好友聊天记录
void check_mes_fri()
{
    int i = 0;
    int flag = CHECK_MES_FRI;
    char mes_fri[MAX_CHAR];
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n你想要查看与谁的聊天记录? ");
    scanf("%s",mes_fri);
    send_pack(flag, user, "server", mes_fri);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//群组聊天记录
void check_mes_grp()
{
    int i = 0;
    int flag = CHECK_MES_GRP;
    char mes_grp[MAX_CHAR];
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n你想要查看那个群的聊天记录? ");
    scanf("%s",mes_grp);
    send_pack(flag, user, "server", mes_grp);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

// //接收文件


//消息盒子
void Menu_mes_box()
{
    int i;
    char ch[5];
    pthread_mutex_lock(&mutex);
    printf("\n您有%d条消息未读\n", sign);
    for(i = 0; i < sign; i++){
        if(sign_ive[i] == PASSIVE){
            printf("NO.%d: %s", i + 1, mes_box[i]);
            scanf("%s", ch);
            if(mes_box_inc[i] == ADD_GRP)
                send_pack(mes_box_inc[i], grp_name, name[i], ch);
            else
                send_pack(mes_box_inc[i], user, name[i], ch);
        }
        else if(sign_ive[i] == ACTIVE)
            printf("NO.%d: %s\n", i + 1, mes_box[i]);
    }
    sign = 0;
    pthread_mutex_unlock(&mutex);
}

//发包
void send_pack(int type, char *send_name, char *recv_name, char *mes)
{
    PACK pack_send;
    memset(&pack_send, 0, sizeof(PACK));
    pack_send.type = type;
    pack_send.data.recv_fd = confd;
    strcpy(pack_send.data.send_name, send_name);
    strcpy(pack_send.data.recv_name, recv_name);
    strcpy(pack_send.data.mes, mes);
    if(send(confd, &pack_send, sizeof(PACK), 0) < 0)
        my_err("send",__LINE__);
}


