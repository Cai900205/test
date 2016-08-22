#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static void l_trim(char* output, char* input)
{
    assert(input != NULL);
    assert(output != NULL);
    assert(output != input);
    for(NULL; *input != '\0' && isspace(*input); ++input) {
        ;
    }
    strcpy(output,input); 
}

static void a_trim(char* output, char* input)
{
    char *p = NULL;
    assert(input != NULL);
    assert(output != NULL);
    l_trim(output, input);
    for(p = output + strlen(output) - 1; p >= output && isspace(*p); --p) {
        ;
    }
    *(++p) = '\0';
}

int  syslk_parse_keyval(FILE *fp, char* KeyName, char *value)
{
    char buf_o[100], buf_i[100];
    char *buf = NULL;
    char *c;
    char keyname[50];
    char KeyVal[50];
    int ret = -1;
    printf("########begin##\n");
    while( !feof(fp) && fgets(buf_i,100,fp) != NULL) {
        l_trim(buf_o, buf_i);
        if( strlen(buf_o) <= 0 ) {
            continue;	
        }
        buf = NULL;
        buf = buf_o;
        if ( buf[0] == '#') {
            continue;	
        } else {
            if( (c = (char*)strchr(buf, '=')) == NULL ) {
                continue;
            }	
            sscanf( buf, "%[^=|^ |^\t]", keyname );
            if( strcmp(keyname, KeyName) == 0 ) {
                sscanf( ++c, "%[^\n]", KeyVal );
                printf( "%s\n",KeyVal );
                char *KeyVal_o = (char *)malloc(strlen(KeyVal) + 1);
                if(KeyVal_o != NULL) {
                    memset(KeyVal_o, 0, sizeof(KeyVal_o));
                    a_trim(KeyVal_o, KeyVal);
                    if(KeyVal_o && strlen(KeyVal_o) > 0) {
                        strcpy(value, KeyVal_o);
                    }
                    free(KeyVal_o);
                    KeyVal_o = NULL;
                    ret = 0;
                    break;
                }
            }
        }
        
    }
    return ret;
}

static void syslk_parse_ips_mode(char *filename)
{
    FILE *fp = NULL;
    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf(stderr, "can not open %s", filename);
        return;
    } 
    fseek(fp, 0L, SEEK_SET);
    int ret = -1;
    uint8_t value;
    char data[100];
    ret =  syslk_parse_keyval(fp, "dma_use_nlwr", data);
    value = atoi(data);
    printf("ret:%d dma_nlwr_mode:%d\n", ret, value); 
    fseek(fp, 0L, SEEK_SET);
    
    printf("end\n"); 
    ret =  syslk_parse_keyval(fp, "dma_use_chain", data);
    value = atoi(data);
    printf("ret:%d dma_use_chain:%d\n", ret, value); 
    fseek(fp, 0, SEEK_SET);
    ret =  syslk_parse_keyval(fp, "tx_use_wptr", data);
    value = atoi(data);
    printf("tx_use_wptr%d\n", value); 
    fseek(fp, 0, SEEK_SET);
    ret =  syslk_parse_keyval(fp, "tx_use_rptr", data);
    value = atoi(data);
    printf("tx_use_rptr%d\n", value); 
    fseek(fp, 0, SEEK_SET);
    ret =  syslk_parse_keyval(fp, "rx_use_wptr", data);
    value = atoi(data);
    printf("rx_use_wptr%d\n", value); 
    fseek(fp, 0, SEEK_SET);
    ret =  syslk_parse_keyval(fp, "rx_use_rptr", data);
    value = atoi(data);
    printf("rx_use_rptr%d\n", value);
    
    fclose(fp);

}

int main(int argc, char **argv)
{
    syslk_parse_ips_mode("./init.conf");
    return 0;  
}
