#include "main.h"

#define TIMEOUT_HTTP 60*60

struct list_head httpc_list;


///////////////////////////////////////////////////////////////////////

void timeout(void* arg)
{
	struct http_conntrack *httpc_cursor , *httpc_tmp;
	
	long current_sec;
	
	while(1)
	{
		
		sleep(TIMEOUT_HTTP);
		current_sec=get_current_sec();

		list_for_each_entry_safe(httpc_cursor, httpc_tmp, &httpc_list, list)
		{		
			if(current_sec - httpc_cursor->last_time > TIMEOUT_HTTP)
			{
				debug_log("httpc timeout");
				thread_lock();
				la_list_del(&httpc_cursor->list);
				free_page(httpc_cursor);
				httpc_cursor=NULL;
				thread_unlock();
				continue;
			}
			
		}	
	}
}
///////////////////////////////////////////////////////////////////////

struct http_conntrack* find_http_conntrack_by_host(struct _skb *skb)
{
	struct http_conntrack *cursor , *tmp;
	list_for_each_entry_safe(cursor, tmp, &httpc_list, list)
	{
		if(!strcmp(skb->hhdr.host , cursor->host) ||
			(skb->iph->saddr == cursor->sip &&
			skb->iph->daddr == cursor->dip))
		{
			return cursor;
		}
	}	
	return NULL;
}

struct http_conntrack* find_http_conntrack_by_ack(struct _skb *skb)
{
	struct http_conntrack *cursor , *tmp;
	
	list_for_each_entry_safe(cursor, tmp, &(httpc_list), list)
	{		
		if((skb->iph->saddr == cursor->dip &&
			skb->iph->daddr == cursor->sip) &&
			ntohl(skb->tcp->ack_seq) == ntohl(cursor->seq)+cursor->http_len)
		{
			return cursor;
		}
	}	
	return NULL;
}

struct http_conntrack* init_httpc(struct _skb *skb)
{
	struct http_conntrack *httpc;
	httpc=(struct http_conntrack*)new_page(sizeof(struct  http_conntrack));
	if(!httpc)
	{
		return NULL;
	}
	thread_lock();	
	httpc->last_time = get_current_sec();
	httpc->sip = skb->iph->saddr;
	httpc->dip = skb->iph->daddr;
	httpc->seq = skb->tcp->seq;
	httpc->ack_seq = skb->tcp->ack_seq;
	httpc->http_len = skb->http_len;
	httpc->is_handle = 0;
	strncpy(httpc->host , skb->hhdr.host ,COMM_MAX_LEN);
	la_list_add_tail(&(httpc->list), &(httpc_list));
	thread_unlock();
	return httpc;
}

int update_httpc(struct http_conntrack *httpc,
	struct _skb *skb)
{
	thread_lock();
	httpc->last_time = get_current_sec();
	httpc->seq = skb->tcp->seq;
	httpc->ack_seq = skb->tcp->ack_seq;
	httpc->http_len = skb->http_len;
	httpc->is_handle = 0;
	thread_unlock();
	return 0;
}

///////////////////////////////////////////////////////////////


int change_accept_encoding(struct _skb *skb)
{	
	char* tmp;
	tmp=strstr(skb->http_head, "Accept-Encoding: gzip");
	if(!tmp)
	{
		return -1;
	}
	
	tmp[0]='B';

	skb->tcp->check=tcp_chsum(skb->iph , skb->tcp , skb->tcp_len);
	return 0;
}

int filter(struct _skb* skb)
{
	if(strncmp(skb->hhdr.accept, "text/html" ,9)!=0)
	{
		return ERROR;
	}
	return OK;
}
int dispath(struct _skb* skb)
{
	struct http_conntrack* httpc;
	if(skb->http_len <=1 || !skb->http_head )
	{
		return -1;
	}
	
	switch (skb->hhdr.http_type)
	{
		case HTTP_TYPE_REQUEST_GET:
		{
			if(ERROR == filter(skb))
			{
				return -1;
			}
			if(ERROR == check_plug_hook(skb , CHECK_PLUG_PRE))
			{
				return -1;
			}
			httpc=find_http_conntrack_by_host(skb);
			if(!httpc)
			{
				httpc=init_httpc(skb);
				if(!httpc)
				{
					return -1;
				}
				
				return change_accept_encoding(skb);
			}
			
			update_httpc(httpc , skb);
			return change_accept_encoding(skb);
			
		}
		case HTTP_TYPE_REQUEST_POST:
		{
			return -1;
		}
		case HTTP_TYPE_RESPONSE:
		case HTTP_TYPE_OTHER:
		default:
		{
			httpc=find_http_conntrack_by_ack(skb);
			if(!httpc)
			{
				return -1;
			}
			if(httpc->is_handle==1)
			{
				return -1;
			}
			if(ERROR == check_plug_hook(skb , CHECK_PLUG_POST))
			{
				return -1;
			}
			else
			{
				httpc->is_handle=1;
			}
			return 0;			
		}
	}
	return 0;
}


