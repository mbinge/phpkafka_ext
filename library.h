#ifndef PHP_KAFKA_LIBRARY_H
#define	PHP_KAFKA_LIBRARY_H
typedef void (*logCallback)(char *format, ...);
typedef long (*copyCallback)(char *dst, const char *src, long len); 

long _kafka_init(char* brokers, char mode, logCallback logger);
long _kafka_put(long handle, char* topic, char* msg, size_t msg_len, logCallback logger);
long _kafka_start_get(long handler, char* topic, long partition, long offset, logCallback logger);
long _kafka_get(long handler, long partition, long *offset, char** data, logCallback logger, copyCallback copy);
long _kafka_end_get(long handler, long partition, logCallback logger);
void _kafka_close(long handle, logCallback logger);
#endif	/* PHP_KAFKA_LIBRARY_H */
