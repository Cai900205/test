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
        /*ע�ⲻ����get_cmd(s),��Ϊp��ŵĲ��ǡ�s��ŵĵ�ַ��������
         s����ĵ�ַ����ϸ�鿴�����׸㶨Cָ�롱�ĵ�4ƪ��
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
    /*������һ��ָ��ʵ�֣�����ֻ����ʾָ��ָ���ָ�봫��*/
    void get_cmd(char **p)
    {
        fgets(*p,100,stdin);
    }

    /*�����пո���ַ���s���н�������ŵ�cmd�ַ�������*/

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
                /*�������ո�����*/
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

    /*�������*/

    void print_array(char _arr[][100],int _len)
    {
        int i = 0;
        for(i = 0; i < _len ; i++)
        {
            printf("%s\n",_arr[i]);
        }
    }
