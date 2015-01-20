/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Round Robin lbmethod EXAMPLE module for Apache proxy */

/* NOTE: This is designed simply to provide some info on how to create
         extra lbmethods via sub-modules... This code is ugly
         and needs work to actually do round-robin "right"
         but that is left as an exercise for the reader */

#include "mod_proxy.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "apr_version.h"
#include "ap_hooks.h"
#include "ap_slotmem.h"

#include "bloom.c"

#if APR_HAVE_UNISTD_H
#include <unistd.h> /* for getpid() */
#endif

module AP_MODULE_DECLARE_DATA lbmethod_roundrobin_module;

typedef struct {
    int index;
} rr_data ;

unsigned int sax_hash(const char *key){
	unsigned int h=0;

	while(*key) h^=(h<<5)+(h>>2)+(unsigned char)*key++;

	return h;
}

unsigned int sdbm_hash(const char *key) {
	unsigned int h=0;
	while(*key) h=(unsigned char)*key++ + (h<<6) + (h<<16) - h;
	return h;
}

int checkLastFromFiles(request_rec * r, const char * filter, const char * packet) {
	BLOOM * bloom;
	
	FILE * fp = fopen(filter, "r");
	
	size_t read;
	
	/* read the filter size */
	size_t size;
	read = fread(&size, sizeof(size_t), 1, fp);
	
	/* Re-create the filter */		
	if(!(bloom=bloom_create(size, 2, sax_hash, sdbm_hash)))
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(05011) "check last: bloom filter");

	/* Read the filter from file */			
	read = fread(bloom->a, sizeof(char), (bloom->asize+CHAR_BIT-1)/CHAR_BIT, fp);
	
	fclose(fp);
				
	fp = fopen(packet, "r");
	
	/* Read len of the packet */			
	int buflen;
	read = fread(&buflen, sizeof(int), 1, fp);
	
	/* Alocate memory for the buffer */		
	char * buffer;
	if((buffer = (char *) malloc(buflen * sizeof(char))) == NULL)
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(05011) "check last: buffer");

	/* Read the buffer */	
	read = fread(buffer, sizeof(char), buflen, fp);
			
	fclose(fp);
	
	
	/* Check and return if the request was received */
	if(bloom_check(bloom, buffer)){
		free(buffer);
		bloom_destroy(bloom);
		return 1;
	}
	else{
		free(buffer);
		bloom_destroy(bloom);
		return 0;
	}

}


void writePacket(const char * filename, char * buffer, int bufflen) {
	FILE *p = fopen(filename, "w");
	/* tamanho do buffer */
	fwrite(&bufflen, sizeof(int), 1, p);
	/* buffer */	
	fwrite(buffer, sizeof(char), bufflen, p);
	fflush(p);
	fclose(p);
}


