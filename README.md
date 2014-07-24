phpkafka_ext
============

producer and consumer client for kafka 0.8+

STEP
----------------------------------- 
	1. modify config.m4 : /path/to/librdkafka
	2./path/to/phpize
	3../configure --with-kafka
	4.make && make install
	5./path/to/kafka/bin/kafka-topics.sh --create --zookeeper localhost:2181 --replication-factor 3 --partitions 1 --topic fds
	6./path/to/php send.php
	7./path/to/php recv.php