int decode_http(struct _skb *skb)
{
	char **toks = NULL;
	int num_toks=0,tmp_num_toks=0;
	int i = 0;
	char **opts;
	int num_opts=0;
	char req_post[][16]={"5" ,"POST " };
	char req_get[][16]={"4" ,"GET " };
	char res[][16]={"7" ,"HTTP/1."};
	char http_head_end[][16]={"4" ,"\r\n\r\n"};
	
	skb->http_len=skb->ip_len-skb->iph_len-skb->tcph_len;
	if(skb->http_len<=0)
		return -1;
	
	skb->http_head=(char*)(skb->pload+skb->iph_len+skb->tcph_len);
	if(!skb->http_head)
		return -1;

	//////////////http_head_start///////////
	if(!memcmp(skb->http_head,req_post[1],atoi(req_post[0])))
	{
		skb->hhdr.http_type=HTTP_TYPE_REQUEST_POST;
	}
	else if(!memcmp(skb->http_head,req_get[1],atoi(req_get[0])))
	{
		skb->hhdr.http_type=HTTP_TYPE_REQUEST_GET;
	}
	else if(!memcmp(skb->http_head,res[1],atoi(res[0])))
	{
			
		skb->hhdr.http_type=HTTP_TYPE_RESPONSE;
	}
	else 
	{
		skb->hhdr.http_type=HTTP_TYPE_OTHER;
		return 0;
	}
	toks = mSplit(skb->http_head, "\r\n", MAX_PATTERN_NUM, &num_toks,'\\');

	tmp_num_toks=num_toks;
	num_toks--;
	while(num_toks)
	{ 
		if(i==0)
		{
			opts = mSplit(toks[i], " ", 3, &num_opts,'\\');
			while(isspace((int)*opts[0])) opts[0]++;
			if(skb->hhdr.http_type==HTTP_TYPE_RESPONSE)
			{
				strncpy(skb->hhdr.error_code, opts[1] ,COMM_MAX_LEN);
			}
			else if(skb->hhdr.http_type==HTTP_TYPE_REQUEST_GET||
				skb->hhdr.http_type==HTTP_TYPE_REQUEST_POST)
			{
				strncpy(skb->hhdr.uri , opts[1] , COMM_MAX_LEN);
			}
		}
		else
		{
			opts = mSplit(toks[i], ": ", 2, &num_opts,'\\');
			while(isspace((int)*opts[0])) opts[0]++;
			
			if(!strcasecmp(opts[0], "host"))
			{
				strncpy(skb->hhdr.host , opts[1] ,COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "accept-encoding"))
			{
				strncpy(skb->hhdr.accept_encoding , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "accept"))
			{
				strncpy(skb->hhdr.accept , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "accept-charset"))
			{
				strncpy(skb->hhdr.accept_charset , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "accept-language"))
			{
				strncpy(skb->hhdr.accept_language , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "authorization"))
			{
				strncpy(skb->hhdr.authorization , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "cache-control"))
			{
				strncpy(skb->hhdr.cache_control , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "connection"))
			{
				strncpy(skb->hhdr.connection , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "content-encoding"))
			{
				strncpy(skb->hhdr.content_encoding , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "content-language"))
			{
				strncpy(skb->hhdr.content_language , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "content-length"))
			{
				strncpy(skb->hhdr.content_length , opts[1],COMM_MAX_LEN);
				skb->hhdr.res_type=HTTP_RESPONSE_TYPE_CONTENTLENGTH;
			}
			else if(!strcasecmp(opts[0], "content-type"))
			{
				strncpy(skb->hhdr.content_type , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "content-range"))
			{
				strncpy(skb->hhdr.content_range , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "connection"))
			{
				strncpy(skb->hhdr.connection , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "user-agent"))
			{
				strncpy(skb->hhdr.user_agent , opts[1],COMM_MAX_LEN);
			}
			else if(!strcasecmp(opts[0], "transfer-encoding"))
			{
				strncpy(skb->hhdr.transfer_encoding , opts[1],COMM_MAX_LEN);
				if(!strcmp(skb->hhdr.transfer_encoding , "chunked"))
				{
					skb->hhdr.res_type=HTTP_RESPONSE_TYPE_CHUNKED;
					
				}
			}	
		}
		mSplitFree(&opts ,num_opts);
		--num_toks;
		i++;
	}
	mSplitFree(&toks ,tmp_num_toks);

	//////////////http_head_end///////////
	skb->http_data=strstr(skb->http_head, http_head_end[1]);
	if(!skb->http_data)
	{
		return 0;
	}
	
	skb->httph_len=strlen(skb->http_head)-strlen(skb->http_data);
	
	return 0;
}

