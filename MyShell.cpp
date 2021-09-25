//程序名：MyShell.cpp
//作者: 曾帅 3190105729
//功能: 程序的主框架，并且拥有命令的执行过程和信号处理
#include "Buildin.h"
using namespace std;

int main(int argc, char **argv)
{
    //初始化进程列表
    JobsInit();
    //初始化信号处理
    signalInit();
    //如果是文件处理指令，需要单独处理
    if(argc != 1)
    {
        //打开脚本文件进行处理
        fstream fp;
        fp.open(argv[1], ios::in);
        //添加环境变量
        rootpath = getcwd(NULL, 0);
        setenv("SHELL", rootpath.c_str(), 1);
        string tmp;
        while (getline(fp, tmp))
        {
            cout << "[root@MyShell " << getcwd(NULL, 0) << "]$" << tmp << endl;
            if(tmp == "exit")
                exit(0);
            //返回一组命令包含管道
            vector<CMD> ins1 = paser(tmp);
            RUN(ins1);
        }
        return 0;
    }    
    //输出进入程序提示语
	cout << "Welcome To MyShell" << endl;
    //增加环境变量
    rootpath = getcwd(NULL, 0);
    setenv("SHELL", rootpath.c_str(), 1);
	//输出命令提示符(包含当前路径)
    string cmd;
	cout << "[root@MyShell " << getcwd(NULL, 0) << "]$";
    //读取一行命令
	getline(cin, cmd);
	while (cmd != "exit")
	{
        //由于一行可能由多条命令组成，用一个容器来处理
        vector<CMD> ins;
        //返回一组命令包含管道
        ins = paser(cmd);  
        RUN(ins);   
        cout << "[root@MyShell " << getcwd(NULL, 0) << "]$";
		getline(cin, cmd);
	}
	cout << "Have a Good Day, Bye" << endl;
	return 0;
}

vector<CMD> paser(string& in)
{
    vector<CMD> ret;                    //返回当前读入的命令，构造函数将会进行初始化
    vector<string> tmp;                 //用以存放临时的分割结果
    const char* delim = " \n\t";        //以空格、换行和table作为分割符
    //获取分割出来的第一个字符
    char* splited = strtok(const_cast<char*>(in.c_str()), delim);
    while(splited)
    {
        //若分割出来的字符不是空的，加入string数组
        tmp.push_back(splited);
        splited = strtok(NULL, delim);
    }

    CMD* R = new CMD();
    for(int i = 0; i < tmp.size(); i++)
    {
        //如果当前字符是"<"意味着需要输入重定向，且下一个就是文件位置
        if(tmp[i] == "<")
        {
            R->reIn = true;
            R->InPath = tmp[++i];
        }
        //如果当前字符是">"意味着需要输出重定向，且下一个就是文件位置
        else if(tmp[i] == ">")
        {
            R->reOut = true;
            R->OutPath = tmp[++i];
        }
        //如果当前字符是">>"意味着需要输出重定向，且下一个就是文件位置
        else if(tmp[i] == ">>")
        {
            R->reApp = true;
            R->OutPath = tmp[++i];
        }
        //如果当前字符是"&"意味着需要后台运行
        else if(tmp[i] == "&")
        {
            R->IsBG = true;
        }
        //如果当前字符是"|"意味着需要存在管道，且后面的内容属于下一条命令
        else if(tmp[i] == "|")
        {
            R->pipOut = true;       //此命令需要管道输出
            ret.push_back(*R);      //将此命令存入命令容器
            R = new CMD();          //建立新命令(CMD类带有初始化的构造函数)
            R->pipIn = true;        //新命令应该接受管道作为输入
        }
        //普通关键此压入命令类的关键字容器
        else
            R->word.push_back(tmp[i]);
    }
    ret.push_back(*R);
    return ret;
}

//根据单条命令执行内建指令，若不是内建指令返回false
bool execute(CMD& cmd)
{
    if(cmd.word.empty())
    {
        fprintf(stderr, "命令不合法");
        exit(1);
    }
    else if(cmd.word[0] == "bg")
        BG();
    else if(cmd.word[0] == "fg")
        FG();
    else if(cmd.word[0] == "jobs")
        JOBS();
    else if(cmd.word[0] == "cd")
        CD(cmd.word);
    else if(cmd.word[0] == "pwd")
        PWD();
    else if(cmd.word[0] == "clr")
        CLR();
    else if(cmd.word[0] == "time")
        TIME();
    else if(cmd.word[0] == "umask")
        UMASK(cmd.word);
    else if(cmd.word[0] == "dir")
        DIR_(cmd.word);
    else if(cmd.word[0] == "set")
        SET();
    else if(cmd.word[0] == "echo")
        ECHO(cmd.word);
    else if(cmd.word[0] == "exec")
        EXEC(cmd.word);
    else if(cmd.word[0] == "test")
        TEST(cmd.word);
    else if(cmd.word[0] == "shift")
        SHIFT(cmd.word);
    else if(cmd.word[0] == "unset")
        UNSET(cmd.word);
    //处理失败说明可能是外部命令，返回false
    else
        return false;
    //如果成功处理返回true
    return true;
}

