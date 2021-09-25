## MyShell设计

[TOC]

### 设计思想

#### 设计语言

考虑在实现Unix环境编程方面上，C语言已经有大量可以使用的库和函数，方便直观，而且也有成熟的教程以供学习查阅，C++不仅完整兼容C，还有大量的STL特性方便程序设计，综合考虑下我选择了使用C语言Unix环境编程库的C++进行此次MyShell设计。

#### 主体框架

shell程序都是按照读入命令->命令语法分析->执行命令->读入命令这个循环流程进行操作的，在读入命令这一部分，我选择了C++的string类型以方便后续的分割和语法分析处理，读入命令之后，使用strok函数对此命令行以空格与Tab为分隔进行分割，之后将进入语法分析阶段，此阶段主要负责重定向、管道和后台执行的识别与参数列表的生成，最后在执行命令，执行完毕后进行下一次命令的读取。

### 功能与模块

本程序主要分为：读取输入模块，分割与语法分析模块，执行命令模块和信号处理模块

#### 读取输入模块

在本程序中，如果在执行时就带有脚本名作为参数，也即main函数的参数存在，那么程序将直接读入参数文件中的命令并执行，执行到文件最后退出程序，不会接受用户输入。而如果在调用本程序时没有参数，那么本程序会输出命令提示符等待并提示用户输入。

#### 语句分割与分析模块

在本程序中，读取命令是一次读取缓冲区的一行内容，读取后使用strok函数对读入命令进行简单的分割处理，分割出的每个单词会按照原顺序存放在类型为string的容器中，为了分析这一行命令，程序还会遍历整个分割结果容器，分析产生一个命令类(CMD)容器，为什么要产生一个类容器而不是类呢？因为一行命令可能是由管道命令组成的多条linux命令构成，所以根据分割的结果，可能产生不止一条命令。

本程序顺序读取分割结果，如果遇到普通单词，将其push进CMD类的单词容器中，作为命令名或参数，如果遇到"<"，">"，">>"这类重定向字符时，会将CMD类中的对应重定向标志置为true，并且记录下重定向文件的路径，如果遇到"&"，会将命令的后台运行标记置为true，如果遇到"|"，说明下一个单词不再属于本条命令，于是将本条命令的管道输出标志置为true，push进入语法分析结果容器，之后在新建一个类接受新命令参数，并将新命令的接受管道输入的标志置为true，重复上述过程。

#### 执行命令模块

有了前面的工作做铺垫，本次实验执行命令模块会显得直观很多，拿到一个语法分析结果(也就是命令容器)之后，首先判断有无与重定向和管道操作无关的内建命令，这类命令可以直接在主进程中执行，不用建立新进程。

而一旦涉及到有重定向和管道选项的命令，本程序都建立一个新的进程，在子进程中再判断有无管道选项，无管道选项说明是单命令执行，再执行前判断输入输出重定向，内建指令使用Bulidin.h中的函数执行，外部指令使用execvp函数执行即可；如果有管道命令，意味这命令容器含有多条命令，且命令之间需要管道通信，这时需要循环建立新进程并在新进程中执行命令，直到将命令容器中的所有命令执行完毕，此时原本的进程就用于维护管道通信共享文件temp.txt。

在父进程中主要就是建立当前命令的进程表信息，如果此条进程是后台执行，父进程不会等待子进程执行完毕，而前台命令父进程则会被阻塞。

#### 信号处理模块

信号处理模块主要是负责对挂起当前进程的STGSTP信号与子进程结束的SIGCHLD信号做出响应，挂起进程需要更改进程表的状态并且暂停进程，而子进程结束信号也是为了更新进程表，防止僵尸进程的产生，具体阐述在用户手册的后台进程部分说明。

### 数据结构与算法

在本次程序中，主要有两个类需要说明，一个是组成进程表的job类，一个是表示一条命令及其各种属性的CMD命令类，在详细的注解中已经说明了每个变量的作用。

```c++
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
```

需要注意的是job*进程表被存放在shmget()申请到的共享内存中。

```c++
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
```

### 用户手册

#### I/O重定向

实现I/O重定向的方法比较容易，只需要把标准读入或标准输出替换为对应文件读入或对应文件输出即可，既可以使用open()函数打开需要重定向的文件，再使用dup2()函数进行文件流的替换；也可以使用close()函数关闭原标准输入/输出，再使用open()函数打开重定向文件。检测是否需要重定向也比较简单，在命令需要分析时，加入对">"，"< " ，" >> "的识别，当识别到这些符号时，为这条命令加上需要对应操作的信号。

