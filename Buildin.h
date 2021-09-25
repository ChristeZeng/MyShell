#include "MyShell.h"
using namespace std;

vector<string> split(string& cmd)
{                 
    vector<string> ret;                 //返回当前读入的命令，构造函数将会进行初始化
    const char* delim = " \n\t";        //以空格、换行和table作为分割符
    //获取分割出来的第一个字符
    char* splited = strtok(const_cast<char*>(cmd.c_str()), delim);
    while(splited)
    {
        //若分割出来的字符不是空的，加入string数组
        ret.push_back(splited);
        splited = strtok(NULL, delim);
    }
    return ret;
}

//输出当前目录
void PWD() { cout << getcwd(NULL, 0) << endl; }
//清屏
void CLR() { cout << "\e[1;1H\e[2J"; }
void CD(vector<string>& word)
{
    //没有参数，输出当前目录
    if(word.size() == 1)
        PWD();
    else if(word.size() == 2)
    {
        if(chdir(word[1].c_str()))
            fprintf(stderr, "Error:没有此目录!\n");     //改变目录失败，没有此目录
        else
        {
            setenv("PWD", word[1].c_str(), 1);         //CD指令也可以改变PWD环境变量
        }
    }
    else
        fprintf(stderr, "Error:非法输入!(参数过多)\n");
}
//输出当前时间
void TIME()
{
    time_t t = time(0);
    char* date = ctime(&t);
    cout << date;
}
//掩码设置
void UMASK(vector<string>& word)
{
    mode_t old_umask, new_umask = 0;
    //得到原来的掩码
    old_umask = umask(new_umask);
    //设置回去
    umask(old_umask);
    //如果没有参数，输出当前掩码
    if(word.size() == 1)
        cout << setfill('0') << setw(3) << oct << old_umask << endl;
    //否则设置参数与掩码
    else
    {
        try 
        { 
            //字符串转8进制数字
            new_umask = stoi(word[1], nullptr, 8); 
            //设置
            umask(new_umask);
        }
        catch(...) { cout << "ileagal umask!" << endl; }
    }
}
void DIR_(vector<string>& word)
{
    DIR* dp;
    struct dirent *dirp;
    //若没有参数，默认当前环境文件夹
    if(word.size() == 1)
        word.push_back(getcwd(NULL, 0));
    //打开所指路径
    if((dp = opendir(word[1].c_str())) == NULL)
        fprintf(stderr, "Error:Can not open the path!");
    
    while((dirp = readdir(dp)) != NULL)
    {
        //排除隐藏文件
        if(dirp->d_name[0] != '.')
            cout << dirp->d_name << " ";
    }
    cout << endl;
    closedir(dp);
}
//输出环境变量
void SET()
{
    extern char ** environ;
    for(int i = 0; environ[i]; i++)
        cout << environ[i] << endl;   
}
//输出参数
void ECHO(vector<string>& word)
{
    for(int i = 1; i < word.size(); i++)
        cout << word[i] << " ";
    cout << endl; 
}
//执行外部命令
void EXEC(vector<string>& word)
{
    //转化参数，存放在char*数组中
    char* arglist[word.size()];
    for(int i = 1; i < word.size(); i++)
        arglist[i - 1] = const_cast<char*>(word[i].c_str());
    //末尾一定要是NULL
    arglist[word.size() - 1] = NULL;
    //执行外部命令
    if(!execvp(word[1].c_str(), arglist))
        cout << "No operation!" << endl;
    //退出程序
    exit(0);
}

//比较字符串与数字
void TEST(vector<string> word)
{
    int res = 0;
    //数字之间比较
    if(word[2] == "-eq")
        res = (stoi(word[1], nullptr, 10) == stoi(word[3], nullptr, 10));
    else if(word[2] == "-ne")
        res = (stoi(word[1], nullptr, 10) != stoi(word[3], nullptr, 10));
    else if(word[2] == "-gt")
        res = (stoi(word[1], nullptr, 10) > stoi(word[3], nullptr, 10));
    else if(word[2] == "-ge")
        res = (stoi(word[1], nullptr, 10) >= stoi(word[3], nullptr, 10));
    else if(word[2] == "-lt")
        res = (stoi(word[1], nullptr, 10) < stoi(word[3], nullptr, 10));
    else if(word[2] == "-le")
        res = (stoi(word[1], nullptr, 10) <= stoi(word[3], nullptr, 10));
    //字符串之间比较
    else if(word[2] == "=")
        res = (word[1] == word[3]);
    else if(word[2] == "!=")
        res = (word[1] != word[3]);
    cout << res << endl;
}

