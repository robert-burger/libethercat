#!/bin/bash

DISTRIBUTOR_ID=$(lsb_release -i | awk '{ print tolower($3) }')
RELEASE=$(lsb_release -r | awk '{ print $2 }')
CODENAME=$(lsb_release -c | awk '{ print tolower($2) }')
KERNEL_VERS=$(uname -r)

#echo $DISTRIBUTOR_ID
#echo $RELEASE
#echo $CODENAME

if [ $CODENAME == 'n/a' ]; then
    echo $DISTRIBUTOR_ID/$RELEASE/$KERNEL_VERS
else
    echo $DISTRIBUTOR_ID/$CODENAME/$KERNEL_VERS
fi

