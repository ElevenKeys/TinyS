#include "tiny_config.h"

extern handler_module_t tiny_static_module;
extern handler_module_t tiny_proxy_module;
//extern handler_module_t tiny_cgi_module;
//extern handler_module_t  tiny_fastcgi_module;
//extern handler_module_t tiny_uwsgi_module;
handler_module_t *pass_handler_set[] = 
{
    &tiny_static_module,
	&tiny_proxy_module,
	//&tiny_cgi_module,
	//&tiny_fastcgi_module,
	//&tiny_uwsgi_module
};
