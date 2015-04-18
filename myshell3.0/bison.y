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
                  ������Ϣִ�к���
****************************************************************/
void yyerror()
{
    printf("not the right command\n");
}

/****************************************************************
                  main������
****************************************************************/
int main(int argc, char** argv) {
    int i;
    char c;

    init(); //��ʼ������
    commandDone = 0;
    
    printf("shz@computer:%s$$$$ ", get_current_dir_name()); //��ӡ��ʾ����Ϣ

   while(1){
        i = 0;
        while((c = getchar()) != '\n'){ //
            inputBuff[i++] = c;
        }
        inputBuff[i] = '\0';

        len = i;
        offset = 0;

        yy_scan_string( inputBuff) ;

        yyparse(); //�����﷨�����������ú�����yylex()�ṩ��ǰ����ĵ��ʷ���

        if(commandDone == 1){ //�����Ѿ�ִ����ɺ������ʷ��¼��Ϣ
            commandDone = 0;
            addHistory(inputBuff);
        }
        
        printf("shz@computer:%s$$$$ ", get_current_dir_name()); //��ӡ��ʾ����Ϣ
     }

    return (EXIT_SUCCESS);
}
