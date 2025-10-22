#include "MyHeadFile.h" 

//全局数据库指针
sqlite3 *db = NULL;

// 数据库执行函数封装
int db_exec(const char *sql)
{
    char *errmsg = NULL;  
    // 执行SQL语句
    int ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK) 
    {  
        fprintf(stderr, "DB exec error: %s\n", errmsg);  
        sqlite3_free(errmsg); 
        return fail;  
    }
    return success; 
}

//数据库查询并获取详细书籍信息
//执行查询语句将结果储存为BookInfo结构体数组
int db_get_books(const char *sql, BookInfo *books) 
{
    char *errmsg = NULL;  
    char **res = NULL;    // 定义二级指针指向数组存储查询结果
    int row, col;         // 行和列
    // 执行查询的sql语句并返回行数
    int ret = sqlite3_get_table(db, sql, &res, &row, &col, &errmsg);
    if (ret != SQLITE_OK) 
    {  
        fprintf(stderr, "DB get err: %s\n", errmsg);
        sqlite3_free(errmsg);
        return fail; 
    }
    
    // 提取res二维数组中的查询结果存储到到BookInfo数组用于查询
    int i;
    for (i = 0; i < row && i < max_records; i++) 
    {
        // 按列提取数据存储到书籍信息结构体中
        //(i+1)*col 是为了跳过表头 + 前 i 行数据, i是代表行数
        books[i].id = atoi(res[(i+1)*col + 0]);  // 第0列是书籍ID，使用atoi将字符串转为int型
        strcpy(books[i].book_name, res[(i+1)*col + 1]);  // 1列是书名
        strcpy(books[i].author, res[(i+1)*col + 2]);  // 2列是作者
        books[i].condition = atoi(res[(i+1)*col + 3]);  // 3列是品相
        strcpy(books[i].location, res[(i+1)*col + 4]);  // 4列是取书地点
        strcpy(books[i].lender, res[(i+1)*col + 5]);  // 5列是发布者
        books[i].status = atoi(res[(i+1)*col + 6]);  // 6列是状态
        strcpy(books[i].cover_name, res[(i+1)*col + 7]);  // 7列是封面文件名
    }
    
    sqlite3_free_table(res);  //存储后释放查询结果内存
    // 返回实际记录的行数也是数据的条数，数量不能超过定义为最大数的宏值
    return row > max_records ? max_records : row; 
}


//数据库查询并获取借阅记录
int db_get_borrows(const char *sql, BorrowRecord *borrows) 
{
    char *errmsg = NULL;
    char **res = NULL;
    int row, col;
    // 执行查询并获取结果
    int ret = sqlite3_get_table(db, sql, &res, &row, &col, &errmsg);
    if (ret != SQLITE_OK)
    {
        fprintf(stderr, "DB get err: %s\n", errmsg);
        sqlite3_free(errmsg);
        return fail;  
    }
    
    // 提取查询结果到BorrowRecord数组
    int i;
    for (i = 0; i < row && i < max_records; i++)  
    {
        borrows[i].book_id = atoi(res[(i+1)*col + 0]);  // 书籍ID
        strcpy(borrows[i].book_name, res[(i+1)*col + 1]);  // 书名
        strcpy(borrows[i].borrower, res[(i+1)*col + 2]);  // 借阅者
        strcpy(borrows[i].borrow_time, res[(i+1)*col + 3]);  // 借阅时间
        strcpy(borrows[i].return_time, res[(i+1)*col + 4]);  // 归还时间
    }
    
    sqlite3_free_table(res); 
    return row > max_records ? max_records : row;  
}


//注册用户时根据用户名创建该用户的目录用于存储上传的书籍封面
int createUserDir(const char *username)
{
    char cmd[128];  // 存储创建目录的命令
    // 构建创建目录的命令：-p保证父目录存在
    sprintf(cmd, "mkdir -p %s%s && chmod 777 %s%s", 
            user_dir_base, username, user_dir_base, username); 
    // 执行命令
    if (system(cmd) == -1) 
    {
        perror("create dir fail");  
        return fail;  
    }
    return success;  
}


