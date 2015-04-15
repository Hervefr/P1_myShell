作者：何雨
功能：增加对管道命令的支持

改动1：bison.y，修改了语法分析函数和词法分析函数，以支持管道
改动2：global.h，添加了两个全局变量：
	char* argbuf[200];//命令参数数组
	int argcnt = 0;//命令参数数目
改动3：execute.c，修改了execute()函数，修改了handleSimpleCmdStr函数，增加了do_simple_cmd和executePipeCmd函数