```c++
//输入重定向
close(0);
open(InPath, O_RDONLY);
//输出重定向(覆盖)
close(1);
open(OutPath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
//输出重定向(追加)
close(1);
open(OutPath, O_RDWR | O_CREAT | O_APPEND, 0666);
```

#### 管道操作

管道本质上就是实现进程之间的通信，其中匿名管道主要用于实现父进程与子进程之间的通信，若是只有简单的二级管道的情形("CMD1 | CMD 2")，我们可以简单的利用父子进程之间的匿名管道通信实现，具体实现方式是首先fork一个实现CMD2命令的子进程，在此子进程中再fork一个实现CMD1的子进程，并且利用waitpid函数使得只有当CMD1进程执行完才会去执行CMD2，执行完之后使用dup2函数将CMD1的输出重定向到管道，并将CMD2的输入重定向到管道

```c
pid_t pid1 = fork()
//子进程
if(pid1 == 0)
{
    //执行CMD1
}
//父进程
else if(pid1 > 0)
{
    waitpid(pid1, NULL, 0);
    pid_t pid2 = fork()
    if(pid2 == 0)
    {
     	//执行CMD2   
    }
}
```

但这个方法在处理多级管道，也就是"CMD1 | CMD2 | CMD3 ..."的形式时会遇到较大的麻烦，第一是难以组织dup2的重定向，第二是管道输入输出端的开闭问题容易出错，实现多级管道的一个简便易行的方法是使用一个自行创建的共享文件来存储中间结果，过程上类似于二级管道，只不过最开始fork的父进程用于创建和读写共享中间文件，此父进程不断递归调用fork来建立和回收新进程，例如调用fork建立执行CMD1的进程1，输出写入共享中间文件，回收进程1后再fork新进程执行CMD2，并且从共享文件读取输入，再将结果输出到共享文件，整个过程就可以通过共享文件简单完成。总体的多级管道实现过程如下所示

```c++
pid_t pid = fork()
//子进程
if(pid == 0)
{
    for(从管道的第一条命令到非最后一条)
        第一条的输入需要从标准输入获取
        打开管道文件以读取或输出
    最后一条指令需要输出到标准输出
}
//主进程
else
    增加进程表
    改变环境
    waitpid等待子进程结束
```

#### 程序环境

使用C/C++语言进行此次实验，程序环境的管理便利就是一大优势，C/C++允许使用库来调用Linux系统保存的环境变量，并且是全局变量，允许程序的各个部分与各个进程使用和修改，通过getenv()函数我们可以获取当前环境变量，setenv()函数可以修改环境变量。

#### 后台程序执行

后台程序的执行首先离不开的就是进程表，而且此进程表是要能够被整个程序的所有子进程共同维护和修改的，所以仅仅将其设为全局的远远不够，因为这样只能被子进程继承，但子进程进行的修改不能影响到父进程，相当于没有修改进程表的信息，所以在此实验中，我使用了shmget这一系列函数申请与维护一个共享内存，将进程表存放在共享内存中，这样上述的问题就得以解决。

有了进程表，命令分析时读取到"&"符号就将当前的进程置上后台执行的标记，使用fork...exec框架执行它时，主进程不再使用waitpid的阻塞主进程选项，而是使用waitpid不会阻塞子进程的WNOHANG选项，当前父进程退出后，子进程可能仍在运行，当然只让子进程继续运行是远远不够的，我们还需知道它在后台何时结束，以便更新进程表同时避免僵尸进程出现。在这里我利用了子进程结束后会发出一个SIGCHLD信号的特点，通过捕捉SIGCHLD信号和后台运行的标记这两大特征，实现了后台程序的运行。

当然后台程序不止包括使用"&"将其送入后台的进程，还包括使用CTRL+Z挂起在后台暂停的进程，我们可以识别STGSTP信号后，使用kill达到暂停的目的，将进程暂挂在后台之后，我们可以使用两种指令将其调出，一种是使其在后台继续执行的bg命令，一种是将其调入前台执行的fg命令，两种命令都依靠kill向挂起的进程发送SIGCONT信号实现，唯一不同的是，使用fg指令还会使用waitpid阻塞主进程，使主进程等待此进程的完成，而bg无需阻塞，直接在后台完成即可。

