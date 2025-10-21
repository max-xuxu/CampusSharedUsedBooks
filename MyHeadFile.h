#include <stdio.h>     
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>  
#include <sqlite3.h>    
#include <pthread.h>  
#include <time.h>      
#include <dirent.h>    
#include <fcntl.h>     
#include <errno.h>     


#define Register      1       // 注册请求
#define login         2       // 登录请求
#define publish_book  3       // 发布书籍请求
#define search_book   4       // 搜索书籍请求
#define borrow_book   5       // 借阅书籍请求
#define return_book   6       // 归还书籍请求
#define my_books      7       // 我的书籍请求
#define borrow_history 8      // 借阅历史请求
#define Exit          9       // 退出请求


#define success       0       // 操作成功
#define fail          -1      // 操作失败
#define user_exist    -2      // 注册时用户名已存在
#define user_pwd_err  -3      // 登录时用户名或密码错误
#define book_not_exist -4     // 书籍不存在


#define max_name_len  30      // 用户名/密码最大长度
#define max_book_info 50      // 书籍信息text类型最大长度
#define max_data_len  1024    // 数据缓冲区大小
#define max_records   20      // 最大记录数
#define user_dir_base "/home/linux/" // 用户文件存储根目录

// 书籍信息结构体
typedef struct {
    int id;                       // 书籍ID
    char book_name[max_book_info]; // 书名
    char author[max_book_info];    // 作者
    int condition;                // 书籍品相
    char location[max_book_info];  // 取书地点
    char lender[max_name_len];     // 书籍发布者
    int status;                    // 状态
    char cover_name[max_book_info];// 封面图片文件名
} BookInfo;

// 借阅记录结构体
typedef struct {
    int book_id;                       // 书籍ID
    char book_name[max_book_info];     // 书名
    char borrower[max_name_len];       // 借阅者
    char borrow_time[max_data_len/10]; // 借阅时间
    char return_time[max_data_len/10]; // 归还时间
} BorrowRecord;

// 通信结构体
typedef struct {
    int type;                         // 消息类型
    char username[max_name_len];      // 用户名
    char passwd[max_name_len];        // 密码
    
    // 书籍信息
    int book_id;                      // 书籍ID
    char book_name[max_book_info];    // 书名
    char author[max_book_info];       // 作者
    int condition;                    // 品相
    char location[max_book_info];     // 取书地点
    int status;                       // 状态
    
    // 封面文件传输
    char filedata[max_data_len];      // 封面文件二进制数据
    int size;                         // 封面数据大小
    char filename[max_book_info];     // 封面文件名
    
    // 批量数据传输
    int count;                        // 记录书籍或借阅记录数量
    BookInfo books[max_records];      // 查询返回的书籍集合
    BorrowRecord borrows[max_records];// 查询返回的记录集合
    
    char msg[max_data_len];           // 用于提示信息或错误
} MSG;

//服务器端函数
void initDB(sqlite3 *db);                  // 初始化数据库
int doRegister(MSG *msg);                  // 处理注册请求
int doLogin(MSG *msg);                     // 处理登录请求
int doPublishBook(MSG *msg, sqlite3 *db);  // 处理发布书籍请求
int doSearchBook(MSG *msg, sqlite3 *db);   // 处理搜索书籍请求
int doBorrowBook(MSG *msg, sqlite3 *db);   // 处理借阅书籍请求
int doReturnBook(MSG *msg, sqlite3 *db);   // 处理归还书籍请求
int doMyBooks(MSG *msg, sqlite3 *db);      // 处理查询个人书籍请求
int doBorrowHistory(MSG *msg, sqlite3 *db);// 处理借阅历史查询请求

//客户端处理函数
void registerReq(int sockfd, MSG *msg);    // 发送注册请求并处理响应
void loginReq(int sockfd, MSG *msg);       // 发送登录请求并处理响应
void publishBookReq(int sockfd, MSG *msg); // 发送发布书籍请求并处理响应
void searchBookReq(int sockfd, MSG *msg);  // 发送搜索书籍请求并处理响应
void borrowBookReq(int sockfd, MSG *msg);  // 发送借阅书籍请求并处理响应
void returnBookReq(int sockfd, MSG *msg);  // 发送归还书籍请求并处理响应
void myBooksReq(int sockfd, MSG *msg);     // 发送查询个人书籍请求并处理响应
void borrowHistoryReq(int sockfd, MSG *msg);// 发送借阅历史查询请求并处理响应
void showMainMenu(int sockfd, MSG *msg);   // 显示登录后的主菜单

