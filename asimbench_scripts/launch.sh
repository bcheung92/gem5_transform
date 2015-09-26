#!/bin/bash

function launch_bmark {
    echo "**************************************************************"
    bmark_name=$1
    mkdir -p $bmark_name
    sed "s/BMARK/$bmark_name/g" "launch.scr.template" > $bmark_name/$bmark_name.scr
    echo "Launching test $bmark_name/$bmark_name.scr"
    condor_submit $bmark_name/$bmark_name.scr
    echo "Done Launching test $bmark_name/$bmark_name.scr Sleeping ..."
    sleep 5
    condor_q
    echo "**************************************************************"
}

# declare -a BmarkList=('360buy'); 
declare -a CoreBmarkList=('360buy' 'baidumap' 'bbench' 'kingsoftoffice' 'mxplayer' 'ttpod');
declare -a BmarkList=('adobe' 'frozenbubble' 'k9mail' 'netease' 'sinaweibo');

for i in ${CoreBmarkList[@]}; do
    launch_bmark $i
done
for i in ${BmarkList[@]}; do
    launch_bmark $i
done