//初始化数据库并且创建数据库表结构
void initDB(sqlite3 *db) 
{
    // 创建用户表
    db_exec("create table if not exists users ("
            "username text primary key, passwd text not null);");
    
    // 创建书籍表
    db_exec("create table if not exists books ("
            "id integer primary key autoincrement,"  // 自增ID
            "book_name text not null, author text not null,"  // 书名、作者
            "condition integer not null, location text not null,"  // 品相、取书地点
            "lender text not null, status integer default 0,"  // 发布者、状态（0可借，1已借）
            "cover_name text not null,"  // 封面图片文件名
            "foreign key(lender) references users(username));");  // 外键关联用户表
    
    // 创建借阅记录表
    db_exec("create table if not exists borrows ("
            "book_id integer not null,"  // 书籍ID
            "book_name text not null,"  // 书名
            "borrower text not null,"  // 借阅者
            "borrow_time text not null,"  // 借阅时间
            "return_time text default '',"  // 归还时间
            "foreign key(book_id) references books(id));");  // 外键关联书籍表
}

//处理注册请求
int doRegister(MSG *msg) {
    char sql[512];  
    // 检查用户名是否已存在
    sprintf(sql, "select * from users where username='%s';", msg->username);
    char **res;  
    int row, col; 
    // 执行查询语句
    sqlite3_get_table(db, sql, &res, &row, &col, NULL);
    if (row > 0) 
    {  				 
        //返回行数大于0说明已经创建了相同用户名
        sqlite3_free_table(res);  
        strcpy(msg->msg, "用户名已存在");
        return user_exist;  
    }
    sqlite3_free_table(res);  // 释放结果内存
    
    //查询成功没有相同用户名插入新用户记录
    sprintf(sql, "insert into users values('%s','%s');", msg->username, msg->passwd);
    if (db_exec(sql) == fail) 
    {  
        strcpy(msg->msg, "注册失败");
        return fail;  
    }
    
    // 创建用户目录
    if (createUserDir(msg->username) == fail)  
    {
        strcpy(msg->msg, "目录创建失败");
        return fail; 
    }
    
    strcpy(msg->msg, "注册成功！");
    return success; 
}


//处理登录请求
int doLogin(MSG *msg) 
{
    char sql[512];
    // 查询匹配的用户名和密码
    sprintf(sql, "select * from users where username='%s' and passwd='%s';",
            msg->username, msg->passwd);
    char **res;
    int row, col;
    sqlite3_get_table(db, sql, &res, &row, &col, NULL);
    if (row == 0)
    { 
       
        sqlite3_free_table(res);
        strcpy(msg->msg, "用户名或密码错误");
        return user_pwd_err; 
    }
    sqlite3_free_table(res);
    
    // 检查用户目录是否存在
    char dirpath[100];
    sprintf(dirpath, "%s%s", user_dir_base, msg->username);  
    DIR *dp = opendir(dirpath);  //根据客户端传回的当前用户名打开对应目录
    if (dp == NULL)
    {  
        // 目录不存在
        strcpy(msg->msg, "用户目录不存在");
        return fail; 
    }
    closedir(dp);  
    
    strcpy(msg->msg, "登录成功！");
    return success; 
}


//处理发布书籍请求
int doPublishBook(MSG *msg, sqlite3 *db) 
{
    char sql[512], cover_path[200];  //存储SQL语句和封面图片路径
    
    //构建封面图片的保存路径
    sprintf(cover_path, "%s%s/%s", 
            user_dir_base,   
            msg->username,   // 根据用户名为找到相应存储的目录路径
            msg->filename);  // 封面文件名
    
    //将客户端发送的图片数据写入文件
    int fd = open(cover_path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd == -1) 
    {  
        sprintf(msg->msg, "封面保存失败：%s（路径：%s）", 
                strerror(errno), cover_path);
        return fail;  
    }
    // 写入图片以二进制数据形式
    write(fd, msg->filedata, msg->size);
    close(fd);
    
    //将书籍信息存入数据库
    sprintf(sql, "insert into books(book_name,author,condition,location,lender,cover_name) "
            "values('%s','%s',%d,'%s','%s','%s');",
            msg->book_name, msg->author, msg->condition,
            msg->location, msg->username, msg->filename);  //在数据库中存储封面文件名
     // 数据库操作失败
    if (db_exec(sql) == fail) 
    { 
        unlink(cover_path);  //删除已保存的封面图片
        strcpy(msg->msg, "发布失败（数据库错误）");
        return fail;  
    }
    
    //获取刚插入的书籍ID打印成功的提示信息
    msg->book_id = sqlite3_last_insert_rowid(db);
    sprintf(msg->msg, "发布成功！书籍ID：%d，封面已保存至：%s", 
            msg->book_id, cover_path);
    return success;  
}

