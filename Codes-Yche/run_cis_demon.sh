#!/bin/bash
function run_with_different_thread_count(){
  for thread_count in $( seq 1 32 )
  do
	      outputfile=$2"demon_"${thread_count}
        inputfile=$1
        echo $inputfile
        ./Daemon_Test $thread_count $inputfile $3> $outputfile
        outputfile=$2"cis_"${thread_count}
        ./CISTest $thread_count $inputfile $3> $outputfile
        let thread_count+=1
  done
}