#### 内建命令解释

1. bg命令
解释：后台执行一个挂起的后台进程
范例：bg
2. cd命令
解释：无参数显示当前目录，有参数切换文件目录并更改环境变量
范例：cd    
范例：cd \<dir\>
3. clr命令
解释：清理当前屏幕内容
范例：clr
4. dir命令
解释：无参数显示当前文件夹内容，有参数显示参数文件夹内容
范例：dir   
范例：dir \<dir\>
5. echo命令
解释：显示参数内容
范例：echo Hello World
6. exec命令
解释：使用参数命令代替当前进程
范例：exec ls -l
7. exit
解释：退出shell
范例：exit
8. fg
解释：将后台进程转到前台执行
范例：fg
9. help
解释：输出用户手册并使用more进行控制
范例：help
10. jobs
解释：显示所有的后台进程及其状态
范例：jobs
11. pwd
解释：显示当前路径
范例：pwd
12. set
解释：显示所有的环境变量
范例：set   
13. shift
解释：读入参数并左移参数位后输出，无参数表示默认左移1位
范例：shift    
范例：shift 3
14. test
解释：比较字符串或数字之间的大小关系是否成立
范例：test aaa = aaa
范例：test bbb != aaa
范例：test 1 -eq 1
范例：test 1 -ne 2
范例：test 1 -lt 1
范例：test 1 -gt 1
范例：test 1 -le 1
范例：test 1 -ge 1
15. time
解释：输出当前系统时间
范例：time
16. umask
解释：无参数时显示当前掩码，有参数则设置掩码为参数
范例：umask
范例：umask 0666
17. unset
解释：删除参数对应的环境变量
范例：unset SHELL

### 测试

#### 进入程序

不带任何参数进入程序后显示提示语， 并且输出当前路径

![image-20210816212806495](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816212813.png)

#### 执行文件

如果带有文件参数执行程序

![image-20210818010222213](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210818010222.png)

#### exit命令

输入exit命令，程序立马退出，并输出退出提示语

![image-20210816213049534](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816213049.png)

#### dir命令

若dir命令没有带参数，列出当前文件夹下的内容

![image-20210816213901234](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816213901.png)

与系统终端结果对比可知功能正常

![image-20210816214219837](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816214219.png)

如dir有一个参数，列出对应文件夹下的内容

![image-20210816214337729](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816214337.png)

同样与系统终端对比

![image-20210816214400036](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816214400.png)

#### echo命令

在屏幕上显示\<comment\>并换行(多个空格和制表符可能被缩减为一个空格)。

![image-20210816215300590](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816215300.png)

#### exec命令

执行命令并退出当前shell，可以看到利用exec执行完命令后，已经回到了系统终端

![image-20210816215134666](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816215134.png)

#### pwd命令

显示当前目录。

![image-20210816220100502](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816220100.png)

#### set命令

若set不带参数，则是列出所有的环境变量，shell 的环境变量也包含在了其中(截图仅摘取前面部分)

![image-20210816224646538](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816224646.png)

#### unset命令

删除对应的环境变量，从图中可以看到SHELL环境变量变为了空值

![image-20210816224938754](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816224938.png)

#### time命令

显示当前时间

![image-20210816220141308](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816220141.png)

#### shift命令

左移变量，参数为左移的位数，默认为1

![image-20210816225320749](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816225320.png)

shift命令主要与管道或者重定向联合使用，我们使用管道和重定向来验证它的正确性

![image-20210819111241542](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210819111248.png)

#### test命令

test命令主要提供字符串与数字的比较，结果用0表示false，1表示true

![image-20210816230228115](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816230228.png)

#### clr命令

在上述操作结果的基础上输入clr命令，可以看见终端被清屏

![image-20210816230503665](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210816230503.png)

#### jobs命令

jobs命令用于显示后台进程信息，包含运行中/暂停/运行完成三种状态，当执行sleep执行时输出Ctrl+Z后台挂起程序，利用jobs查看后台

![image-20210817115740435](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817115747.png)

再运行两个后台程序，利用jobs查看后台

![image-20210817115955565](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817115955.png)

当执行的后台进程完成时，会将进程状态改为DONE，jobs能在第一次查看到它。如下面所示，sleep后台进程结束

![image-20210817120730251](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817120730.png)

#### fg命令