static proxy_worker *find_best_roundrobin(proxy_balancer *balancer,
                                         request_rec *r)
{
    int i;
    proxy_worker **worker;
    proxy_worker *mycandidate = NULL;
    int checking_standby;
    int checked_standby;
    rr_data *ctx;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01116)
                 "proxy: Entering roundrobin for BALANCER %s (%d)",
                 balancer->s->name, (int)getpid());

	/* Example creating a file and writing using APR framework */

	apr_pool_t *tpool;
	apr_pool_create(&tpool, r->pool);

	ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(05001) "Before Checking Bloom");
	
	const char *packetp = ap_server_root_relative(tpool, "bloom/packet.txt");
	const char *filterp = ap_server_root_relative(tpool, "bloom/filter.txt");
	
	const char *debug = ap_server_root_relative(tpool, "bloom/debug.log");
	
	/*FILE * debugp = fopen(debug, "w+");

	int check = checkLastFromFiles(r, filterp, packetp);

	fprintf(debugp, "CHECK: %d\n", check);
	fflush(debugp);
	fclose(debugp);*/
	
	/* PRINT HEADERS TO PAGE */
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const apr_array_header_t    *fields;
    apr_table_entry_t           *e = 0;

    fields = apr_table_elts(r->headers_in);
    e = (apr_table_entry_t *) fields->elts;
    int len = 0;
    for(i = 0; i < fields->nelts; i++) {
        ap_rprintf(r, "<b>%s</b>: %s<br/>", e[i].key, e[i].val);
        len+= strlen(e[i].key)+strlen(e[i].val)+2;
    }
	len+=strlen(r->the_request);
	char *hdr = (char *) malloc(sizeof(char)*(len+1));
	strncpy(hdr, r->the_request, strlen(r->the_request));
	len = strlen(r->the_request);
	int ci;
	for(i = 0; i < fields->nelts; i++) 
		if(strncmp(e[i].key, "Connection", 10) != 0){
			strncpy(hdr+len, e[i].key, strlen(e[i].key));
			len += strlen(e[i].key);
			strncpy(hdr+len, ": ", 2);
			len+=2;
			strncpy(hdr+len, e[i].val, strlen(e[i].val));
			len += strlen(e[i].val);
		}
		else
			ci = i;
			
	i = ci;		
	strncpy(hdr+len, e[i].key, strlen(e[i].key));
	len += strlen(e[i].key);
	strncpy(hdr+len, ": ", 2);
	len+=2;
	strncpy(hdr+len, e[i].val, strlen(e[i].val));
	len += strlen(e[i].val);		
	
	hdr[len] = '\0';
	ap_rprintf(r, "</br></br>%d</br>%s</br></br></br>", len, hdr);
	
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	
	writePacket(packetp, hdr, len);

	ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(05002) "After Checking Bloom");

    /* The index of the candidate last chosen is stored in ctx->index */
    if (!balancer->context) {
        /* UGLY */
        ctx = apr_pcalloc(r->server->process->pconf, sizeof(rr_data));
        balancer->context = (void *)ctx;
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01117)
                 "proxy: Creating roundrobin ctx for BALANCER %s (%d)",
                 balancer->s->name, (int)getpid());
    } else {
        ctx = (rr_data *)balancer->context;
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, APLOGNO(01118)
                 "proxy: roundrobin index: %d (%d)",
                 ctx->index, (int)getpid());

    checking_standby = checked_standby = 0;
    while (!mycandidate && !checked_standby) {
        worker = (proxy_worker **)balancer->workers->elts;

        for (i = 0; i < balancer->workers->nelts; i++, worker++) {
            if (i < ctx->index)
                continue;
            if (
                (checking_standby ? !PROXY_WORKER_IS_STANDBY(*worker) : PROXY_WORKER_IS_STANDBY(*worker)) ||
                (PROXY_WORKER_IS_DRAINING(*worker))
                ) {
                continue;
            }
           // if (!PROXY_WORKER_IS_USABLE(*worker))
             //   ap_proxy_retry_worker("BALANCER", *worker, r->server);
            if (PROXY_WORKER_IS_USABLE(*worker)) {
                mycandidate = *worker;
                break;
            }
        }
        checked_standby = checking_standby++;
    }


    ctx->index += 1;
    if (ctx->index >= balancer->workers->nelts) {
        ctx->index = 0;
    }
    return mycandidate;
}

static apr_status_t reset(proxy_balancer *balancer, server_rec *s) {
        return APR_SUCCESS;
}

static apr_status_t age(proxy_balancer *balancer, server_rec *s) {
        return APR_SUCCESS;
}

static const proxy_balancer_method roundrobin =
{
    "roundrobin",
    &find_best_roundrobin,
    NULL,
    &reset,
    &age
};


static void ap_proxy_rr_register_hook(apr_pool_t *p)
{
    ap_register_provider(p, PROXY_LBMETHOD, "roundrobin", "0", &roundrobin);

}

AP_DECLARE_MODULE(lbmethod_roundrobin) = {
    STANDARD20_MODULE_STUFF,
    NULL,       /* create per-directory config structure */
    NULL,       /* merge per-directory config structures */
    NULL,       /* create per-server config structure */
    NULL,       /* merge per-server config structures */
    NULL,       /* command apr_table_t */
    ap_proxy_rr_register_hook /* register hooks */
};
