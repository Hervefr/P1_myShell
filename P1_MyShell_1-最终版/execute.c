#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <stdio.h>

#include "global.h"
#define DEBUG
int goon = 0, ingnore = 0;       //用于设置signal信号量
char *envPath[10], cmdBuff[40];  //外部命令的存放路径及读取外部命令的缓冲空间
History history;                 //历史命令
Job *head = NULL;                //作业头指针
pid_t fgPid;                     //当前前台作业的进程号

int pos1, pos2;
char* wcbuff[100];
char* argbuf[200];//命令参数数组
int argcnt = 0;//命令参数数目
int sigflag = 0;

void wildcardBuff(char *args);
int executePipeCmd(int argc, char** argv, int back);///处理管道的函数声明
int do_simple_cmd(int argc, char** argv, int prefd[], int postfd[], int back);
int getCmdStr();
int file_exist(const char* file, char* buffer);
int posWildcard(char *str);
void exeWildcard(char *args);
char *getPath(int pos, char *str);
char *substring(char *str, int start, int end);
int matchStr(char *pat, char *s, int whole);

/*******************************************************
                  工具以及辅助方法
********************************************************/
/*判断命令是否存在*/
int exists(char *cmdFile){
    int i = 0;
    if((cmdFile[0] == '/' || cmdFile[0] == '.') && access(cmdFile, F_OK) == 0){ //命令在当前目录
        strcpy(cmdBuff, cmdFile);
        return 1;
    }else{  //查找ysh.conf文件中指定的目录，确定命令是否存在
        while(envPath[i] != NULL){ //查找路径已在初始化时设置在envPath[i]中
            strcpy(cmdBuff, envPath[i]);
            strcat(cmdBuff, cmdFile);
            
            if(access(cmdBuff, F_OK) == 0){ //命令文件被找到
                return 1;
            }
            
            i++;
        }
    }
    
    return 0; 
}

/*将字符串转换为整型的Pid*/
int str2Pid(char *str, int start, int end){
    int i, j;
    char chs[20];
    
    for(i = start, j= 0; i < end; i++, j++){
        if(str[i] < '0' || str[i] > '9'){
            return -1;
        }else{
            chs[j] = str[i];
        }
    }
    chs[j] = '\0';
    
    return atoi(chs);
}

/*调整部分外部命令的格式*/
void justArgs(char *str){
    int i, j, len;
    len = strlen(str);
    
    for(i = 0, j = -1; i < len; i++){
        if(str[i] == '/'){
            j = i;
        }
    }

    if(j != -1){ //找到符号'/'
        for(i = 0, j++; j < len; i++, j++){
            str[i] = str[j];
        }
        str[i] = '\0';
    }
}

/*设置goon*/
void setGoon(){
    goon = 1;
}

/*释放环境变量空间*/
void release(){
    int i;
    for(i = 0; strlen(envPath[i]) > 0; i++){
        free(envPath[i]);
    }
}

/*******************************************************
                  信号以及jobs相关
********************************************************/
/*添加新的作业*/
Job* addJob(pid_t pid){
    Job *now = NULL, *last = NULL, *job = (Job*)malloc(sizeof(Job));
    
	//初始化新的job
    job->pid = pid;
    strcpy(job->cmd, inputBuff);
    strcpy(job->state, RUNNING);
    job->next = NULL;
    
    if(head == NULL){ //若是第一个job，则设置为头指针
        head = job;
    }else{ //否则，根据pid将新的job插入到链表的合适位置
		now = head;
		while(now != NULL && now->pid < pid){
			last = now;
			now = now->next;
		}
        last->next = job;
        job->next = now;
    }
    
    return job;
}