void SHIFT(vector<string>& word)
{
    //获取位移大小
    int offset = (word.size() == 1) ? 1 : stoi(word[1], nullptr, 10);
    //输入参数
    char args[MAXCMD];
    fgets(args, MAXCMD, stdin);
    string tmp = args;
    vector<string> argments = split(tmp);
    //移动参数并输出
    for(int i = offset; i < argments.size(); i++)
        cout << argments[i] << " ";
    cout << endl;
}
//置空环境变量
void UNSET(vector<string> word)
{
    if(word.size() == 2)
    {
        if(word[1] == "pwd" || word[1] == "status")
            return;
        char* env = getenv(word[1].c_str());
        setenv(word[1].c_str(), "", 1);
    }
}
//初始化进程表
void JobsInit()
{
    int shmid;        //共享内存标识符
    //创建共享内存
    if((shmid = shmget((key_t)1234, sizeof(jobs) * MAXJOB, 0666 | IPC_CREAT)) == -1)
    {    
        fprintf(stderr, "共享内存创建失败");
        exit(EXIT_FAILURE);
    }
    //将共享内存连接到当前进程的地址空间
    void* shm = NULL;
    if((shm = shmat(shmid, 0, 0)) == (void*)-1)
    {
        fprintf(stderr, "共享内存连接失败");
        exit(EXIT_FAILURE);
    }
    //获取内存地址作为进程表
    jobs = (job*) shm;
    //将首个进程设为myshell
    jobs[0].pid = 0;
    jobs[0].jobcnt = 1;
    strcpy(jobs[0].name, "MyShell");
    jobs[0].IsFG = false;
    jobs[0].IsRUN = true;
    jobs[0].bgcnt = 0;
}

void FG()
{
    //找到最近的后台进程
    int renct = -1;
    for(int i = 1; i < jobs[0].jobcnt; i++)
    {
        if(!jobs[i].IsFG)
            renct = i;
    }
    //没找到返回错误信息
    if(renct == -1)
        fprintf(stderr, "MyShell: fg: current: no such job\n");
    else
    {
        //输出进程名
        cout << jobs[renct].name << endl;
        //从后台进程表中删除
        int i = findjob(jobs[renct].pid);
        //找到了此进程
        if(i != -1)
        {
            for( ; i < jobs[0].jobcnt - 1; i++)
            {
                //如果要删除的进程原本是后台进程，则要使得后续后台进程的后台进程序号更新
                if(!jobs[i+1].IsFG)
                    jobs[i+1].bgcnt--;
            }
            //如果删除的是后台进程，后台进程总数减一
            jobs[0].bgcnt--;
        }
        //如果此进程本身就是运行的，转到前台运行即可
        if(jobs[renct].IsRUN)
        {
            jobs[renct].IsFG = true;
            //阻塞主进程
            waitpid(jobs[renct].pid, NULL, 0);
            
        }
        //如果是后台暂停的，转到前台运行
        else
        {
            jobs[renct].IsFG = true;
            jobs[renct].IsRUN = true;
            //继续运行进程
            kill(jobs[renct].pid, SIGCONT);
            //阻塞主进程
            waitpid(jobs[renct].pid, NULL, 0);
        }
    }
}

void BG()
{
    //找到最近的后台暂停进程
    for(int i = 0; i < jobs[0].jobcnt; i++)
    {
        if(!jobs[i].IsRUN && !jobs[i].IsFG && !jobs[i].IsDone)
        {
            jobs[i].IsRUN = true;
            jobs[i].IsFG = false;
            BGCONT = true;
            //继续运行此进程
            kill(jobs[i].pid, SIGCONT);
            //输出信息
            cout << "[" << jobs[i].bgcnt << "] " << jobs[i].name << " &" << endl;
        }
    }
    //如果没有后台进程，输出错误信息
    if(!jobs[0].bgcnt)
        fprintf(stderr, "MyShell: bg: current: no such job\n");
}

//jobs显示所有的后台进程
void JOBS()
{
    //记录要删除的进程(不能中途删除)
    vector<pid_t> delpid;
    for(int i = 1; i < jobs[0].jobcnt; i++)
    {
        //找到后台运行的
        if(!jobs[i].IsFG)
        {
            //如果是之前运行结束的
            if(jobs[i].IsDone)
            {
                cout << "[" << jobs[i].bgcnt << "]" << "    " << setw(6) << jobs[i].pid << "    DONE       " << jobs[i].name << endl;
                //将要进程表删除
                delpid.push_back(jobs[i].pid);
            }
            //暂停的
            else if(!jobs[i].IsRUN)
                cout << "[" << jobs[i].bgcnt << "]" << "    " << setw(6) << jobs[i].pid << "    SUSPEND    " << jobs[i].name << endl;
            else
                cout << "[" << jobs[i].bgcnt << "]" << "    " << setw(6) << jobs[i].pid << "    RUNNING    " << jobs[i].name << endl;
        }
    }
    //统一从进程表中删除已完成的进程(jobs只会显示一次)
    for(int i = 0; i < delpid.size(); i++)
        Deljob(delpid[i]);
}