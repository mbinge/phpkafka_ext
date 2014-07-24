<?php
/***************************************************************************
 * 
 * Copyright (c) 2014 
 *  
 **************************************************************************/ 
 
 
 
/** 
 * @file recv.php 
 * @author champion(binge@live.com) 
 * @date 2014-07-02 21:59 
 **/ 

$host = '127.0.0.1:9092';
$start = microtime(true);
$k = kafka_init($host, KAFKA_MODE_CONSUMER);
$partition = 0;
$offset=0;
$ret = kafka_start_get($k, 'fds', $partition, $offset);
while(true) {
        $ret = kafka_get($k, $partition, $offset);
        $noffset = $ret['offset'];
        file_put_contents('recv.log', "$offset=>$noffset:".strlen($ret['data'])."\n", FILE_APPEND);
        if($ret == KAFKA_READ_HEAD)break;
        else if($ret === false)break;
        unset($ret);
        $offset=$noffset;
	break;
}
$ret = kafka_end_get($k, $partition);
kafka_close($k); 
 
 
/* vim: set ts=4 sw=4 fdm=2: */