/*移除一个作业*/
void rmJob(int sig, siginfo_t *sip, void* noused){
    pid_t pid;
    Job *now = NULL, *last = NULL;
    
    if(ingnore == 1){
        ingnore = 0;
        return;
    }
    
    pid = sip->si_pid;

    now = head;
	while(now != NULL && now->pid < pid){
		last = now;
		now = now->next;
	}
    
    if(now == NULL){ //作业不存在，则不进行处理直接返回
        return;
    }
    
	//开始移除该作业
    if(now == head){
        head = now->next;
    }else{
        last->next = now->next;
    }
    
    free(now);
}
/*组合键命令ctrl+c*/
void ctrl_c(){
	Job *now = NULL;
	Job *p = NULL;
	ingnore = 1;    //SIGCHLD信号产生自ctrl+c
	if (fgPid == 0){ //前台没有作业则直接返回
		return;
	}
	now = head;
	p = head;
	while (now != NULL && now->pid != fgPid){
		p = now;
		now = now->next;
	}
	if (now == NULL){ //未找到前台作业，则根据fgPid添加前台作业
		now = addJob(fgPid);
	}
	if (now == head)head = head->next;
	else p->next = now->next;
	kill(fgPid, SIGSTOP);
	printf("\ndelete success\n");
	fgPid = 0;
	return;
}
/*组合键命令ctrl+z*/
void ctrl_Z(){
	Job *now = NULL;
	ingnore = 1;    //SIGCHLD信号产生自ctrl+z
	if (fgPid == 0){ //前台没有作业则直接返回
		return;
	}


	now = head;
	while (now != NULL && now->pid != fgPid)
		now = now->next;

	if (now == NULL){ //未找到前台作业，则根据fgPid添加前台作业
		now = addJob(fgPid);
	}

	//修改前台作业的状态及相应的命令格式，并打印提示信息
	strcpy(now->state, STOPPED);
	now->cmd[strlen(now->cmd)] = '&';
	now->cmd[strlen(now->cmd) + 1] = '\0';
	printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);

	//发送SIGSTOP信号给正在前台运作的工作，将其停止
	kill(fgPid, SIGSTOP);
	fgPid = 0;
	return;
}

/*fg命令*/          
void fg_exec(int pid){  
    Job *now = NULL; 
	int i;
    sigset_t sigs;//定义信号集
    sigemptyset(&sigs);//空信号集
    sigaddset(&sigs,SIGSTOP);///屏蔽ctrl_Z引发的信号
    //SIGCHLD信号产生自此函数
    ingnore = 1;
    
	//根据pid查看作业
    now = head;
	while(now != NULL && now->pid != pid)
		now = now->next;
    
    if(now == NULL){ ///未找到作业
        printf("The work whose pid is 7%d doesn't exsit!\n", pid);
        return;
    }

    //记录前台作业的pid,修改作业状态
    fgPid = now->pid;
    strcpy(now->state, RUNNING);
    
	signal(SIGINT, ctrl_c);
	signal(SIGTSTP, ctrl_Z); //设置signal信号，为下次按下Ctrl+z做准备
   i = strlen(now->cmd) - 1;
    while(i >= 0 && now->cmd[i] != '&')
		i--;
    now->cmd[i] = '\0';
    
    printf("%s\n", now->cmd);
    kill(now->pid, SIGCONT); ////向对象信号发出SIGCONTi信号，使其进行运行
	sigsuspend(&sigs);//屏蔽SIGSTOP信号
	waitpid(now->pid, NULL, 0); ///父进程等待前台进程
}

/*bg命令*/
void bg_exec(int pid){
    Job *now = NULL;
    
    //SIGCHLD信号产生自此函数
    ingnore = 1;
    
	//根据pid查找作业
	now = head;
    while(now != NULL && now->pid != pid)
		now = now->next;
    
    if(now == NULL){ //未找到作业
        printf("The work whose pid is 7%d doesn't exsit！\n", pid);
        return;
    }
    
    strcpy(now->state, RUNNING); //修改对象作业的状态
    printf("[%d]\t%s\t\t%s\n", now->pid, now->state, now->cmd);
    
    kill(now->pid, SIGCONT); //向对象作业发送SIGCONT信号，使其运行
}

