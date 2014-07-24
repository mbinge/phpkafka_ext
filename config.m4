dnl config.m4 for extension kafka

dnl PHP_ARG_ENABLE(kafka, Whether to enable the "kafka" extension,
dnl   [  --enable-kafka      Enable "kafka" extension support])


 PHP_ARG_WITH(kafka, for kafka support, 
 Make sure that the comment is aligned:
 [  --with-kafka             Include kafka support])

  PHP_ADD_INCLUDE("/path/to/librdkafka/include")

if test $PHP_KAFKA != "no"; then
  PHP_ADD_LIBRARY_WITH_PATH(rdkafka, /path/to/librdkafka/lib, KAFKA_SHARED_LIBADD)
  PHP_SUBST(KAFKA_SHARED_LIBADD)
  PHP_NEW_EXTENSION(kafka, kafka.c library.c, $ext_shared)
fi
