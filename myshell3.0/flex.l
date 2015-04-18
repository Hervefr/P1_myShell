%{
	#include "bison.tab.h"
	int flag=0;

void insert(char a[], int i,char *t)
{
	int len = strlen(t);
	int k=0;
	for(;k<=len-1;k++)
	{
		a[i]=t[k];
		i++;
	}
}


%}

%%
"<"	{ if(flag==0) return '<';else {flag=0; return STRING;}}
">"	{ if(flag==0) return '>';else {flag=0; return STRING;}}
"&"	{ if(flag==0) return '&';else {flag=0; return STRING;}}
"|"	{ if(flag==0) return '|';else {flag=0; return STRING;}}
[ \t]	{}
[^\n\t]+ { return STRING; }
%%