/*******************************************************
                    命令历史记录
********************************************************/
void addHistory(char *cmd){
    if(history.end == -1){ //第一次使用history命令
        history.end = 0;
        strcpy(history.cmds[history.end], cmd);
        return;
	}
    
    history.end = (history.end + 1)%HISTORY_LEN; //end前移一位
    strcpy(history.cmds[history.end], cmd); //将命令拷贝到end指向的数组中
    
    if(history.end == history.start){ //end和start指向同一位置
        history.start = (history.start + 1)%HISTORY_LEN; //start前移一位
    }
}

/*******************************************************
                     初始化环境
********************************************************/
/*通过路径文件获取环境路径*/
void getEnvPath(int len, char *buf){
    int i, j, last = 0, pathIndex = 0, temp;
    char path[40];
    
    for(i = 0, j = 0; i < len; i++){
        if(buf[i] == ':'){ //将以冒号(:)分隔的查找路径分别设置到envPath[]中
            if(path[j-1] != '/'){
                path[j++] = '/';
            }
            path[j] = '\0';
            j = 0;
            
            temp = strlen(path);
            envPath[pathIndex] = (char*)malloc(sizeof(char) * (temp + 1));
            strcpy(envPath[pathIndex], path);
            
            pathIndex++;
        }else{
            path[j++] = buf[i];
        }
    }
    
    envPath[pathIndex] = NULL;
}

/*初始化操作*/
void init(){
    int fd, n, len;
    char c, buf[80];

	//打开查找路径文件ysh.conf
    if((fd = open("ysh.conf", O_RDONLY, 660)) == -1){
        perror("init environment failed\n");
        exit(1);
    }
    
	//初始化history链表
    history.end = -1;
    history.start = 0;
    
    len = 0;
	//将路径文件内容依次读入到buf[]中
    while(read(fd, &c, 1) != 0){ 
        buf[len++] = c;
    }
    buf[len] = '\0';

    //将环境路径存入envPath[]
    getEnvPath(len, buf); 
    
    //注册信号
    struct sigaction action;
    action.sa_sigaction = rmJob;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
	signal(SIGINT, ctrl_c);
	signal(SIGTSTP, ctrl_Z);
}

/*******************************************************
                      命令解析
********************************************************/
SimpleCmd* handleSimpleCmd(int argc,char**argv,int back){
	int i, j, k, h;
    int fileFinished; //记录命令是否解析完毕
    char c, buff[10][40], inputFile[30], outputFile[30], *temp = NULL;
    SimpleCmd *cmd = (SimpleCmd*)malloc(sizeof(SimpleCmd));

	//默认为非后台命令，输入输出重定向为null
    cmd->isBack = 0;
    cmd->input = cmd->output = NULL;
    
    //初始化相应变量
    for(i = 0; i<10; i++){
        buff[i][0] = '\0';
    }
    inputFile[0] = '\0';
    outputFile[0] = '\0';
	k=0;
	for(i=0;i<argc;i++)
	{
		if(strcmp(argv[i],"<")==0)
		{
			strcpy(inputFile,argv[++i]);
			continue;
		}
		if(strcmp(argv[i],">")==0)
		{
			strcpy(outputFile,argv[++i]);
			continue;
		}
		strcpy(buff[k++],argv[i]);
	}
//依次为命令名及其各个参数赋值
	cmd->args = (char**)malloc(sizeof(char*) * 1000);

	for (i = h = 0; i < k; i++){
		wildcardBuff(buff[i]);
		while (pos1 <= pos2){
			j = strlen(wcbuff[pos1]);
			cmd->args[h] = (char*)malloc(sizeof(char) * (j + 1));
			strcpy(cmd->args[h], wcbuff[pos1]);
			pos1++;
			h++;
		} 
    }
	cmd->args[h] = NULL;

	if(back==1)
	{
		cmd->isBack=1;
	}
	//如果有输入重定向文件，则为命令的输入重定向变量赋值
    if(strlen(inputFile) != 0){
        j = strlen(inputFile);
        cmd->input = (char*)malloc(sizeof(char) * (j + 1));
        strcpy(cmd->input, inputFile);
    }

    //如果有输出重定向文件，则为命令的输出重定向变量赋值
    if(strlen(outputFile) != 0){
        j = strlen(outputFile);
        cmd->output = (char*)malloc(sizeof(char) * (j + 1));   
        strcpy(cmd->output, outputFile);
    }
    #ifdef DEBUG
	printf("****\n");
	printf("isBack: %d\n", cmd->isBack);
    for(i = 0; cmd->args[i] != NULL; i++){
		printf("args[%d]: %s\n", i, cmd->args[i]);
	}
	printf("input: %s\n", cmd->input);
	printf("output: %s\n", cmd->output);
	printf("****\n");
    #endif
    return cmd;
}

