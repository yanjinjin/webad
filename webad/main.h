#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <netdb.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <net/if.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter.h>
#include "libnetfilter_queue.h"


#include "list.h"
#include "mpool.h"
#include "msocket.h"
#include "mstring.h"
#include "acsmx.h"
#include "regexp.h"
#include "task.h"
#include "plug.h"
#include "thpool.h"
#include "util.h"

#define PRIVATE static 
#define OK		0
#define ERROR   -1

#define BUFSIZE 2048
#define PKT_LEN 1500
#define COMM_MAX_LEN 256
#define MAX_PATTERN_NUM 20

typedef enum 
{
	HTTP_TYPE_OTHER,
	HTTP_TYPE_REQUEST_POST,
	HTTP_TYPE_REQUEST_GET,
	HTTP_TYPE_RESPONSE
	
}HTTP_TYPE;

typedef enum 
{
	HTTP_RESPONSE_TYPE_OTHER,
	HTTP_RESPONSE_TYPE_CHUNKED,
	HTTP_RESPONSE_TYPE_CONTENTLENGTH
	
}HTTP_RESPONSE_TYPE;


struct http_hdr
{
	HTTP_TYPE http_type;
	HTTP_RESPONSE_TYPE res_type;
	char error_code[COMM_MAX_LEN];
	char uri[COMM_MAX_LEN];
	char host[COMM_MAX_LEN];
	char accept_encoding[COMM_MAX_LEN];
	char accept[COMM_MAX_LEN];
	char accept_charset[COMM_MAX_LEN];
	char accept_language[COMM_MAX_LEN];
	char authorization[COMM_MAX_LEN];
	char cache_control[COMM_MAX_LEN];
	char connection[COMM_MAX_LEN];
	char content_encoding[COMM_MAX_LEN];
	char content_language[COMM_MAX_LEN];
	char content_length[COMM_MAX_LEN];
	char content_type[COMM_MAX_LEN];
	char content_range[COMM_MAX_LEN];
	char user_agent[COMM_MAX_LEN];
	char transfer_encoding[COMM_MAX_LEN];	
};

struct _skb
{
	struct list_head list;
	int pkt_id;
	int pload_len;
	unsigned char* pload;
	struct iphdr *iph;
	struct tcphdr *tcp;	
	char* http_head;
	char* http_data;
	struct http_hdr hhdr;
	int iph_len;
	int ip_len;
	int tcph_len;
	int tcp_len;
	int httph_len;
	int http_len;
};

struct http_conntrack
{
	struct list_head list;
	unsigned long sip,dip,seq,ack_seq;
	int http_len;
	char host[COMM_MAX_LEN];
	char is_handle;
	long last_time;;
};


#endif

