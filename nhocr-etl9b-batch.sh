#!/bin/bash

if [ $# -eq 0 ] ; then
	echo "Usage: ./nhocr-etl9b-batch.sh <first-etl9b-file> [<second-etl9b-file> ...] "
	exit -1
fi

for FILE in "$@"
do
	BASENAME=$(basename $FILE)
	NAME=${BASENAME%.*}
	echo "Converting $FILE to $NAME.img..."
	./nhocr-etl9b $FILE $NAME cc-table.txt
done
