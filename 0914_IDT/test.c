     #include <stdio.h>
    #include <malloc.h>
    #include <string.h>

    void get_cmd(char **p);
    int split_string(char *s,char _cmd[][100]);
    void print_array(char _arr[][100],int _len);
        
    int main()
    {
        char *s;
        char (*cmd)[100];
        int len;
        s = (char *)malloc(sizeof(char)*100);
        cmd = (char (*)[])malloc(sizeof(s));
        /*注意不能用get_cmd(s),因为p存放的不是“s存放的地址”，而是
         s本身的地址。详细查看“彻底搞定C指针”的第4篇。
        */
        while(1)
        {
           printf("IDT_OP>");
           get_cmd(&s);
           len = split_string(s,cmd);
           print_array(cmd,len);
           if(!strncmp(cmd[0],"q",1))
            break;
        }
    }
    /*可以用一个指针实现，这里只是演示指向指针的指针传参*/
    void get_cmd(char **p)
    {
        fgets(*p,100,stdin);
    }

    /*将带有空格的字符串s进行解析，存放到cmd字符数组中*/

    int split_string(char *s,char _cmd[][100])
    {
        char *p = s;
        int i = 0;
        int j = 0;
        while(*p != '\n')
        {
            if(*p == ' ')
            {
                _cmd[i][j]='\0';
                i++;
                j = 0;
                p++;
                /*处理多个空格的情况*/
                while(*p == ' ')
                {
                    p++;
                }
            }
            else
            {
                _cmd[i][j] = *p;
                p++;
                j++;
            }
        }
        return i+1;
    }

    /*输出数组*/

    void print_array(char _arr[][100],int _len)
    {
        int i = 0;
        for(i = 0; i < _len ; i++)
        {
            printf("%s\n",_arr[i]);
        }
    }
