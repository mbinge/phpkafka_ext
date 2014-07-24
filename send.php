<?php
/***************************************************************************
 * 
 * Copyright (c) 2014 
 *  
 **************************************************************************/ 
 
 
 
/** 
 * @file send.php 
 * @author champion(binge@live.com) 
 * @date 2014-07-02 18:13 
 **/ 
 
$host = '127.0.0.1:9092';
$t=microtime(true);
$k = kafka_init($host, KAFKA_MODE_PRODUCER);
$f=substr($t-(int)$t,1);
$times=date("Ymd His", $t).$f;
$ret = kafka_put($k, 'fds', "from $host php $times");
file_put_contents('send.log', $times."\n", FILE_APPEND);
kafka_close($k);
echo "cost:".(microtime(true)-$t),"\n"; 
/* vim: set ts=4 sw=4 fdm=2: */
