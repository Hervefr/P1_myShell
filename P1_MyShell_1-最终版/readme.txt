作者：何雨
功能：增加对管道命令的支持

改动1：bison.y，修改了语法分析函数和词法分析函数，以支持管道
改动2：global.h，添加了两个全局变量：
	char* argbuf[200];//命令参数数组
	int argcnt = 0;//命令参数数目
改动3：execute.c，修改了execute()函数，修改了handleSimpleCmdStr函数，增加了do_simple_cmd和executePipeCmd函数

#######################第二次修正myshell2.0##########
功能：实现了后台命令
改动：增加了sigflag全局变量，修改了execOuterCmd函数

#######################第三次修正myshell3.0#########
功能：解决了fg函数在处理ctrl+Z后进程的bug
改动：execute.c，修改了fg_exec函数，增加了头文件#include<stdio.h>

#######################shell最终版+第四次修正##################
整合了小组其他人的工作，增加了通配符的支持和ctrl_c命令
第四次修正：修改了重定向和管道同时出现时处理的bug
改动：execute.c，修改了do_simple_cmd函数。