在此程序中fg命令用于将后台暂停进程或后台运行进程放到前台执行，同样以sleep指令为例，首先是后台执行的进程sleep 10，运行fg后会到前台运行。执行完毕后输出命令提示符。

![image-20210817221601107](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817221601.png)

![image-20210817221708065](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817221708.png)

同样的在执行sleep 10进程时将其通过Ctrl+Z挂起，通过jobs指可以看出它已经处于后台暂停状态，执行fg会将其调到前台继续执行，执行完毕后已经没有此后台进程。

![image-20210817222258496](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817222258.png)

![image-20210817222427535](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817222427.png)

下面也是一个典型的例子，将进程挂起之后，使用bg命令使其在后台继续执行，立马使用fg将其调到前台

![image-20210819120524225](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210819120524.png)

#### bg命令

bg命令在此程序中是将后台暂定的进程继续执行，同样使用sleep命令来进行测试，将sleep 10命令通过ctrl+Z挂起后，利用jobs命令查到此进程后台暂停，使用bg命令，返回继续执行的后台暂停进程信息，此时立马使用jobs查看后台进程，可以看到sleep进程已经在运行，等到执行完毕后，使用jobs指令可以看到此进程已经执行完毕的信息，并且再执行一次jobs确认其已经从进程表中删除。

![image-20210819111740064](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210819111740.png)

#### cd命令

把当前默认目录改变为\<directory\>。如果没有\<directory\>参数，则显示当前目录。如该目录不存在，会出现合适的错误信息。这个命令也可以改变PWD 环境变量。从下图可以看到不带参数的cd输出了当前的环境变量，而cd /root更改当前目录到了/root(从命令提示符就可直观看出)，再次调用cd和pwd检查当前环境变量是否被改变，可以看到cd改变了PWD环境变量。

同时cd命令还对目录不存在与参数数目不对两种错误情况做了提示。

![image-20210817224828091](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817224828.png)

#### umask命令

显示当前掩码或者修改掩码，无参数时显示当前掩码，有参数则设置掩码为参数。

![image-20210817233715182](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817233715.png)

#### help命令

help命令会输出已经写好的用户手册并使用more进行组织，使用空格可以输出下一个，回车键输出下一行，具体组织方式如下图所示

![image-20210819112512113](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210819112512.png)

#### 外部命令

通过fork...exec框架，此程序还实现了对外部命令的支持，下面举几条常见命令为例

![image-20210817233742355](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817233742.png)

其他的命令行输入被解释为程序调用，shell 创建并执行这个程序，并作为自己的子进程。程序的执行的环境变量包含父进程环境变量，通过调用set命令查看环境变量可以看到正确信息，符合要求。

![image-20210819114601418](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210819114601.png)

#### 重定向

首先是对内建命令重定向的支持，以dir, set(environ), echo, help为例，测试重定向输出，通过下述几天命令我分别测试了覆盖与追加的重定向输出方式，结果摘取关键部分给出

![image-20210817233927752](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817233927.png)

![image-20210817233953298](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817233953.png)

![image-20210817234027991](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817234028.png)

通过wc指令我们可以更加直观的测试输入输出重定向

![image-20210817234405315](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817234405.png)

同时我们还可以测试管道的输入输出重定向

![image-20210817234641581](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817234641.png)

#### 管道

本程序不仅仅支持二级管道，还支持多级管道，通过对ls -l操作结果的一系列筛选与统计过程我们可以验证管道操作的正确性。

![image-20210817234912520](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210817234912.png)

#### 后台执行

除开前面所说的"&"实现后台执行以外，本程序还支持管道命令的后台执行，可以看到下一次的命令提示符输出后结果才输出。

![image-20210819112927031](https://raw.githubusercontent.com/ChristeZeng/Picture/main/img/20210819112927.png)

### 源代码

#### Makefile

```makefile
CC = g++ -std=c++11

myshell: MyShell.cpp MyShell.h Buildin.h
	$(CC) -o myshell MyShell.cpp MyShell.h Buildin.h
```

#### MyShell.h

```c++
//程序名：MyShell.h
//作者: 曾帅 3190105729
//功能: 程序的主框架的头文件，拥有全局变量定义与函数定义
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
```

#### MyShell.cpp

```c++
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
```

#### Bulidin.h

```c++
//程序名：Buildin.h
//作者: 曾帅 3190105729
//功能: 内建命令的实现
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
```