void RUN(vector<CMD>& ins)
{
    //如果是help指令，需要使用more来组织
    if(ins[0].word[0] == "help")
    {
        ins[0].word[0] = "more";
        string helppath = rootpath + "/help.txt";
        ins[0].word.push_back(helppath);
    }
    //若是内建指令与管道和重定向无关的进程单独执行
    if(ins[0].word[0] == "umask" || 
       ins[0].word[0] == "cd"    || 
       ins[0].word[0] == "exec"  || 
       ins[0].word[0] == "jobs"  || 
       ins[0].word[0] == "fg"    || 
       ins[0].word[0] == "unset" ||
       ins[0].word[0] == "clr" ||
       ins[0].word[0] == "bg"      )
    {
        execute(ins[0]);
        return;
    }
    //创建新的进程以执行命令
	pid_t pid = fork();
    //创建失败
	if(pid < 0) 
    {
		fprintf(stderr, "进程创建失败");
        exit(1);
	}
	//子进程主要涉及的操作是命令的执行
	else if(pid == 0) 
    {
        //等待进程表的添加
        usleep(1);
        //主进程进程表添加完毕才可以继续运行
        while(!jobs[findjob(getpid())].IsRUN)
            usleep(1);
        //如果没有管道，则直接执行命令即可
		if(!ins[0].pipIn && !ins[0].pipOut) 
        {
            //判断有无输入重定向
			if(ins[0].reIn) 
            {
				close(0);
				int Fd = open(ins[0].InPath.c_str(), O_RDONLY);
			}
            //判断有无输出重定向
			if(ins[0].reOut) 
            {
				close(1);
				int Fd2 = open(ins[0].OutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
			}
            if(ins[0].reApp)
            {
                close(1);
                int Fd2 = open(ins[0].OutPath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
            }
            //如果命令不是内建命令，则需要调用execvp执行外部命令
            if(!execute(ins[0]))
            {
                //将string类型命令转为execvp参数
                char* arglist[ins[0].word.size()];
                for(int i = 0; i < ins[0].word.size(); i++)
                    arglist[i] = const_cast<char*>(ins[0].word[i].c_str());
                //在末尾补充NULL(execvp函数要求)
                arglist[ins[0].word.size()] = NULL;
                if(!execvp(ins[0].word[0].c_str(), arglist))
                    fprintf(stderr, "此命令不存在!");
            }
            //恢复标准输入输出
            close(0);
            close(1);
		}
        //有管道，需要执行一组命令
		else 
        {
            int i;
			for(i = 0; i < ins.size() - 1; i++)
            {
                //创建新进程以执行管道命令
				pid_t pid2 = fork();
				if(pid2 < 0) 
                {
					fprintf(stderr, "进程创建失败");
					exit(1);
				}
                //子进程负责执行和输入输出重定向
				else if(pid2 == 0) 
                {
                    //只有第一个命令不需要从共享文件读取数据
					if(i) 
                    {
                        close(0);
					    int Fd = open(f, O_RDONLY); //输入重定向
                    }
                    //输入重定向
                    if(ins[i].reIn) 
                    {
                        close(0);
                        int Fd = open(ins[i].InPath.c_str(), O_RDONLY);
                    }
                    //输出重定向(覆盖)
					if(ins[i].reOut) 
                    {
                        close(1);
                        int Fd = open(ins[i].OutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    }
                    //输出重定向(追加)
                    if(ins[i].reApp)
                    {
                        close(1);
                        int Fd = open(ins[i].OutPath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
                    }	
                    close(1);
                    //解引用
                    remove(f);
                    //输出到管道共享文件
                    int Fd = open(f, O_WRONLY | O_CREAT | O_TRUNC, 0666);
					//执行外部命令
                    if(!execute(ins[i]))
                    {
                        char* arglist[ins[i].word.size()];
                        for(int j = 0; j < ins[i].word.size(); j++)
                            arglist[j] = const_cast<char*>(ins[i].word[j].c_str());
                        arglist[ins[i].word.size()] = NULL;
                        if(!execvp(ins[i].word[0].c_str(), arglist))
                            fprintf(stderr, "此命令不存在!");
                    }
				}
                //父进程等待子进程结束
				else 
					waitpid(pid2, NULL, 0);
			}
            //最后一条命令无需管道输出，单独执行
			close(0);
            //最后一条命令需要管道文件输入
			int Fd = open(f, O_RDONLY);
            //输出重定向
			if(ins[i].reOut) 
            {
                close(1);
                int Fd = open(ins[i].OutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            }
            if(ins[i].reApp)
            {
                close(1);
                int Fd = open(ins[i].OutPath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
            }	
            //执行命令
            if(!execute(ins[i]))
            {
                //如果是外部命令，调用execvp
                char* arglist[ins[i].word.size()];
                for(int j = 0; j < ins[i].word.size(); j++)
                    arglist[j] = const_cast<char*>(ins[i].word[j].c_str());
                arglist[ins[i].word.size()] = NULL;
                
                if(!execvp(ins[i].word[0].c_str(), arglist))
                    fprintf(stderr, "此命令不存在!");
            }
            //恢复标准输入输出
            close(0);
            close(1);
		}
	}
	//原始父进程负责进程操作
	else 
    {
        //在进程表中建立新的进程信息
        int i = (jobs[0].jobcnt++);
        jobs[i].pid = pid;
        strcpy(jobs[i].name, ins[0].word[0].c_str());
        jobs[i].IsRUN = false;
        jobs[i].IsFG = !ins[ins.size() - 1].IsBG;
        jobs[i].IsDone = false;
        //环境变量中需要增加父进程信息
        setenv("PARENT", rootpath.c_str(), 1);
        //后台进程输出进程信息
        if(ins[ins.size() - 1].IsBG)
        {
            //更新后台进程数目，并设置唯一的后台进程号
            jobs[i].bgcnt = (++jobs[0].bgcnt);
            printf("[%d] %d\n", jobs[i].bgcnt, pid);
        }
        //将信号传给子进程，可以开始执行此命令
        jobs[i].IsRUN = true;

        //只有当前指令是前台命令时才等待它运行完，并从进程表中删除
        if(!ins[ins.size() - 1].IsBG)
        {
            //等待子进程执行完毕(CTRLD)
            waitpid(pid, NULL, WUNTRACED);
            //前台进程结束，在进程表中删除进程信息
            if (jobs[i].IsRUN)
                Deljob(pid);
        }
        //如果是后台命令
        else
            waitpid(pid, NULL, WNOHANG);    //不阻塞主进程
    }
}

//从进程表中找到对应pid的进程
int findjob(pid_t pid)
{
    int ret;
    //遍历查找
    for(ret = 0; ret < jobs[0].jobcnt; ret++)
        if(pid == jobs[ret].pid)
            break;
    if(ret != jobs[0].jobcnt)
        return ret;
    else
        return -1;
}

//从进程表中删除进程
void Deljob(pid_t pid)
{
    int i = findjob(pid);
    //找到了此进程
    if(i != -1)
    {
        //记录状态
        bool IsBG = !jobs[i].IsFG;
        for( ; i < jobs[0].jobcnt - 1; i++)
        {
            //如果要删除的进程原本是后台进程，则要使得后续后台进程的后台进程序号更新
            if(IsBG && !jobs[i+1].IsFG)
                jobs[i+1].bgcnt--;
            jobs[i] = jobs[i+1];
        }
        jobs[0].jobcnt--;
        //如果删除的是后台进程，后台进程总数减一
        if(IsBG)
            jobs[0].bgcnt--;
    }
}

//初始化信号处理
void signalInit()
{
    //捕获中断信号并进行处理
    signal(SIGTSTP, SUSPEND);
    signal(SIGSTOP, SUSPEND);
    //将act结构全部清零
    memset(&act, 0, sizeof(act));
    //部署GHLD信号处理函数
    act.sa_sigaction = CHLDHAND;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    //SIGCHLD信号处理
    sigaction(SIGCHLD, &act, &old_act);
}

void SUSPEND(int sign)
{
    if(jobs[jobs[0].jobcnt - 1].pid != 0)
    {
        //向此进程发送中止信号
        kill(jobs[jobs[0].jobcnt - 1].pid, SIGSTOP);
        //将其设为后台暂停进程
        jobs[jobs[0].jobcnt - 1].IsRUN = false;
        jobs[jobs[0].jobcnt - 1].IsFG = false;
        jobs[jobs[0].jobcnt - 1].IsDone = false;
        //更新后台进程序号
        jobs[jobs[0].jobcnt - 1].bgcnt = (++jobs[0].bgcnt);
        //输出提示信息
        cout << endl;
        cout << "[" << jobs[jobs[0].jobcnt - 1].bgcnt << "]" << "    Stopped    " << jobs[jobs[0].jobcnt - 1].name << endl;
    }
}

//捕获SIGCHLD信号执行的函数
void CHLDHAND(int sig_no, siginfo_t* info, void* vcontext)
{
    if(BGCONT)
    {
        BGCONT = false;
        return;
    }
    //获取发送信号的进程
    int i = findjob(info->si_pid);
    //若存在此进程进一步处理
    if(i != -1)
    {
        //当此子进程后台指令且之前正在运行，状态更改为已完成
        if(!jobs[i].IsFG && jobs[i].IsRUN)
           jobs[i].IsDone = true;
        //如果此进程为后台暂停进程不予处理
        else if(!jobs[i].IsFG && !jobs[i].IsRUN);
        //否则删除此进程
        else
            Deljob(jobs[i].pid);
    }
}