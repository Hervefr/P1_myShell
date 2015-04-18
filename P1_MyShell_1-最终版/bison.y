%{
    #include "global.h"

    void yyerror ();
      
    int offset,len, commandDone;
%}

%token AMP 
%token STRING
%token PIPE
%token FROM
%token INTO
%token NEWLINE


%%
line            :   /* empty */
                    |command               {   execute();  commandDone = 1;   }
;

command         :   fgCommand
                    |fgCommand '&'
;


fgCommand		:	pipeCmd
;
	
pipeCmd			:   simpleCmd
					|simpleCmd '|' pipeCmd
;

simpleCmd       :   progInvocation inputRedirect outputRedirect
;

progInvocation  :   STRING args
;

inputRedirect   :   /* empty */
                    |'<' STRING
;

outputRedirect  :   /* empty */
                    |'>' STRING
;

args            :   /* empty */
                    |args STRING
;

%%



/****************************************************************
                  错误信息执行函数
****************************************************************/
void yyerror()
{
    printf("not the right command\n");
}

/****************************************************************
                  main主函数
****************************************************************/
int main(int argc, char** argv) {
    int i;
    char c;

    init(); //初始化环境
    commandDone = 0;
    
    printf("OGK@computer:%s$$$$ ", get_current_dir_name()); //打印提示符信息

   while(1){
        i = 0;
        while((c = getchar()) != '\n'){ //
            inputBuff[i++] = c;
        }
        inputBuff[i] = '\0';

        len = i;
        offset = 0;

        yy_scan_string( inputBuff) ;

        yyparse(); //调用语法分析函数，该函数由yylex()提供当前输入的单词符号

        if(commandDone == 1){ //命令已经执行完成后，添加历史记录信息
            commandDone = 0;
            addHistory(inputBuff);
        }
        
        printf("OGK@computer:%s$$$$ ", get_current_dir_name()); //打印提示符信息
     }

    return (EXIT_SUCCESS);
}
