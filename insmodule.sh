#!/bin/bash
KVERSION=`uname -r`
mkdir -p /lib/modules/${KVERSION}/extra
sudo cp gpiots.ko /lib/modules/${KVERSION}/extra/
sudo insmod /lib/modules/${KVERSION}/extra/gpiots.ko gpios=17,18,22,23
