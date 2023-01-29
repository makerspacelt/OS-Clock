<?php

function flushBuffer($buffer, $csv)
{
    if (
        $buffer[0] === 'r'
        && $buffer[2] === 'd'
        && $buffer[4] === 'b'
        && $buffer[6] === 'p'
    ) {
//        $realTime = $buffer[1];
        fputcsv($csv, [
            hexdec(bin2hex(strrev($buffer[1]))),
//            bin2hex(strrev($buffer[1])),
//            hexdec(bin2hex(strrev($buffer[3]))),
//            bin2hex(strrev($buffer[3])),
            bin2hex($buffer[3][2]) . ':' .
            bin2hex($buffer[3][1]) . ':' .
            bin2hex($buffer[3][0]),
            hexdec(bin2hex(strrev($buffer[5]))),
//            bin2hex(strrev($buffer[5])),
            hexdec(bin2hex(strrev($buffer[7]))),
//            bin2hex(strrev($buffer[7]))
        ]);
    } else {
        echo 'Error ';
        var_dump($buffer);
    }
}

//r - real time; d - diff; b - battery level; p - rpm;
$file = fopen('data.txt', 'rb');
$csv = fopen('data.csv', 'wb');
$line = fgets($file);
$buffer = [];
$start = false;
$last = '';
while ($line !== false) {
//    $line = trim($line);
//    echo bin2hex($line) . ' ' . $line . PHP_EOL;
    $line = substr($line, 0, -2);
    if ($line === 'r') {
        $start = true;
    }
    if ($start) {
        $buffer[] = $line;
    }
    if ($last === 'p') {
        flushBuffer($buffer, $csv);
        $buffer = [];
        $start = false;
    }

    $last = $line;
    $line = fgets($file);
}


fclose($file);
fclose($csv);
