#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>
#include <ctime>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <iomanip>
#include <dirent.h>
#include <sys/shm.h>
#include <wait.h>
#include <fcntl.h>
#include <signal.h>
#include <iomanip>
using namespace std;
//全局常量定义
#define MAXCMD 512  //最大命令长度
#define MAXJOB 128  //最多进程数
#define MAXNAM 32   //最大进程名长度
//进程类定义
class job
{
public:
    pid_t pid = 0;      //进程号
    char name[MAXNAM];  //进程名
    bool IsFG;          //是否为前台运行
    bool IsRUN;         //是否正在运行
    bool IsDone;        //是否运行结束
    int jobcnt = 0;     //总进程数
    int bgcnt = 0;      //后台进程数
};
//进程表定义
job* jobs;
//命令类定义
class CMD
{
public:
    vector<string> word;    //命令及参数
    bool IsBG;              //命令是否为后台运行
    bool pipIn;             //命令是否有管道输入
    bool pipOut;            //命令是否有管道输出
    bool reIn;              //重定向输入标记
    bool reOut;             //重定向输出(覆盖)
    bool reApp;             //重定向输出(覆盖)
    string InPath;          //重定向输入路径
    string OutPath;         //重定向输出路径
    //初始化
    CMD()
    {
        IsBG = false;
        pipIn = false;
        pipOut = false;
        reIn = false;
        reOut = false;
        reApp = false;
        InPath = "";
        OutPath = "";
    }
};
//后台继续执行的信号标记
bool BGCONT = false;
//程序路径
string rootpath;
//充当管道的中间文件
char f[32] = "temp.txt";
//信号类
struct sigaction act;
struct sigaction old_act;

//分割函数，将命令做分割
vector<CMD> paser(string& in);
//执行内建命令，若输入命令不是内建命令则返回false
bool execute(CMD& cmd);
//执行输入的一行新命令
void RUN(vector<CMD>& ins);
//从进程表中找到对应pid的进程
int findjob(pid_t pid);
//从进程表中删除进程
void Deljob(pid_t pid);
//初始化信号处理
void signalInit();
//处理Ctrl+Z(暂停)信号
void SUSPEND(int sign);
//处理SIGCHLD信号
void CHLDHAND(int sig_no, siginfo_t* info, void* vcontext);