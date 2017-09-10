#!/bin/sh
if [ ! "$#" -eq 1 ]; then
	echo "./scripts [aa args data file]"
	return 0
fi
c=1
cat $1 | while read i; do
	data=`echo "$i" | cut -d '|' -f 1`
	ds=`echo "$i" | cut -d '|' -f 2`
	dc=`echo "$i" | cut -d '|' -f 3`
	dm=`echo "$data" | ../cli -i - -o - -c "$dc" -s $ds -dec_mouse`
 	du=`echo "$dm" | ../cli -i - -o - -unserialize`
	echo "$du" > $c.diff || break
 	echo "$du" | ../cli -i - -o - -undiff > $c || break
	line="`tail -1 $c`"
	c=$((c+1))
done