//处理搜索书籍请求
int doSearchBook(MSG *msg, sqlite3 *db) 
{
    char sql[512];
    //使用模糊查询方便与查询书籍不必输入完整书名
    sprintf(sql, "select * from books where book_name like '%%%s%%';", msg->book_name);
    
    //查询并获取书籍详细信息
    msg->count = db_get_books(sql, msg->books);
    if (msg->count < 0) 
    { 
        strcpy(msg->msg, "搜索失败");
        return fail;  
    }
    
    sprintf(msg->msg, "搜索到%d本相关书籍", msg->count);
    return success; 
}


//处理借阅书籍请求
int doBorrowBook(MSG *msg, sqlite3 *db) 
{
    char sql[512], time_buf[20], book_name[50];
    // 获取当前时间
    time_t t = time(NULL);
    strftime(time_buf, 20, "%Y-%m-%d %H:%M", localtime(&t));
    
    // 获取书籍名称和当前状态
    //使用预处理语句可以将将sql语句与传输的数据分来，在客户端执行多次借阅时不重复编译完整sql语句，只需编译一次
    sqlite3_stmt *stmt;  

    sprintf(sql, "select book_name, status from books where id=%d;", msg->book_id);
    // 准备预处理语句
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) 
    {
        strcpy(msg->msg, "查询书籍失败");
        return fail; 
    }
    
    // 执行查询并检查结果
    if (sqlite3_step(stmt) != SQLITE_ROW) 
    {  
        // 没有找到书籍
        sqlite3_finalize(stmt);  // 释放预处理语句
        strcpy(msg->msg, "书籍不存在");
        return book_not_exist; 
    }
    
    // 提取查询结果
    strcpy(book_name, (const char*)sqlite3_column_text(stmt, 0));  // 书名
    int status = sqlite3_column_int(stmt, 1);  // 状态
    sqlite3_finalize(stmt);  // 释放预处理语句
    
    if (status == 1) 
    {  
        strcpy(msg->msg, "书籍已借出");
        return fail;  
    }
    
    // 更新书籍状态为已借出将status置1
    sprintf(sql, "update books set status=1 where id=%d;", msg->book_id);
    db_exec(sql);
    
    // 记录借阅历史到借阅历史表以便于查询该用户的借阅历史记录
    sprintf(sql, "insert into borrows(book_id, book_name, borrower, borrow_time) "
            "values(%d, '%s', '%s', '%s');",
            msg->book_id, book_name, msg->username, time_buf);
    db_exec(sql);

    
    strcpy(msg->msg, "借阅成功");
    return success; 
}


//处理归还书籍请求
int doReturnBook(MSG *msg, sqlite3 *db) 
{
    char sql[512], time_buf[20];
 
    time_t t = time(NULL);
    strftime(time_buf, 20, "%Y-%m-%d %H:%M", localtime(&t));
    
    //检查是否存在该用户的未归还记录没有记录则代表该用户没有借阅的书籍无法归还
    sprintf(sql, "select * from borrows where book_id=%d and borrower='%s' and return_time='';",
            msg->book_id, msg->username);
    char **res;
    int row, col;
    sqlite3_get_table(db, sql, &res, &row, &col, NULL);
    if (row == 0) 
    {  
        // 没有找到匹配的借阅记录
        sqlite3_free_table(res);
        strcpy(msg->msg, "无此借阅记录");
        return fail; 
    }
    sqlite3_free_table(res);
    
    //归还后更新书籍状态为可借阅将status置0
    sprintf(sql, "update books set status=0 where id=%d;", msg->book_id);
    db_exec(sql);
    
    // 更新归还时间
    sprintf(sql, "update borrows set return_time='%s' where book_id=%d and borrower='%s';",
            time_buf, msg->book_id, msg->username);
    db_exec(sql);
    
    strcpy(msg->msg, "归还成功");
    return success;  
}