/*得到目标路径*/
char *getPath(int pos, char *str)
{
	int i, slash;
	char *tmp;
	slash = -1;

	for (i = pos - 1; i >= 0; i--){
		if (str[i] == '/'){
			slash = i;
			break;
		}
	}

	if (slash < 0)
		return ".";
	else if (slash == 0)
		return "/";
	else{
		tmp = (char *)malloc(sizeof(char)*(slash + 2));
		strncpy(tmp, str, slash);
		tmp[slash + 1] = '\0';
	}

	return tmp;
}

/*截取字符串*/
char *substring(char *str, int start, int end)
{
	int i;
	char *sub;

	if (end < start)
		return "\0";

	sub = (char *)malloc(sizeof(char)*(end - start + 2));

	for (i = 0; i <= end - start; i++)
		sub[i] = str[start + i];

	sub[end - start + 1] = '\0';

	return sub;
}

/*通配符位置*/
int posWildcard(char *str)
{
	int pos;

	for (pos = 0; str[pos] != '\0'; pos++)
		if (str[pos] == '*' || str[pos] == '?')
			return pos;

	return -1;
}

void wildcardBuff(char *args)
{
	pos1 = pos2 = 0;
	wcbuff[pos1] = (char *)malloc(sizeof(char)*strlen(args));
	strcpy(wcbuff[pos1], args);

	while (posWildcard(wcbuff[pos1]) >= 0 && pos1 <= pos2){
		exeWildcard(wcbuff[pos1]);
		pos1++;
	}
}

/*通配符操作**/
void exeWildcard(char *args)
{
	int i, pos, slash1, slash2, len, start, end, n;
	char *path, *file_name, *tmp, *buff_end, *buff_start;
	DIR *d;
	struct dirent *dirent;

	pos = posWildcard(args);
	slash1 = slash2 = -1;
	len = strlen(args);
	path = getPath(pos, args);

	if (pos >= 0){
		len = strlen(args);
		path = getPath(pos, args);
		d = opendir(path);

		for (i = pos - 1; i >= 0; i--){
			if (args[i] == '/'){
				slash1 = i;
				break;
			}
		}

		for (i = pos + 1; args[i] != '\0'; i++){
			if (args[i] == '/'){
				slash2 = i;
				break;
			}
		}

		start = slash1 >= 0 ? slash1 + 1 : 0;
		end = slash2 >= 0 ? slash2 - 1 : len - 1;
		tmp = substring(args, start, end);

		if (d){
			while (dirent = readdir(d)){
				file_name = dirent->d_name;
				if (matchStr(tmp, file_name, 1)){
					buff_start = substring(args, 0, start - 1);
					buff_end = substring(args, end + 1, len - 1);
					n = strlen(file_name) + start + len - end - 1;
					wcbuff[pos2 + 1] = (char *)malloc(sizeof(char)*n);
					strcpy(wcbuff[pos2 + 1], buff_start);
					strcat(wcbuff[pos2 + 1], file_name);
					strcat(wcbuff[pos2 + 1], buff_end);
					pos2++;
				}
			}

		}
	}
}