int decode_tcp(struct _skb *skb)
{
	skb->tcp = (struct tcphdr *)(skb->pload+skb->iph_len);
	if(!skb->tcp)
		return -1;
	skb->tcph_len = 4 * skb->tcp->doff;
	skb->tcp_len = skb->ip_len-skb->iph_len;
	
	if(-1==decode_http(skb))
		return -1;

	return 0;

}

int decode_ip(struct _skb *skb)
{
	
	skb->iph = (struct iphdr *)(skb->pload);
	if(!skb->iph)
		return -1;
	if(skb->iph->ihl < 5 || skb->iph->version != 4)
		return -1;
	skb->ip_len = ntohs(skb->iph->tot_len);
	if(skb->ip_len != skb->pload_len)
		return -1;
	skb->iph_len=4 * skb->iph->ihl;
	if (skb->ip_len < skb->iph_len)
		return -1;
	
	
	if(skb->iph->protocol!=IPPROTO_TCP)
		return -1;
	
	if(-1==decode_tcp(skb))
		return -1;
	
	
	return 0;
}

int decode(struct _skb* skb)
{

	if(-1==decode_ip(skb))
	{
		return -1;
	}
	/*
	char sip[COMM_MAX_LEN],dip[COMM_MAX_LEN];
	ip2addr(sip , ntohl(skb->iph->saddr));
	ip2addr(dip , ntohl(skb->iph->daddr));
	debug_log("%s-->%s seq:%lu , ack_seq:%lu \n" , 
		sip ,dip,
		ntohl(skb->tcp->seq) , ntohl(skb->tcp->ack_seq));
	*/

	return 0;
}

static int queue_cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data)
{
    struct nfqnl_msg_packet_hdr *ph;
	struct _skb skb;

    //get unique ID of packet in queue
    ph = nfq_get_msg_packet_hdr(nfa);
    if(!ph)
    {
        return -1;
    }
	skb.pkt_id= ntohl(ph->packet_id);

    //get payload
    skb.pload_len = nfq_get_payload(nfa, &(skb.pload));
    if(skb.pload_len <=0)
    {
      	goto send;
    }

	skb.pload[skb.pload_len]='\0';
	if(-1==decode(&skb))
	{
		goto send;
	}

	if(-1==dispath(&skb))
	{
		goto send;
	}
	
	send:
	nfq_set_verdict(qh, skb.pkt_id, NF_ACCEPT, skb.pload_len, skb.pload);
	
    return 0;
    
}

int nfq()
{
	int len, fd;
    char buf[BUFSIZE]={0};
    struct nfq_handle *h;
    struct nfq_q_handle *qh;
	//call nfq_open() to open a NFQUEUE handler
    h = nfq_open();
    if(!h)
    {
        fprintf(stderr, "error during nfq_open()\n");
        exit(1);
    }

    //unbinging existing nf_queue handler for PE_INET(if any)
    if(nfq_unbind_pf(h, PF_INET) < 0)
    {
        fprintf(stderr, "error during nfq_unbind_pf()\n");
        exit(1);
    }

    //binding nfnetlink_queue as nf_queue handler for PF_INET
    if(nfq_bind_pf(h, PF_INET) < 0)
    {
        fprintf(stderr, "error during nfq_bind_pf()\n");
        exit(1);
    }

    //binding this socket to queue '0'
    qh = nfq_create_queue(h, 0, &queue_cb, NULL);
    if(!qh)
    {
        fprintf(stderr,"error during nfq_create_queue()\n");
        exit(1);
    }

    //setting copy_packet mode
    if(nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0)
    {
        fprintf(stderr, "can't set packet_copy_mode\n");
        exit(1);
    }

    //get the file descriptor associated with the nfqueue handler
    fd = nfq_fd(h);

    //handle a packet received from the nfqueue subsystem
    CONTINUE:
    while ((len = recv(fd, buf, BUFSIZE, 0)) && len >= 0)
    {
        nfq_handle_packet(h, buf, len);
    }
	debug_log("error len=%d" ,len);
	sleep(2);
	goto CONTINUE;
    nfq_destroy_queue(qh);
    nfq_close(h);
    return 0;
}
void start_work(struct task_info *ti)
{
    signal_ignore();
	INIT_LIST_HEAD(&httpc_list);
	init_plug();
	init_thpool(2);
	thpool_add_job(timeout , NULL);
	nfq();
}

int main(int argc, const char *argv[])
{
	//capure http protocol packets from kernel
	system("iptables -D INPUT -p tcp --sport 80 -j QUEUE");
	system("iptables -D OUTPUT -p tcp --dport 80 -j QUEUE");
	system("iptables -A INPUT -p tcp --sport 80 -j QUEUE");
	system("iptables -A OUTPUT -p tcp --dport 80 -j QUEUE");
	
	init_mpool(1*1024*1024);//256M
	new_task(1, 1, start_work);
	task_manage();
	return 0;
}