//处理查询本人发布的书籍查询请求
int doMyBooks(MSG *msg, sqlite3 *db) 
{
    char sql[512];
    //查询当前用户已经发布的书籍
    sprintf(sql, "select * from books where lender='%s';", msg->username);
    
    //获取书籍详细信息
    msg->count = db_get_books(sql, msg->books);
    if (msg->count < 0) 
    {  // 查询失败
        strcpy(msg->msg, "查询失败");
        return fail;  
    }
    
    sprintf(msg->msg, "您发布了%d本书籍", msg->count);
    return success;  
}


//处理借阅历史查询请求
int doBorrowHistory(MSG *msg, sqlite3 *db) 
{
    char sql[512];
    //根据用户名查询当前用户的借阅记录
    sprintf(sql, "select book_id, book_name, borrower, borrow_time, return_time "
            "from borrows where borrower='%s';", msg->username);
    
    //获取借阅记录
    msg->count = db_get_borrows(sql, msg->borrows);
    if (msg->count < 0) 
    {  // 查询失败
        strcpy(msg->msg, "查询失败");
        return fail;
    }
    
    sprintf(msg->msg, "您有%d条借阅记录", msg->count);
    return success;  
}


//进行对客户端线程操作的处理传入socket描述符
void *client_handler(void *arg) 
{
    int sockfd = *(int*)arg;  // 提取客户端socket描述符
    free(arg);  // 释放动态分配的内存
    MSG msg; 
    
    while (1) 
    { 
        memset(&msg, 0, sizeof(MSG));  //清空消息结构体
        // 接收客户端数据
        int ret = recv(sockfd, &msg, sizeof(MSG), 0);
	
        if (ret <= 0) 
        {  
            printf("客户端断开\n");
            break; 
        }
        
        // 根据消息类型调用相应处理函数
        switch (msg.type) {
            case Register:  
                msg.type = doRegister(&msg); 
                break;
            case login:  
                msg.type = doLogin(&msg); 
                break;
            case publish_book: 
                msg.type = doPublishBook(&msg, db); 
                break;
            case search_book: 
                msg.type = doSearchBook(&msg, db); 
                break;
            case borrow_book:  
                msg.type = doBorrowBook(&msg, db); 
                break;
            case return_book: 
                msg.type = doReturnBook(&msg, db); 
                break;
            case my_books:  
                msg.type = doMyBooks(&msg, db); 
                break;
            case borrow_history:  
                msg.type = doBorrowHistory(&msg, db); 
                break;
            case Exit:  
                close(sockfd);  
	        pthread_exit(NULL); 
            default:  
                msg.type = fail;  
                strcpy(msg.msg, "未知请求");
        }
        
        // 将处理结果发送回客户端
        send(sockfd, &msg, sizeof(MSG), 0);
    }
    
}


//服务器主函数进行tcp通信的初始化
int main(int argc, char *argv[]) 
{
    if (argc != 3) {
        printf("请输入：可执行文件+ip地址+端口号\n");
        exit(-1);
    }
    
    // 打开数据库连接
    if (sqlite3_open("./book.db", &db) != SQLITE_OK) {
        fprintf(stderr, "DB open fail: %s\n", sqlite3_errmsg(db));
        exit(-1);
    }

    // 初始化数据库表结构
    initDB(db);
    

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {
        AF_INET,  
        htons(atoi(argv[2])),  
        inet_addr(argv[1])  
    };
	
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));

    listen(server_fd, 5);
    printf("服务器启动：%s:%s\n", argv[1], argv[2]);
    
    pthread_t tid; 
    //循环服务器不断接受客户端连接
    while (1) {
        int *client_fd = malloc(sizeof(int));  // 动态分配客户端socket存储
        //阻塞等待客户端连接
        *client_fd = accept(server_fd, NULL, NULL);
        printf("新连接：%d\n", *client_fd);
        pthread_create(&tid, NULL, client_handler, client_fd);
        // 分离线程
        pthread_detach(tid);
    }
    
    sqlite3_close(db);  
    close(server_fd);  
    return 0;
}
