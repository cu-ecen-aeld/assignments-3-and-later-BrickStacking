#!/bin/sh
#Write in 11/6/2023 by Brick

if [ $# -eq 2 ]
then
	#echo "Right parameter with file dir $1, str: $2. Verify..."
    if [ -d $1 ]
    then
        #echo "Corect directory"
        numberFiles=$(find $1 -type f | wc -l)
        #echo "Total files: $numberFiles"
        numberLines=0
        numberLines=$(grep $2 -R $1 | wc -l)
        #echo "Total str find: $numberLines"
        echo "The number of files are $numberFiles and the number of matching lines are $numberLines"
    else
        echo "Not a directory"
        exit 1
    fi
else
    echo "Total parameter: $#, not valid input"
    echo "To run bash script correctly, run by guide below:"
    echo "./finder.sh <directory looking>  <string looking>"
    echo "Good luck ;)"
    exit 1 
fi