/* conf_api.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/* Part of the code in here was originally in conf.c, which is now removed */

#include <openssl/conf.h>
#include <openssl/conf_api.h>

static void value_free_hash(CONF_VALUE *a, LHASH *conf);
static void value_free_stack(CONF_VALUE *a,LHASH *conf);
static unsigned long hash(CONF_VALUE *v);
static int cmp_conf(CONF_VALUE *a,CONF_VALUE *b);

/* Up until OpenSSL 0.9.5a, this was get_section */
CONF_VALUE *_CONF_get_section(CONF *conf, char *section)
	{
	CONF_VALUE *v,vv;

	if ((conf == NULL) || (section == NULL)) return(NULL);
	vv.name=NULL;
	vv.section=section;
	v=(CONF_VALUE *)lh_retrieve(conf->data,&vv);
	return(v);
	}

/* Up until OpenSSL 0.9.5a, this was CONF_get_section */
STACK_OF(CONF_VALUE) *_CONF_get_section_values(CONF *conf, char *section)
	{
	CONF_VALUE *v;

	v=_CONF_get_section(conf,section);
	if (v != NULL)
		return((STACK_OF(CONF_VALUE) *)v->value);
	else
		return(NULL);
	}

int _CONF_add_string(CONF *conf, CONF_VALUE *section, CONF_VALUE *value)
	{
	CONF_VALUE *v = NULL;
	STACK_OF(CONF_VALUE) *ts;

	ts = (STACK_OF(CONF_VALUE) *)section->value;

	value->section=section->section;	
	if (!sk_CONF_VALUE_push(ts,value))
		{
		return 0;
		}

	v = (CONF_VALUE *)lh_insert(conf->data, value);
	if (v != NULL)
		{
		sk_CONF_VALUE_delete_ptr(ts,v);
		Free(v->name);
		Free(v->value);
		Free(v);
		}
	return 1;
	}

char *_CONF_get_string(CONF *conf, char *section, char *name)
	{
	CONF_VALUE *v,vv;
	char *p;

	if (name == NULL) return(NULL);
	if (conf != NULL)
		{
		if (section != NULL)
			{
			vv.name=name;
			vv.section=section;
			v=(CONF_VALUE *)lh_retrieve(conf->data,&vv);
			if (v != NULL) return(v->value);
			if (strcmp(section,"ENV") == 0)
				{
				p=Getenv(name);
				if (p != NULL) return(p);
				}
			}
		vv.section="default";
		vv.name=name;
		v=(CONF_VALUE *)lh_retrieve(conf->data,&vv);
		if (v != NULL)
			return(v->value);
		else
			return(NULL);
		}
	else
		return(Getenv(name));
	}

long _CONF_get_number(CONF *conf, char *section, char *name)
	{
	char *str;
	long ret=0;

	str=_CONF_get_string(conf,section,name);
	if (str == NULL) return(0);
	for (;;)
		{
		if (conf->meth->is_number(conf, *str))
			ret=ret*10+conf->meth->to_int(conf, *str);
		else
			return(ret);
		str++;
		}
	}

int _CONF_new_data(CONF *conf)
	{
	if (conf == NULL)
		{
		return 0;
		}
	if (conf->data == NULL)
		if ((conf->data = lh_new(hash,cmp_conf)) == NULL)
			{
			return 0;
			}
	return 1;
	}

void _CONF_free_data(CONF *conf)
	{
	if (conf == NULL || conf->data == NULL) return;

	conf->data->down_load=0; /* evil thing to make sure the 'Free()'
				  * works as expected */
	lh_doall_arg(conf->data,(void (*)())value_free_hash,conf->data);

	/* We now have only 'section' entries in the hash table.
	 * Due to problems with */

	lh_doall_arg(conf->data,(void (*)())value_free_stack,conf->data);
	lh_free(conf->data);
	}

static void value_free_hash(CONF_VALUE *a, LHASH *conf)
	{
	if (a->name != NULL)
		{
		a=(CONF_VALUE *)lh_delete(conf,a);
		}
	}

static void value_free_stack(CONF_VALUE *a, LHASH *conf)
	{
	CONF_VALUE *vv;
	STACK *sk;
	int i;

	if (a->name != NULL) return;

	sk=(STACK *)a->value;
	for (i=sk_num(sk)-1; i>=0; i--)
		{
		vv=(CONF_VALUE *)sk_value(sk,i);
		Free(vv->value);
		Free(vv->name);
		Free(vv);
		}
	if (sk != NULL) sk_free(sk);
	Free(a->section);
	Free(a);
	}

static unsigned long hash(CONF_VALUE *v)
	{
	return((lh_strhash(v->section)<<2)^lh_strhash(v->name));
	}

static int cmp_conf(CONF_VALUE *a, CONF_VALUE *b)
	{
	int i;

	if (a->section != b->section)
		{
		i=strcmp(a->section,b->section);
		if (i) return(i);
		}

	if ((a->name != NULL) && (b->name != NULL))
		{
		i=strcmp(a->name,b->name);
		return(i);
		}
	else if (a->name == b->name)
		return(0);
	else
		return((a->name == NULL)?-1:1);
	}

/* Up until OpenSSL 0.9.5a, this was new_section */
CONF_VALUE *_CONF_new_section(CONF *conf, char *section)
	{
	STACK *sk=NULL;
	int ok=0,i;
	CONF_VALUE *v=NULL,*vv;

	if ((sk=sk_new_null()) == NULL)
		goto err;
	if ((v=(CONF_VALUE *)Malloc(sizeof(CONF_VALUE))) == NULL)
		goto err;
	i=strlen(section)+1;
	if ((v->section=(char *)Malloc(i)) == NULL)
		goto err;

	memcpy(v->section,section,i);
	v->name=NULL;
	v->value=(char *)sk;
	
	vv=(CONF_VALUE *)lh_insert(conf->data,v);
	if (vv != NULL)
		{
#if !defined(NO_STDIO) && !defined(WIN16)
		fprintf(stderr,"internal fault\n");
#endif
		abort();
		}
	ok=1;
err:
	if (!ok)
		{
		if (sk != NULL) sk_free(sk);
		if (v != NULL) Free(v);
		v=NULL;
		}
	return(v);
	}

IMPLEMENT_STACK_OF(CONF_VALUE)