/*匹配字符串**/
int matchStr(char *pat, char *s, int whole)
{
	int n, i;
	switch (*pat) {
	case 0:
		return !*s;
	case '*':
		if (whole && *s == '.')
			return 0;
		if (!pat[1])
			return 1;
		n = strlen(s);
		for (i = 0; i<n; i++)
			if (matchStr(pat + 1, s + i, 0))
				return 1;
		return 0;
	case '?':
		if (!*s || whole && *s == '.')
			return 0;
		return matchStr(pat + 1, s + 1, 0);
	default:
		if (*pat != *s)
			return 0;
		return matchStr(pat + 1, s + 1, 0);
	}
}

/*******************************************************
                      命令执行
********************************************************/
/*执行外部命令*/
void execOuterCmd(SimpleCmd *cmd){
    pid_t pid;
    int pipeIn, pipeOut;
    
    if(exists(cmd->args[0])){ //命令存在

        if((pid = fork()) < 0){
            perror("fork failed");
            return;
        }
        
        if(pid == 0){ //子进程
            if(cmd->input != NULL){ //存在输入重定向
                if((pipeIn = open(cmd->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
                    printf("can't open the file %s！\n", cmd->input);
                    return;
                }
                if(dup2(pipeIn, 0) == -1){
                    printf("redirect standard input is wrong！\n");
                    return;
                }
            }
            
            if(cmd->output != NULL){ //存在输出重定向
                if((pipeOut = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
                    printf("can't open the file %s！\n", cmd->output);
                    return ;
                }
                if(dup2(pipeOut, 1) == -1){
                    printf("redirect standard output is wrong！\n");
                    return;
                }
            }
            
			if (cmd->isBack){ //若是后台运行命令，等待父进程增加作业
				//	printf("back......NO.1\n");
				/*            signal(SIGUSR1, setGoon); //收到信号，setGoon函数将goon置1，以跳出下面的循环
							while(goon == 0) ; //等待父进程SIGUSR1信号，表示作业已加到链表中
							goon = 0; //置0，为下一命令做准备
							*/         while (sigflag == 0);
			sigflag=0;
       //         printf("[%d]\t%s\t\t%s\n", getpid(), RUNNING, inputBuff);
	//	sigflagsb=1;
      //          kill(getppid(), SIGUSR1);
			}
            
            justArgs(cmd->args[0]);
            if(execv(cmdBuff, cmd->args) < 0){ //执行命令
                printf("execv failed!\n");
                return;
            }
        }
		else{ //父进程
            if(cmd ->isBack){ //后台命令   
	//	printf("back.......NO.2\n");          
                fgPid = 0; //pid置0，为下一命令做准备
                addJob(pid); //增加新的作业
         //       kill(pid, SIGUSR1); //子进程发信号，表示作业已加入
                sigflag=1;

                //等待子进程输出
	//	while(sigflagsb==0);
	//	sigflagsb=0;
          /*      signal(SIGUSR1, setGoon);
                while(goon == 0) ;
                goon = 0;	*/
            }else{ //非后台命令
                fgPid = pid;
                waitpid(pid, NULL, 0);
            }
		}
    }else{ //命令不存在
        printf("can't find the command 15%s\n", inputBuff);
    }
}

/*执行命令*/
void execSimpleCmd(SimpleCmd *cmd){
    int i, pid;
    char *temp;
    Job *now = NULL;
    
    if(strcmp(cmd->args[0], "exit") == 0) { //exit命令
        exit(0);
    } else if (strcmp(cmd->args[0], "history") == 0) { //history命令
        if(history.end == -1){
            printf("execute no command\n");
            return;
        }
        i = history.start;
        do {
            printf("%s\n", history.cmds[i]);
            i = (i + 1)%HISTORY_LEN;
        } while(i != (history.end + 1)%HISTORY_LEN);
    } else if (strcmp(cmd->args[0], "jobs") == 0) { //jobs命令
        if(head == NULL){
            printf("no jobs\n");
        } else {
            printf("index\tpid\tstate\t\tcommand\n");
            for(i = 1, now = head; now != NULL; now = now->next, i++){
                printf("%d\t%d\t%s\t\t%s\n", i, now->pid, now->state, now->cmd);
            }
        }
    } else if (strcmp(cmd->args[0], "cd") == 0) { //cd命令
        temp = cmd->args[1];
        if(temp != NULL){
            if(chdir(temp) < 0){
                printf("cd; %s wrong file or directory！\n", temp);
            }
        }
    } else if (strcmp(cmd->args[0], "fg") == 0) { //fg命令
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            if(pid != -1){
                fg_exec(pid);
            }
        }else{
            printf("fg; argument is illegal，right form：fg %<int>\n");
        }
    } else if (strcmp(cmd->args[0], "bg") == 0) { //bg命令
        temp = cmd->args[1];
        if(temp != NULL && temp[0] == '%'){
            pid = str2Pid(temp, 1, strlen(temp));
            
            if(pid != -1){
                bg_exec(pid);
            }
        }
		else{
            printf("bg; argument is illegal，right form：bg %<int>\n");
        }
    } else{ //外部命令
        execOuterCmd(cmd);
    }
    
    //释放结构体空间
    for(i = 0; cmd->args[i] != NULL; i++){
        free(cmd->args[i]);
        free(cmd->input);
        free(cmd->output);
    }
}

/*******************************************************
                     命令执行接口
********************************************************/
void execute(){
	int back;
	back = getCmdStr();
	executePipeCmd(argcnt, argbuf, back);
}


int executePipeCmd(int argc, char** argv,int back)//back:0-前台，1-后台
{
	int i = 0;
	int j = 0;
	int prepipe = 0;
	int prefd[2];
	int postfd[2];
	char* p;

	while(argv[i]) {
		if(strcmp(argv[i], "|") == 0) { // pipe
			p = argv[i];
			argv[i] = 0;

			pipe(postfd); 		//create the post pipe		
			if(prepipe)	
				do_simple_cmd(i-j, argv+j, prefd, postfd,back);
			else
				do_simple_cmd(i-j, argv+j, 0, postfd,back);//0表示来自标志输入
			argv[i] = p;
			prepipe = 1;
			prefd[0] = postfd[0];//后管道变成了前管道
			prefd[1] = postfd[1];
			j = ++i;
		} else
			i++;
	}
	if (prepipe)
		do_simple_cmd(i - j, argv + j, prefd, 0, back);
	else
		do_simple_cmd(i - j, argv + j, 0, 0, back);
	return 0;
}


int do_simple_cmd(int argc, char** argv, int prefd[], int postfd[],int back)
{
	int pipeIn,pipeOut;
	SimpleCmd *cmd;
	int pid;
	int status;
	if(argc == 0)//表示需要处理的字符个数
		return 0;
	cmd=handleSimpleCmd(argc,argv,back);

	if(prefd == 0 && postfd == 0) {//说明这是一个shell内部命令,处理方法与之前。
		execSimpleCmd(cmd);
		return 0;
	}
	
	//否则是管道命令
	if((pid = fork())<0)
	{
		printf("pipe error\n");
	}
	else if(pid== 0) {//child		
		if(cmd->input==NULL && prefd) {//has a pre pipe, redirect stdin；在没有输入重定向的情况下从前管道中读数
			close(prefd[1]);//关闭管道的写段
			if(prefd[0] != STDIN_FILENO) {
	//			fprintf(stderr, "redirect stdin\n");
				dup2(prefd[0], STDIN_FILENO);//把前管道的读端复制给标志输入。
				close(prefd[0]);
			}
		}

		if(cmd->output==NULL && postfd) {//has a post pipe, redirect stdout
			close(postfd[0]);
			if(postfd[1] != STDOUT_FILENO) {
	//			fprintf(stderr, "redirect stdout\n");
				dup2(postfd[1], STDOUT_FILENO);
				close(postfd[1]);
			}
		}
		if(cmd->input != NULL){ //存在输入重定向
                	if((pipeIn = open(cmd->input, O_RDONLY, S_IRUSR|S_IWUSR)) == -1){
                   	 printf("can't open the file %s！\n", cmd->input);
                    	return;
               		}
                	if(dup2(pipeIn, 0) == -1){
                  	 printf("redirect standard input is wrong！\n");
                    	return;
                	}
           	 }           
            	if(cmd->output != NULL){ //存在输出重定向
                	if((pipeOut = open(cmd->output, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) == -1){
                    	printf("can't open the file %s！\n", cmd->output);
                   	return ;
               		 }
                	if(dup2(pipeOut, 1) == -1){
                   	 printf("redirect standard output is wrong！\n");
                   	 return;
               		 }
   		 }
		char buffer[100];
	//	cmd=handleSimpleCmd(argc,argv,back);
		if(file_exist(argv[0], buffer)) {
	//		fprintf(stderr, "exec command %s\n", buffer);
			execv(buffer,cmd->args);/////////@@@@@@@@@@@@@@@@@@@@@@
		}
		else {
			fprintf(stderr, "-msh: %s: command not found\n", argv[0]);
			exit(0);
		}

	//	execSimpleCmd(cmd);//处理完管道之后，剩下的就是按照普通命令执行。
	}
	else//父进程
	{
		waitpid(pid, &status, 0);
		if(postfd) { // no
			close(postfd[1]); // must close this fd here.
		}
	}
	return 0;
}

int file_exist(const char* file, char* buffer)
{
	int i = 0;
	const char* p;
	const char* path;
	path = getenv("PATH");
	//PATH???
	p = path;
	while(*p != 0) {
		if(*p != ':')
			buffer[i++] = *p;
		else {
			buffer[i++] = '/';
			buffer[i] = 0;
			strcat(buffer, file);
			if(access(buffer, F_OK) == 0)
				return 1;
			i = 0;
		}
		p++;
	}
	return 0;
}

int getCmdStr()
{
	int i, j, k, len;
	char ch, *tmp, buff[20][40];
	len = strlen(inputBuff);
	tmp = NULL;

//跳过无用信息
	for (i = 0; i < len && (inputBuff[i] == ' ' || inputBuff[i] == '\t'); i++);

	k = j = 0;
	tmp = buff[k];
	while (i < len){
		switch (inputBuff[i]){
		case ' ':
		case '\t':
			tmp[j] = '\0';
			j = 0;
			k++;
			tmp = buff[k];
			break;

		case '>':
		case '<':
		case '&':
		case '|':
			if (j != 0){
				tmp[j] = '\0';
				k++;
				tmp = buff[k];
				j = 0;
			}
			tmp[j] = inputBuff[i];
			tmp[j + 1] = '\0';
			j = 0;
			k++;
			tmp = buff[k];
			i++;
			break;

		default:
			tmp[j++] = inputBuff[i++];
			continue;
		}

//跳过无用信息
		while (i < len &&
			(inputBuff[i] == ' ' ||
			inputBuff[i] == '\t'))
			i++;
	}

	if (inputBuff[len - 1] != ' '&&
		inputBuff[len - 1] != '\t'&&
		inputBuff[len - 1] != '&'){
		tmp[j] = '\0';
		k++;
	}

	argbuf[k] = NULL;

	for (i = 0; i < k; i++){
		j = strlen(buff[i]);
		argbuf[i] = (char *)malloc(sizeof(char)*(j + 1));
		strcpy(argbuf[i], buff[i]);
	}
	argcnt = k;
	if(k>0)
	{
		if(strcmp(argbuf[k-1],"&")==0)
		{
			argbuf[k-1]=0;
			argcnt--;
			return 1;
		}
	}
	return 0;
}
