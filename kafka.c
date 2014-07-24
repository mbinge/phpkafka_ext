/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_kafka.h"
#include "library.h"

/* If you declare any globals in php_kafka.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(kafka)
*/

/* True global resources - no need for thread safety here */
static int le_kafka;

/* {{{ kafka_functions[]
 *
 * Every user visible function must have an entry in kafka_functions[].
 */
const zend_function_entry kafka_functions[] = {
	PHP_FE(kafka_init,	NULL)
	PHP_FE(kafka_put,	NULL)
	PHP_FE(kafka_start_get,	NULL)
	PHP_FE(kafka_get,	NULL)
	PHP_FE(kafka_end_get,	NULL)
	PHP_FE(kafka_close,	NULL)
	PHP_FE_END	/* Must be the last line in kafka_functions[] */
};
/* }}} */

/* {{{ kafka_module_entry
 */
zend_module_entry kafka_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"kafka",
	kafka_functions,
	PHP_MINIT(kafka),
	PHP_MSHUTDOWN(kafka),
	PHP_RINIT(kafka),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(kafka),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(kafka),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1", /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_KAFKA
ZEND_GET_MODULE(kafka)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("kafka.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_kafka_globals, kafka_globals)
    STD_PHP_INI_ENTRY("kafka.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_kafka_globals, kafka_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_kafka_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_kafka_init_globals(zend_kafka_globals *kafka_globals)
{
	kafka_globals->global_value = 0;
	kafka_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
static int le_kafka;

#define le_kafka_name "kafka handle"

#define KAFKA_MODE_PRODUCER 1
#define KAFKA_MODE_CONSUMER 0
#define KAFKA_READ_HEAD INT_MIN
#define KAFKA_WAIT_TIMEOUT INT_MIN - 1

#define REGISTER_KAFKA_CONSTANT(__c) REGISTER_LONG_CONSTANT(#__c, __c, CONST_CS | CONST_PERSISTENT)

PHP_MINIT_FUNCTION(kafka)
{
	/* If you have INI entries, uncomment these lines 
	REGISTER_INI_ENTRIES();
	*/
	le_kafka = zend_register_list_destructors_ex(NULL, NULL, "kafka", module_number);
	REGISTER_KAFKA_CONSTANT(KAFKA_MODE_PRODUCER);
	REGISTER_KAFKA_CONSTANT(KAFKA_MODE_CONSUMER);
	REGISTER_KAFKA_CONSTANT(KAFKA_READ_HEAD);
	REGISTER_KAFKA_CONSTANT(KAFKA_WAIT_TIMEOUT);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(kafka)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(kafka)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(kafka)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(kafka)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "kafka support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

void logger(char *format,...)
{
	char tmp[512] = {0};
	va_list args;
	long ret;
	va_start(args, format);
	ret = vsprintf(tmp, format, args);
	php_error_docref(NULL TSRMLS_CC, E_WARNING, tmp);
	va_end(args);
}

long copy_data(char **dst, const char* src, long len)
{
	*dst = (char*)ecalloc(len+1, sizeof(char*));
	if(*dst == NULL)return -1;
	memcpy(*dst, src, len);	
	return len;
}


/* Remove the following function when you have succesfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_kafka_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(kafka_init)
{
	char *broker = NULL;
	long broker_len;
	long mode;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &broker, &broker_len, &mode) == FAILURE) {
		RETURN_NULL();
	}
	char m = 'P';
	if(mode == KAFKA_MODE_CONSUMER)m='C'; 
	long handler = _kafka_init(broker, m, logger);
	RETURN_LONG(handler);
}

PHP_FUNCTION(kafka_put)
{
	char *topic = NULL, *data = NULL;
	long topic_len, data_len;
	long handle;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lss", &handle, &topic, &topic_len, &data, &data_len) == FAILURE) {
		return;
	}
	RETURN_LONG(_kafka_put(handle, topic, data, data_len, logger));
}

PHP_FUNCTION(kafka_start_get)
{
	char *topic = NULL;
	long topic_len;
	long handler, offset, partition;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lsll", &handler, &topic, &topic_len, &partition, &offset) == FAILURE) {
		return;
	}
	char *data = NULL;
	long state = _kafka_start_get(handler, topic, partition, offset, logger);

	RETURN_LONG(state);
}

PHP_FUNCTION(kafka_get)
{
	long handler, partition , offset, newoffset=-1;
	char *data = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lll", &handler, &partition, &offset) == FAILURE) {
		return;
	}
	long msg_len = _kafka_get(handler, partition, &newoffset, &data, logger, copy_data);

	if(msg_len == KAFKA_READ_HEAD) {
		RETURN_LONG(KAFKA_READ_HEAD);	
	} else if(msg_len == KAFKA_WAIT_TIMEOUT) {
		RETURN_LONG(KAFKA_WAIT_TIMEOUT);
	} else if(msg_len < 0){
		RETURN_FALSE;
	} else {
		array_init(return_value);
		add_assoc_long(return_value, "offset", newoffset);
		add_assoc_stringl(return_value, "data", data, msg_len, 1);
	}
}

PHP_FUNCTION(kafka_end_get)
{
	long handler, partition;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &handler, &partition) == FAILURE) {
		return;
	}
	char *data = NULL;
	long ret =_kafka_end_get(handler, partition, logger);
	RETURN_LONG(ret);
}



PHP_FUNCTION(kafka_close)
{
	long handle;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &handle) == FAILURE) {
		return;
	}
	_kafka_close(handle, logger);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
